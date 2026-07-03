#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include <ctype.h>
#include <errno.h>

struct termios orig_term;

void die(const char * s){
    perror(s);
    exit(1);
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1){ die("tcsetattr"); };
}

// To enable raw mode(non canonical mode) allowing direct keyboard access
void enableRawMode(){
    if(tcgetattr(STDIN_FILENO, &orig_term) == -1){ die("tcgetattr"); };
    atexit(disableRawMode);

    struct termios term = orig_term;

    term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);  // disabling input processing and Ctrl+S/Ctrl+Q flow control
    term.c_oflag &= ~(OPOST);  // tuning off output processing
    term.c_cflag |= (CS8);  // setting character size to 8 bits(8-bit characters), it's prolly in default settings but not taking any risks
    term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  // tuning off echo, canonical mode, extra control chars and some signals(SIGINT and SIGSTP)

    term.c_cc[VMIN] = 0;    // don't wait for a minimum number of input bytes before read() can return.
    term.c_cc[VTIME] = 1;   // wait at most 100 ms for input before read() times out and returns, so that program isn't just stuch at read, but moves on.

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1){ die("tcsetattr"); };
}



int main(){
    enableRawMode();
    while(1){
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if(iscntrl(c)){
            printf("ASCII code of control character = %d \r\n", c);
        }
        else{
            printf("ASCII code of ('%c') = %d \r\n", c, c);
        }
        if(c == 'q'){
            break;
        }
    }
    return 0;
}