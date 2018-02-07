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
    /* Disable echo on copy of attributes struct - bit flip (╯°□°）╯︵ ┻━┻ */ 
    raw.c_lflag &= ~(ECHO);
    /* Set new terminal attributes */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    /* Read byte(s) from stdin into c, until read() returns 0 (EOF) or q key is pressed */
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
