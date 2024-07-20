//
// Created by 86158 on 2024/7/7.
//

#ifndef KILO_C_HIGHLIGHT_H
#define KILO_C_HIGHLIGHT_H

#ifndef KILO_C_CONFIGS_H
#include "configs.h"
#endif

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORDS1,
    HL_KEYWORDS2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

extern void editorUpdateSyntax(editor_row*);

// maps values in hl to the actual ANSI color codes
extern int editorSyntaxToColor(int hl);

extern void editorSelectSyntaxHighlight();

#endif //KILO_C_HIGHLIGHT_H
