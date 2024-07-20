//
// Created by 86158 on 2024/7/7.
//

#include "highlight.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(editor_row* row) {
    row->hl = realloc(row->hl, row->r_size); // NOLINT(*-suspicious-realloc-usage)
    memset(row->hl, HL_NORMAL, row->r_size);

    if (E.syntax == NULL) return;

    char **keywords = E.syntax->keywords;

    char * scs = E.syntax->single_line_comment_start;
    int scs_len = scs ? (int)strlen(scs) : 0;

    int prev_sep = 1;
    int in_string = 0;

    int i = 0;
    while (i < row->r_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->r_size - i);
                break;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->r_size) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i ++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = (int)c;
                    row->hl[i] = HL_STRING;
                    i ++;
                    continue;
                }
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i ++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j ++ ) {
                int k_len = (int)strlen(keywords[j]);
                int kw2 = keywords[j][k_len - 1] == '|';
                if (kw2) {
                    k_len --;
                }

                if (!strncmp(&row->render[i], keywords[j], k_len) &&
                        is_separator(row->render[i + k_len])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORDS2 : HL_KEYWORDS1, k_len);
                    i += k_len;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_COMMENT: return 36; // cyan
        case HL_KEYWORDS1: return 33; // yellow
        case HL_KEYWORDS2: return 32; // green
        case HL_STRING: return 35; // purple
        case HL_NUMBER: return 31; // red
        case HL_MATCH: return 34; // blue
        case HL_NORMAL:
        default: return 37; // normal white
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.fileName == NULL) return;

    char *ext = strrchr(E.fileName, '.');

    for (unsigned int j = 0; j < (unsigned int)HLDB_ENTRIES; j ++ ) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->file_match[i]) {
            int is_ext = (s->file_match[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->file_match[i])) ||
                    (!is_ext && strstr(E.fileName, s->file_match[i]))) {
                E.syntax = s;

                int file_row;
                for (file_row = 0; file_row < E.num_rows; file_row ++ ) {
                    editorUpdateSyntax(&E.row[file_row]);
                }

                return;
            }
            i ++;
        }
    }
}