//
// Created by 86158 on 2024/6/18.
//

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "configs.h"

int main() {
    enableRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
            die("read");
        }
        if (iscntrl(c)) {
            printf("%d\r\n\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') {
            break;
        }
    }

    return 0;
}