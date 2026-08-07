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
#include<time.h>
#include<stdarg.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define TAB_STOP 8
#define QUIT_TIMES 3

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  BACKSPACE = 127
};

typedef struct erow{  // Contains a row of text
    int size;
    int rsize;
    char * chars;
    char * render;
} erow;

struct editorConfig{
    int curx, cury;
    int renx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int nrows;
    erow * row;
    int dirty;
    char * filename;
    char statusmsg[80];
    time_t statusmsg_time;
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
void die(const char * s);
void enableRawMode(void);
void disableRawMode(void);

int edReadKey(void);
int getCursorPosition(int * rows, int * cols);
int getWindowSize(int * rows, int * cols);

void abufAppend(struct abuf * ab, const char * s, int len);
void abFree(struct abuf * ab);

/* Row operations */
int edRowCurxToRenx(erow * row, int cx);
void edUpdateRow(erow * row);
void edInsertRow(int at, char * s, size_t len);
void edFreeRow(erow * row);
void edDelRow(int at);
void edRowInsertChar(erow *row, int at, int c);
void edRowAppendString(erow * row, char * s, size_t len);
void edRowDelChar(erow * row, int at);

/* Editor operations */
void edInsertChar(int c);
void edDelChar(void);
void edInsertNewline(void);

/* File I/O */
void edOpen(char * filename);
char *edRowsToString(int * bufferlen);
void edSave(void);

/* Input */
char *edPrompt(char * prompt);
void edMoveCursor(int key);
void edProcessKeypress(void);

/* Output */
void edScroll(void);
void edDrawRows(struct abuf * ab);
void edDrawStatusBar(struct abuf * ab);
void edRefreshScreen(void);
void edSetStatusMessage(const char * fmt, ...);
void edDrawMessageBar(struct abuf * ab);

#endif /* ASTE_H */