//
// Created by 86158 on 2024/7/7.
//

#include "highlight.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_separator(const int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(editor_row* row) {
    row->hl = realloc(row->hl, row->r_size); // NOLINT(*-suspicious-realloc-usage)
    memset(row->hl, HL_NORMAL, row->r_size);

    if (E.syntax == NULL) return;

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->single_line_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? (int) strlen(scs) : 0;
    int mcs_len = mcs ? (int) strlen(mcs) : 0;
    int mce_len = mce ? (int) strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->r_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_comment && !in_string) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->r_size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_ML_COMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_ML_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i ++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_ML_COMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
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

    const int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.num_rows) {
        editorUpdateSyntax(&E.row[row->idx + 1]);
    }
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_COMMENT:
        case HL_ML_COMMENT: return 36; // cyan
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

                for (int file_row = 0; file_row < E.num_rows; file_row ++ ) {
                    editorUpdateSyntax(&E.row[file_row]);
                }

                return;
            }
            i ++;
        }
    }
}