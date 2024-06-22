//
// Created by 86158 on 2024/6/18.
//

#ifndef KILO_C_CONFIGS_H
#define KILO_C_CONFIGS_H

// 'a' 97  1100001, the last five bits
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

typedef struct editor_row {
    int size;
    int r_size;
    char *chars;
    char *render;
} editor_row;

struct editorConfig {
    int cx, cy;
    int rx;
    int row_off;
    int col_off;
    int screen_rows;
    int screen_cols;
    int num_rows;
    editor_row* row;
    char * fileName;
    struct termios orig_termios;
};

extern struct editorConfig E;

// print error message when exit the program.
extern void die(const char *s);

// enable raw Mode.
extern void enableRawMode();

#endif //KILO_C_CONFIGS_H
