/*** Includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** Defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    /* Original copy of terminal attributes */
    struct termios orig_termios;
};

struct editorConfig E;

/*** Terminal ***/

void die(const char *s) {
    /* Clean up terminal (clear & reposition) */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    /* Print error and exit */
    perror(s);
    exit(1);
}

void disableRawMode() {
    /* Restore original terminal attributes on exit */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    /* Read in terminal attributes */
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    /* Register restore function */
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    /* Disable flow control, CRNL & misc legacy (BRKINT, INPCK, ISTRIP) */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Disable output processing */
    raw.c_oflag &= ~(OPOST);
    /* Set char size to 8 bits-per-byte */
    raw.c_cflag |= (CS8);
    /* Disable echo, SIGINT/SIGSTP & canonical mode */ 
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Set timeouts for read() to prevent input blocking - fails on Win */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    /* Set new terminal attributes - discarding unread input w/ TCSAFLUSH */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
    int nread;
    char c;
    /* Read and return a single key press from stdin */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 'w';
                case 'B': return 's';
                case 'C': return 'd';
                case 'D': return 'a';
            }
        }

        return '\x1b';
    } else {
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    /* Request cursor position */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    /* Parse response until 'R' received */
    while (i < sizeof(buf) - 1 ) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    /* Set final byte to null */
    buf[i] = '\0';

    /* Check for escape sequence in response */
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    /* Parse window size integers after escape sequence */
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    /* Get window size from ioctl, checking for 0 error */
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* Fallback if ioctl doesn't return values for some systems */
        /* Set cursor to bottom right corner and request cursor position */
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Append Buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    /* Get block of memory equivalent to existing string plus appended item */ 
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    /* Copy s to the end of new buffer */
    memcpy(&new[ab->len], s, len);
    /* Update pointers */
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    /* Free memory */
    free(ab->b);
}

/*** Output ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    /* Loop to draw row tildes */
    for (y = 0; y < E.screenrows; y++) {
        /* Print welcome message */
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "KILO editor -- version %s", KILO_VERSION);
            /* Truncate string if bigger than window width */
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            /* Centre the string - div screen width by 2, sub half of string's length */
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                /* First char should be a tilde */
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }

        /* VT100 Clear line to right of cursor */
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    /* Hide cursor to prevent flicker */
    abAppend(&ab, "\x1b[?25l", 6);
    /* VT100  Reset Cursor Position */
    abAppend(&ab, "\x1b[H", 3);
    /* Draw rows to buffer */
    editorDrawRows(&ab);

    /* Set cursor to E.cx, E.cy */
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    /* Reset cursor location and show/hide status */
    abAppend(&ab, "\x1b[?25h", 6);

    /* Write buffer to screen then free its memory */
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** Input ***/

void editorMoveCursor(char key) {
    switch (key) {
        case 'a':
            E.cx--;
            break;
        case 'd':
            E.cx++;
            break;
        case 'w':
            E.cy--;
            break;
        case 's':
            E.cy++;
            break;
    }
}

void editorProcessKeypress() {
    char c = editorReadKey();
    /* Handle key */
    switch (c) {
        case CTRL_KEY('q'):
            /* Clear screen, reset cursor */
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case 'w':
        case 's':
        case 'a':
        case 'd':
            editorMoveCursor(c);
            break; 
    }
}

/*** Init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;

    /* Get window size and store in global config */
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    /* Read byte(s) from stdin into c */
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
