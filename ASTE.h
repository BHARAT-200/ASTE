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

enum editorHighlight{
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

struct editorSyntax{
    char * filetype;
    char ** filematch;
    char ** keywords;
    char * singleline_comment_start;
    char * multiline_comment_start;
    char * multiline_comment_end;
    int flags;
};

typedef struct erow{  // Contains a row of text
    int idx;
    int size;
    int rsize;
    char * chars;
    char * render;
    unsigned char * hl;
    int hl_open_comment;
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
    struct editorSyntax * syntax;
    int brRow1, brCol1;  // chars-index position of one bracket in the pair currently matched near the cursor (-1 if none)
    int brRow2, brCol2;  // chars-index position of its matching bracket (-1 if the first bracket has no match)
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

/* Syntax Highlighting */
int edIsCFile(void);
int is_separator(int c);
void edUpdateSyntax(erow * row);
int edSyntaxToColor(int hl);
void edSelectSyntaxHighlight(void);

/* Row operations */
int edRowCurxToRenx(erow * row, int cx);
int edRowRenxToCurx(erow * row, int renx);
void edUpdateRow(erow * row);
void edInsertRow(int at, char * s, size_t len);
void edFreeRow(erow * row);
void edDelRow(int at);
void edRowInsertChar(erow *row, int at, int c);
void edRowAppendString(erow * row, char * s, size_t len);
void edRowDelChar(erow * row, int at);

/* Bracket Matching */
int edIsOpenBracket(char c);
int edIsCloseBracket(char c);
char edMatchingBracket(char c);
int edIsRealBracketAt(erow * row, int cx);
void edFindMatchingBracket(void);

/* Editor operations */
void edAutoOutdentOnCloseBracket(int c);
void edCheckAsciHelpCommand(void);
void edInsertChar(int c);
void edDelChar(void);
void edInsertNewline(void);

/* File I/O */
void edFreeAllRows(void);
void edLoadFileRows(FILE * fp);
void edOpen(char * filename);
char *edRowsToString(int * bufferlen);
void edSave(void);
void edOpenFile(void);

/* Find */
void edFindCallback(char * query, int key);
void edFind(void);

/* Go To Line */
void edGoToLine(void);

/* Help */
void edShowHelp(void);

/* Input */
char *edPrompt(char * prompt, void (*callback)(char *, int));
void edMoveCursor(int key);
void edProcessKeypress(void);

/* Output */
int edLineNumWidth(void);
void edDrawLineNumber(struct abuf * ab, int filerow, int numwidth);
void edScroll(void);
void edDrawRows(struct abuf * ab);
void edDrawStatusBar(struct abuf * ab);
void edRefreshScreen(void);
void edSetStatusMessage(const char * fmt, ...);
void edDrawMessageBar(struct abuf * ab);

#endif /* ASTE_H */