//
// Created by 86158 on 2024/6/19.
//

#include "editor.h"
#include "highlight.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

enum editorKey {
    BACKSPACE = 127,
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

static int editorRowRxToCx(editor_row* row, int rx) {
    int curr_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx ++ ) {
        if (row->chars[cx] == '\t') {
            curr_rx += (KILO_TAB_STOP - 1) - (curr_rx % KILO_TAB_STOP);
        }
        curr_rx ++;

        if (curr_rx > rx) {
            return cx;
        }
    }
    return cx;
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
            char *c = &E.row[fileRow].render[E.col_off];
            unsigned char *hl = &E.row[fileRow].hl[E.col_off];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j ++ ) {
                if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
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
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.fileName ? E.fileName : "[No Name]", E.num_rows,
                       E.dirty ? "(modified)" : "");
    int r_len = snprintf(r_status, sizeof(r_status), "%s | %d/%d",
                         E.syntax ? E.syntax->file_type : "no ft", E.cy + 1, E.num_rows);
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
    abAppend(ab, "\r\n", 2);
}

static void editorDrawMessageBar(struct append_buf * ab) {
    // clear the message
    abAppend(ab, "\x1b[K", 3);
    int msgLen = (int)strlen(E.statusMsg);
    if (msgLen > E.screen_cols) {
        msgLen = E.screen_cols;
    }
    if (msgLen && time(NULL) - E.statusMsg_time < 5) {
        abAppend(ab, E.statusMsg, msgLen);
    }
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
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_off) + 1,
            (E.rx - E.col_off) + 1);
    abAppend(&ab, buf, (int)strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

static char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t buffSize = 128;
    char * buf = malloc(buffSize);

    size_t buffLen = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buffLen !=0) {
                buf[--buffLen] = '\0';
            }
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) {
                callback(buf, c);
            }
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if ((int)buffLen != 0) {
                editorSetStatusMessage("");
                if (callback) {
                    callback(buf, c);
                }
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buffLen == buffSize - 1) {
                buffSize *= 2;
                buf = realloc(buf, buffSize); // NOLINT(*-suspicious-realloc-usage)
            }
            buf[buffLen ++ ] = (char)c;
            buf[buffLen] = '\0';
        }

        if (callback) {
            callback(buf, c);
        }
    }
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

    editorUpdateSyntax(row);
}

static void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.num_rows) {
        return;
    }

    E.row = realloc(E.row, sizeof(editor_row) * (E.num_rows + 1)); // NOLINT(*-suspicious-realloc-usage)
    memmove(&E.row[at + 1], &E.row[at], sizeof(editor_row) * (E.num_rows - at));

    E.row[at].size = (int)len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].r_size = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    editorUpdateRow(&E.row[at]);

    E.num_rows ++;
    E.dirty ++;
}

static void editorFreeRow(editor_row* row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

static void editorDelRow(int at) {
    if (at < 0 || at >= E.num_rows) {
        return;
    }
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(editor_row) * (E.num_rows - at - 1));
    E.num_rows --;
    E.dirty ++;
}

static void editorRowInsertChar(editor_row* row, int at, int c) {
    if (at < 0 || at > row->size)   at = row->size;
    row->chars = realloc(row->chars, row->size + 2); // NOLINT(*-suspicious-realloc-usage)
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size ++;
    row->chars[at] = (char)c;
    editorUpdateRow(row);
    E.dirty ++;
}

static void editorRowAppendString(editor_row * row, char * s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1); // NOLINT(*-suspicious-realloc-usage)
    memcpy(&row->chars[row->size], s, len);
    row->size += (int)len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty ++;
}

static void editorInsertChar(int c) {
    if (E.cy == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx ++;
}

static void editorInsertNewline() {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        editor_row * row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], E.row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy ++;
    E.cx = 0;
}

static void editorRowDelChar(editor_row* row, int at) {
    if (at < 0 || at >= row->size) {
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size --;
    editorUpdateRow(row);
    E.dirty ++;
}

static void editorDelChar() {
    if (E.cy == E.num_rows) {
        return;
    }
    if (E.cx == 0 && E.cy == 0) {
        return;
    }

    editor_row * row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx --;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy --;
    }
}

static char* editorRowsToString(int * buffLen) {
    int totalLen = 0;
    int j;
    for (j = 0; j < E.num_rows; j ++ ) {
        totalLen += E.row[j].size + 1;
    }
#pragma clang diagnostic push
#pragma ide diagnostic ignored "DanglingPointer"
    *buffLen = totalLen;
#pragma clang diagnostic pop

    char * buf = malloc(totalLen);
    char * p = buf;
    for (j = 0; j < E.num_rows; j ++ ) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p ++;
    }

    return buf;
}

/**
 * open the file.
 */
void editorOpen(char * fileName) {
    free(E.fileName);
    E.fileName = strdup(fileName);

    editorSelectSyntaxHighlight();

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
        editorInsertRow(E.num_rows, line, lineLen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

static void editorSave() {
    if (E.fileName == NULL) {
        E.fileName = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.fileName == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int len;
    char * buf = editorRowsToString(&len);

    int fd = open(E.fileName, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        // sets the file’s size to the specified length
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

//TODO: It's just that different rows are implemented, and the search for the same row is not implemented.
static void editorFindCallback(char * query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char* saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].r_size);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) {
        direction = 1;
    }
    int current = last_match;

    int i;
    for (i = 0; i < E.num_rows; i ++ ) {
        current += direction;
        if (current == -1) {
            current = E.num_rows - 1;
        }
        else if (current == E.num_rows) {
            current = 0;
        }

        editor_row * row = &E.row[current];
        char * match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, (int)(match - row->render));
            E.row_off = E.num_rows;

            saved_hl_line = current;
            saved_hl = malloc(row->r_size);
            memcpy(saved_hl, row->hl, row->r_size);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

/* find */
static void editorFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_col_off = E.col_off;
    int saved_row_off = E.row_off;

    char * query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

    if (query) {
        free(query);
    }
    else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.col_off = saved_col_off;
        E.row_off = saved_row_off;
    }
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

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusMsg, sizeof(E.statusMsg), fmt, ap);
    va_end(ap);
    E.statusMsg_time = time(NULL);
}

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.row_off = 0;
    E.col_off = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.fileName = NULL;
    E.statusMsg[0] = '\0';
    E.statusMsg_time = 0;
    E.syntax = NULL;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
    E.screen_rows -= 2;
}
//TODO: 中文'\t'有一点小错误

void editorProcessKeypress() {
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey();

    switch (c) {
        case '\r':
            editorInsertNewline();
            break;

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

        case CTRL_KEY('f'):
            editorFind();
            break;
            
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) {
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
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
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage("Warning!!! File has unsaved changes. "
                                       "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times --;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);

        case CTRL_KEY('s'):
            editorSave();
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;
            
        default:
            editorInsertChar(c);
            break;
    }

    quit_times = KILO_QUIT_TIMES;
}
