// ASTE.c

#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<ctype.h>
#include<errno.h>
#include<string.h>
#include<sys/ioctl.h>
#include"ASTE.h"

struct editorConfig E;

/*Init*/
void initEd() {
    E.curx = E.cury = 0;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1){ die("getWindowSize"); }
}

/*Terminal Helpers*/
void die(const char * s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term) == -1){ die("tcsetattr"); };
}

void enableRawMode(){
    if(tcgetattr(STDIN_FILENO, &E.orig_term) == -1){ die("tcgetattr"); };
    atexit(disableRawMode);

    struct termios term = E.orig_term;

    term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);  // disabling input processing and Ctrl+S/Ctrl+Q flow control
    term.c_oflag &= ~(OPOST);  // tuning off output processing
    term.c_cflag |= (CS8);  // setting character size to 8 bits(8-bit characters), it's prolly in default settings but not taking any risks
    term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  // tuning off echo, canonical mode, extra control chars and some signals(SIGINT and SIGSTP)

    term.c_cc[VMIN] = 0;    // don't wait for a minimum number of input bytes before read() can return.
    term.c_cc[VTIME] = 1;   // wait at most 100 ms for input before read() times out and returns, so that program isn't just stuch at read, but moves on.

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1){ die("tcsetattr"); };
}

int edReadKey(){
    ssize_t nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1  &&  errno != EAGAIN){ die("read"); }
    }

    if(c == '\x1b'){
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) != 1){ return '\x1b'; }
        if(read(STDIN_FILENO, &seq[1], 1) != 1){ return '\x1b'; }
        if(seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
        }
    }

    return '\x1b';
    }
    else{
        return c;
    }
}


int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4){ return -1; }
    while(i < sizeof(buf) - 1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if(buf[0] != '\x1b' || buf[1] != '['){ return -1; }
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2){ return -1; }
    return 0;
}

int getWindowSize(int * rows, int * columns){
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){ return -1; }
        return getCursorPosition(rows, columns);
    }
    else{
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

}

void abufAppend(struct abuf * ab, const char * s, int len){
    char * new = realloc(ab->b, ab->len + len);

    if(new == NULL){ return; }
    memcpy(&new[ab->len], s, len);
    ab->b = new; ab->len += len;
}

void abFree(struct abuf * ab){
    free(ab->b);
}

/*Input*/

void edMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      E.curx--;
      break;
    case ARROW_RIGHT:
      E.curx++;
      break;
    case ARROW_UP:
      E.cury--;
      break;
    case ARROW_DOWN:
      E.cury++;
      break;
  }
}

void edProcessKeypress(){
    int c = edReadKey();
    switch(c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        edMoveCursor(c);
        break;
    }
    
}

/*** output ***/
void edDrawRows(struct abuf * ab){
    for(int i = 0; i < E.screenrows; i++){
        if(i == E.screenrows / 3){
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), "ASTE - A Simple Text Editor");
        if(welcomelen > E.screencols){ welcomelen = E.screencols; }
        int padding = (E.screencols - welcomelen) / 2;
        if(padding){
            abufAppend(ab, "~", 1);
            padding--;
        }
        for (; padding > 0; abufAppend(ab, " ", 1), padding--);
    
        abufAppend(ab, welcome, welcomelen);
        }
        else{ abufAppend(ab, "~", 1); }

        abufAppend(ab, "\x1b[K", 3);
        if(i < E.screenrows - 1){
            abufAppend(ab, "\r\n", 2);

        }
    }
}

void edRefreshScreen(){
    struct abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l", 6);
    abufAppend(&ab, "\x1b[H", 3);

    edDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cury + 1, E.curx + 1);
    abufAppend(&ab, buf, strlen(buf));

    abufAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

int main(){
    enableRawMode();
    initEd();
    while(1){
        edRefreshScreen();
        edProcessKeypress();
    }
    return 0;
}