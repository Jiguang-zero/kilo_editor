//
// Created by 86158 on 2024/6/18.
//

#include "configs.h"
#include <termio.h>
#include <unistd.h>
#include <stdlib.h>

struct termios orig_termios;

/**
 * disable raw mode when exit the program.
 */
static void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
