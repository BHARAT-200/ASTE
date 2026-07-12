// ASTE.h


#ifndef ASTE_H
#define ASTE_H
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<ctype.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
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

typedef struct erow{  // Contains a row of text
    int size;
    char * chars;
} erow;

struct editorConfig{
    int curx, cury;
    int rowoff;
    int screenrows;
    int screencols;
    int nrows;
    erow * row;
    struct termios orig_term;
};

extern struct editorConfig E;

struct abuf{  // buffer used to replace frequent write() calls
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