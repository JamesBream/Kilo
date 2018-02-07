#include <termios.h>
#include <unistd.h>

void enableRawMode() {
    struct termios raw;
    /* Read in terminal attributes */
    tcgetattr(STDIN_FILENO, &raw);
    /* Disable echo - bit flip */ 
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
