#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<ctype.h>
#include<errno.h>
#include<sys/ioctl.h>
#include"ASTE.h"

struct editorConfig E;

/*Init*/
void initEd() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
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

char edReadKey(){
    ssize_t nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1  &&  errno != EAGAIN){ die("read"); }
    }
    return c;
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

/*Input*/
void edProcessKeypress(){
    char c = edReadKey();
    switch(c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
    
}

/*** output ***/
void edDrawRows(){
    for(int i = 0; i < E.screenrows; i++){
        write(STDOUT_FILENO, "~", 1);

        if(i < E.screenrows - 1){
            write(STDOUT_FILENO, "\r\n", 2);

        }
    }
}

void edRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    edDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
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