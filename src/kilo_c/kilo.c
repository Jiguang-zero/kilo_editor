//
// Created by 86158 on 2024/6/18.
//

#include "editor.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
#pragma clang diagnostic pop