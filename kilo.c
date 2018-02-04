#include <unistd.h>

int main() {
    char c;
    /* Read byte(s) from stdin into c, until read() returns 0 (EOF) or q key is pressed */
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
