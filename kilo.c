/*** Includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** Defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** Data ***/

/* Original copy of terminal attributes */
struct termios orig_termios;

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    /* Read in terminal attributes */
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetaddr");
    /* Register restore function */
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    /* Disable flow control, CRNL & misc legacy (BRKINT, INPCK, ISTRIP) */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Disable output processing */
    raw.c_oflag &= ~(OPOST);
    /* Set char size to 8 bits-per-byte */
    raw.c_cflag &= ~(CS8);
    /* Disable echo, SIGINT/SIGSTP & canonical mode */ 
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Set timeouts for read() to prevent input blocking - fails on Win */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    /* Set new terminal attributes - discarding unread input w/ TCSAFLUSH */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetaddr");
}

char editorReadKey() {
    int nread;
    char c;
    /* Read and return a single key press from stdin */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

/*** Output ***/

void editorDrawRows() {
    int y;
    /* Loop to draw row tildes */
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {
    /* Use VT100 Erase in Display (2J) to clear screen */
    write(STDOUT_FILENO, "\x1b[2J", 4);
    /* VT100 Cursor Position */
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** Input ***/

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
    }
}

/*** Init ***/

int main() {
    enableRawMode();

    /* Read byte(s) from stdin into c */
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
