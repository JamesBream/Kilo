#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* Original copy of terminal attributes */
struct termios orig_termios;

void disableRawMode() {
    /* Restore original terminal attributes on exit */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    /* Read in terminal attributes */
    tcgetattr(STDIN_FILENO, &orig_termios);
    /* Register restore function */
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    /* Disable flow control and CRNL */
    raw.c_iflag &= ~(ICRNL | IXON);
    /* Disable output processing */
    raw.c_oflag &= ~(OPOST);
    /* Disable echo, SIGINT/SIGSTP & canonical mode */ 
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Set new terminal attributes - discarding unread input w/ TCSAFLUSH */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    /* Read byte(s) from stdin into c, until read() returns 0 (EOF) or q key is pressed */
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if(iscntrl(c)) {
            /* Print only ASCII code of control chars */
            printf("%d\r\n", c);
        } else {
            /* Print ASCII code and character */
            printf("%d ('%c')\r\n", c, c);
        }
    }

    return 0;
}
