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
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();
    
    /* Read byte(s) from stdin into c */
    while (1) {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if(iscntrl(c)) {
            /* Print only ASCII code of control chars */
            printf("%d\r\n", c);
        } else {
            /* Print ASCII code and character */
            printf("%d ('%c')\r\n", c, c);
        }
        /* Quit on q */
        if (c == 'q') break;
    }

    return 0;
}
