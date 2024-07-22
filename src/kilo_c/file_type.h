//
// Created by 86158 on 2024/7/19.
//

#ifndef KILO_C_FILE_TYPE_H
#define KILO_C_FILE_TYPE_H

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

#include <stdlib.h>

struct editorSyntax {
    char *file_type;
    char **file_match;
    char **keywords;
    char *single_line_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

static char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
static char *C_HL_keywords[] = {
        "switch", "if", "while", "for", "break", "continue", "return",
        "else", "struct", "union", "typedef", "static", "enum", "class",
        "case", "int|", "long|", "double|", "float|", "char|", "unsigned|",
        "signed|", "void|", NULL
};

// highlight database
static struct editorSyntax HLDB[] = {
        {
                "c",
                C_HL_extensions,
                C_HL_keywords,
                "//", "/*", "*/",
                HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
        },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

#endif //KILO_C_FILE_TYPE_H
