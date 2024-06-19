//
// Created by 86158 on 2024/6/18.
//

#include "configs.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct editorConfig E;

/**
 * disable raw mode when exit the program.
 */
static void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void die(const char * s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;

    // input flag: IXON: CTRL + C/Q;
    // ICRNL: CR, carriage return, NL, new line
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    // local flag: ICANON: Non-canonical input
    // IEXTEN: CTRL + V
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    // output flag: POST: Output Processing, "\n" -> "\r\n"
    raw.c_oflag &= ~(OPOST);
    // control flag
    raw.c_cflag |= (CS8);

    raw.c_cc[VMIN] = 0; // Minimum Number of Characters
    raw.c_cc[VTIME] = 1; // Timeout in Deciseconds, 1/10 s

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}
