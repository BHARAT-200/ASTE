#ifndef ASTE_H
#define ASTE_H
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)


struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_term;
};

extern struct editorConfig E;

/* Init */

void initEd(void);

/* Terminal helpers */

void die(const char *s);
void enableRawMode(void);
void disableRawMode(void);
char edReadKey(void);

int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);

/* Input */
void edProcessKeypress(void);

/* Output */

void edDrawRows(void);
void edRefreshScreen(void);

#endif /* ASTE_H */