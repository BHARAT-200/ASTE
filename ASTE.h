// ASTE.h

#ifndef ASTE_H
#define ASTE_H
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003
};

struct editorConfig{
    int curx, cury;
    int screenrows;
    int screencols;
    struct termios orig_term;
};

extern struct editorConfig E;

struct abuf{
    char * b;
    int len;
};

#define ABUF_INIT {NULL, 0}

/* Init */

void initEd(void);

/* Terminal helpers */

void die(const char *s);
void enableRawMode(void);
void disableRawMode(void);
int edReadKey(void);

int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);

/* Input */
void edProcessKeypress(void);

/* Output */

void edDrawRows(struct abuf *);
void edRefreshScreen(void);

#endif /* ASTE_H */