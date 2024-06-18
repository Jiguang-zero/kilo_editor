//
// Created by 86158 on 2024/6/18.
//

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "configs.h"

int main() {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) {
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\n", c, c);
        }
    }

    return 0;
}