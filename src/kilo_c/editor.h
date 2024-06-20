//
// Created by 86158 on 2024/6/19.
//

#ifndef KILO_C_EDITOR_H
#define KILO_C_EDITOR_H

#include "configs.h"

#include <stdio.h>
#include <unistd.h>

// ABUF
struct append_buf {
    char* b;
    int len;
};

#ifndef ABUF_INIT
#define ABUF_INIT {NULL, 0};
#endif

extern void editorProcessKeypress();

extern void editorRefreshScreen();

extern void initEditor();

/**
 * editor open the file.
 * @param fileName char*
 */
extern void editorOpen(char * fileName);

#endif //KILO_C_EDITOR_H
