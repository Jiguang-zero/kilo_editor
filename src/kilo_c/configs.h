//
// Created by 86158 on 2024/6/18.
//

#ifndef KILO_C_CONFIGS_H
#define KILO_C_CONFIGS_H

// 'a' 97  1100001, the last five bits
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)

#define KILO_VERSION "0.0.1"

typedef struct editor_row {
    int size;
    char *chars;
} editor_row;

struct editorConfig {
    int cx, cy;
    int screen_rows;
    int screen_cols;
    int num_rows;
    editor_row* row;
    struct termios orig_termios;
};

extern struct editorConfig E;

// print error message when exit the program.
extern void die(const char *s);

// enable raw Mode.
extern void enableRawMode();

#endif //KILO_C_CONFIGS_H
