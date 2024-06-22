//
// Created by 86158 on 2024/6/19.
//

#include "editor.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>


enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

static void abAppend(struct append_buf* ab, const char *s, int len) {
    char * new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

static void abFree(struct append_buf* ab) {
    free(ab->b);
}

static int editorReadKey() {
    int n_read;
    char c;
    while ((n_read = (int)read(STDIN_FILENO, &c, 1)) != 1) {
        if (n_read == -1 && errno != EAGAIN) {
            die("read");
        }
    }

    if (c == '\x1b') {
        char seq[3] ;

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        // arrow
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == '0') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    }

    else {
        return c;
    }
}

static int editorRowCxToRx(editor_row* row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j ++ ) {
        if (row->chars[j] == '\t') {
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx ++;
    }
    return rx;
}

static void editorScroll() {
    E.rx = 0;
    if (E.cy < E.num_rows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.row_off) {
        E.row_off = E.cy;
    }
    if (E.cy >= E.row_off + E.screen_rows) {
        E.row_off = E.cy - E.screen_rows + 1;
    }
    if (E.rx < E.col_off) {
        E.col_off = E.rx;
    }
    if (E.rx >= E.col_off + E.screen_cols) {
        E.col_off = E.rx - E.screen_cols + 1;
    }
}

static void editorDrawRows(struct append_buf * ab) {
    int y;
    for (y = 0; y < E.screen_rows; y ++ ) {
        int fileRow = y + E.row_off;
        if (fileRow >= E.num_rows) {
            if (E.num_rows == 0 && y == E.screen_rows / 3) {
                char welcome[80];
                int welcome_len = snprintf(welcome, sizeof(welcome),
                                           "Kilo editor -- version %s", KILO_VERSION);
                if (welcome_len > E.screen_cols) {
                    welcome_len = E.screen_cols;
                }
                int padding = (E.screen_cols - welcome_len) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcome_len);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[fileRow].r_size - E.col_off;
            if (len < 0) {
                len = 0;
            }
            if (len > E.screen_cols) {
                len = E.screen_cols;
            }
            abAppend(ab, &E.row[fileRow].render[E.col_off], len);
        }


        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

static void editorDrawStatusBar(struct append_buf * ab) {
    // color: white
    abAppend(ab, "\x1b[7m", 4);

    char status[80], r_status[80];
    // %.20s means that 20 size is the longest length.
    int len = snprintf(status, sizeof(status), "%.20s - %d lines",
                       E.fileName ? E.fileName : "[No Name]", E.num_rows);
    int r_len = snprintf(r_status, sizeof(r_status), "%d/%d", E.cy + 1, E.num_rows);
    if (len > E.screen_cols) {
        len = E.screen_cols;
    }
    abAppend(ab, status, len);

    while (len < E.screen_cols) {
        if (E.screen_cols - len == r_len) {
            abAppend(ab, r_status, r_len);
            break;
        } else {
            abAppend(ab, " ", 1);
            len ++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen() {
    editorScroll();

    struct append_buf ab = ABUF_INIT;

    // hide the cursor
    abAppend(&ab, "\x1b[?25l", 6);

    // \x1b: <ESC>;   4, "\x1b[2J" occupies 4 bytes
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_off) + 1,
            (E.rx - E.col_off) + 1);
    abAppend(&ab, buf, (int)strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

static void editorMoveCursor(int key) {
    editor_row * row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy --;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx ++;
            } else if (row && E.cx == row->size) {
                E.cy ++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.num_rows) {
                E.cy++;
            }
            break;
        default:
            break;
    }

    row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
    int rowLen = row ? row->size : 0;
    if (E.cx > rowLen) {
        E.cx = rowLen;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            editorMoveCursor(c);
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.num_rows)
                E.cx = E.row[E.cy].size;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            if (c == PAGE_UP) {
                E.cy = E.row_off;
            } else {
                E.cy = E.row_off + E.screen_rows - 1;
                if (E.cy > E.num_rows) {
                    E.cy = E.num_rows;
                }
            }
            int times = E.screen_rows;
            while (times -- ) {
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
            break;

        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
        default:
            break;
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
static int getCursorPosition(int * rows, int * cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R')  break;
        i ++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1; // NOLINT(*-err34-c)

    return -1;
}
#pragma clang diagnostic pop

static void editorUpdateRow(editor_row* row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j ++ ) {
        if (row->chars[j] == '\t') {
            tabs ++;
        }
    }
    free(row->render);
    row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j ++ ) {
        if (row->chars[j] == '\t') {
            row->render[idx ++ ] = ' ';
            while (idx % KILO_TAB_STOP != 0) {
                row->render[idx ++ ] = ' ';
            }
        } else {
            row->render[idx ++ ] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->r_size = idx;
}

static void editorAppendRow(char* s, size_t len) {
    E.row = realloc(E.row, sizeof(editor_row) * (E.num_rows + 1)); // NOLINT(*-suspicious-realloc-usage)

    int at = E.num_rows;
    E.row[at].size = (int)len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].r_size = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.num_rows ++;
}

/**
 * open the file.
 */
void editorOpen(char * fileName) {
    free(E.fileName);
    E.fileName = strdup(fileName);
    FILE *fp = fopen(fileName, "r");
    if (!fp) {
        die("open");
    }

    char * line = NULL;
    size_t linecap = 0;
    ssize_t lineLen;

    while ((lineLen = getline(&line, &linecap, fp)) != -1) {
        while (lineLen > 0 && (line[lineLen - 1] == '\r' || line[lineLen - 1] == '\n')) {
            lineLen--;
        }
        editorAppendRow(line, lineLen);
    }
    free(line);
    fclose(fp);
}


static int getWindowSize(int * rows, int * cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.row_off = 0;
    E.col_off = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.fileName = NULL;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
    E.screen_rows -= 1;
}
//TODO: 中文'\t'有一点小错误