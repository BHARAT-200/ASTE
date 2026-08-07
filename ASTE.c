// ASTE.c

#include"ASTE.h"

struct editorConfig E;

/*Init*/

void initEd() {
    E.curx = E.cury = E.renx = E.rowoff = E.coloff = E.nrows = E.statusmsg_time = E.dirty = 0;
    E.row = NULL; E.filename = NULL;
    E.statusmsg[0] = '\0';
    if(getWindowSize(&E.screenrows, &E.screencols) == -1){ die("getWindowSize"); }
    E.screenrows -= 2;
}

/*Terminal Helpers*/

void die(const char * s){
    write(STDOUT_FILENO, "\x1b[2J", 4);  // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to top

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
    term.c_oflag &= ~(OPOST);  // tuening off output processing
    term.c_cflag |= (CS8);  // setting character size to 8 bits(8-bit characters), it's prolly in default settings but not taking any risks
    term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  // tuning off echo, canonical mode, extra control chars and some signals(SIGINT and SIGSTP)

    term.c_cc[VMIN] = 0;    // don't wait for a minimum number of input bytes before read() can return.
    term.c_cc[VTIME] = 1;   // wait at most 100 ms for input before read() times out and returns, so that program isn't just stuch at read, but moves on.

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1){ die("tcsetattr"); };
}

int edReadKey() {  // ready key press and handle escape sequences("ESC [ A" gets converted to ARROW_UP)
    ssize_t nread;
    char c;

    // Read one byte
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN){ die("read"); }
    }

    if(c != '\x1b'){ return c; }  // Normal key

    char seq[3];  // Escape sequence

    if(read(STDIN_FILENO, &seq[0], 1) != 1){ return '\x1b'; }
    if(read(STDIN_FILENO, &seq[1], 1) != 1){ return '\x1b'; }

    if (seq[0] == '[') {

        /* Escape sequences like ESC [ 5 ~ */
        if(seq[1] >= '0' && seq[1] <= '9'){
            if(read(STDIN_FILENO, &seq[2], 1) != 1){ return '\x1b'; }
            if(seq[2] == '~'){
                switch (seq[1]) {  // Creating multiple cases for Home and End because there are many different escape sequences that could be sent by these keys, depending on OS or terminal emulator.
                    case '1': return HOME_KEY;
                    case '3': return DEL_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                }
            }
        }

        /* Arrow keys: ESC [ A/B/C/D */
        else{
            switch(seq[1]){
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
    }
    else if(seq[0]  == 'O'){
        switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }
    }

    return '\x1b';
}

int getCursorPosition(int *rows, int *cols){  // Asks the terminal for the current cursor position
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

int getWindowSize(int * rows, int * columns){  // Returns the terminal size.
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){  // if ioctl fails, just move curser to the last position and getCurserPosition
            return -1;
        }
        return getCursorPosition(rows, columns);
    }
    else{
        *columns = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

}

void abufAppend(struct abuf * ab, const char * s, int len){  // Append text to the dynamic output buffer
    char * new = realloc(ab->b, ab->len + len);

    if(new == NULL){ return; }
    memcpy(&new[ab->len], s, len);
    ab->b = new; ab->len += len;
}

void abFree(struct abuf * ab){
    free(ab->b);
}

/* Row Operations */

int edRowCurxToRenx(erow *row, int cx) {  // Convert curx to renx(because on rendering tab converts to 8 spaces instead)
  int rx = 0;
  for(register int i=0; i<cx; i++){
    if(row->chars[i] == '\t'){
        rx += (TAB_STOP - 1)  -  (rx % TAB_STOP);
    }
    rx++;
  }
  return rx;
}

void edUpdateRow(erow * row){  // create renderer, replace TAB with spaces for user
    int tabs = 0;
    for(register int j = 0; j < row->size; j++){
        if(row->chars[j] == '\t'){ tabs++; }
    }
    free(row->render);
    row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for(register int i = 0; i< row->size; i++){
        if(row->chars[i] == '\t'){
            row->render[idx++] = ' ';
            while(idx % TAB_STOP != 0){ row->render[idx++] = ' '; }
        }
        else{
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void edInsertRow(int at, char * s, size_t len){  // handles memory when a new row is inserted at index "at"
    if(at < 0  ||  at > E.nrows){ return; }

    E.row = realloc(E.row, sizeof(erow) * (E.nrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.nrows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(sizeof(char) * (len + 1));
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;

    edUpdateRow(&E.row[at]);
    E.nrows++;
    E.dirty++;
}

void edFreeRow(erow * row){  // frees the memory owned by a single erow
    free(row->render);
    free(row->chars);
}

void edDelRow(int at){  // removes row "at" from E.row, shifting the rest up
    if(at < 0  ||  at >= E.nrows){ return; }
    edFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.nrows - at - 1));
    E.nrows--;
    E.dirty++;
}

void edRowInsertChar(erow *row, int at, int c){
  if(at < 0  ||  at > row->size) { at = row->size; }
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  edUpdateRow(row);
  E.dirty++;
}

void edRowAppendString(erow * row, char * s, size_t len){  // tacks "s" onto the end of row->chars
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  edUpdateRow(row);
  E.dirty++;
}

void edRowDelChar(erow * row, int at){
  if(at < 0  ||  at >= row->size){ return; }
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  edUpdateRow(row);
  E.dirty++;
}

/* Editor operations */

void edInsertChar(int c){
  if (E.cury == E.nrows){ edInsertRow(E.nrows, "", 0); }
  edRowInsertChar(&E.row[E.cury], E.curx, c);
  E.curx++;
}

void edDelChar(){  // backspaces the character to the left of the cursor
  if(E.cury == E.nrows){ return; }
  if(E.curx == 0  &&  E.cury == 0){ return; }

  erow *row = &E.row[E.cury];
  if(E.curx > 0){
    edRowDelChar(row, E.curx - 1);
    E.curx--;
  }
  else{
    E.curx = E.row[E.cury - 1].size;
    edRowAppendString(&E.row[E.cury - 1], row->chars, row->size);
    edDelRow(E.cury);
    E.cury--;
  }
}

void edInsertNewline(){  // handles the Enter key: splits the current row at the cursor
  if(E.curx == 0){
    edInsertRow(E.cury, "", 0);
  }
  else{
    erow *row = &E.row[E.cury];
    edInsertRow(E.cury + 1, &row->chars[E.curx], row->size - E.curx);
    row = &E.row[E.cury];
    row->size = E.curx;
    row->chars[row->size] = '\0';
    edUpdateRow(row);
  }
  E.cury++;
  E.curx = 0;
}

/* File IO */

void edOpen(char * filename){  // open a file and start adding rows to editorconfig
    free(E.filename);
    E.filename = strdup(filename);
    FILE * fp = fopen(filename, "r");
    if(!fp){ die("fopen"); }
    char * line = NULL;
    size_t linecap = 0;
    ssize_t llen;
    while((llen = getline(&line, &linecap, fp)) != -1){
        while(llen > 0  &&  (line[llen - 1] == '\n'  ||  line[llen - 1] == '\r')){  // remove newline
            llen--;
        }
        edInsertRow(E.nrows, line, llen);
    }
    free(line); fclose(fp);
    E.dirty = 0;
}

char * edRowsToString(int * bufferlen) {
  int tlen = 0; int i;
  for(i = 0; i < E.nrows; tlen += E.row[i].size + 1, i++);  // calculate how much memory we actually need(each row + newline char)
  *bufferlen = tlen;
  char * buf = malloc(tlen);
  char * p = buf;
  for(i = 0; i < E.nrows; i++){
    memcpy(p, E.row[i].chars, E.row[i].size);
    p += E.row[i].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void edSave(){
  if(E.filename == NULL){
    E.filename = edPrompt("Save as: %s (ESC to cancel)");
    if(E.filename == NULL){
      edSetStatusMessage("Save aborted");
      return;
    }
  }

  int len;
  char *buf = edRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);  // open for read & write. If file doesn't exist, create it

  if(fd != -1){
    if(ftruncate(fd, len) != -1){
        if(write(fd, buf, len) == len){  // if no error and write worked just fine, close and free
            close(fd); free(buf);
            E.dirty = 0;
            edSetStatusMessage("%d bytes written to disk", len);
            return;
        }
    }
    close(fd);
  }
  free(buf);
  edSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/* Input */

char * edPrompt(char * prompt){  // shows a prompt in the status bar and reads a line of input for it
    size_t bufsize = 128;
    char * buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    while(1){
        edSetStatusMessage(prompt, buf);
        edRefreshScreen();

        int c = edReadKey();
        if(c == DEL_KEY  ||  c == CTRL_KEY('h')  ||  c == BACKSPACE){
            if(buflen != 0){ buf[--buflen] = '\0'; }
        }
        else if(c == '\x1b'){
            edSetStatusMessage("");
            free(buf);
            return NULL;
        }
        else if(c == '\r'){
            if(buflen != 0){
                edSetStatusMessage("");
                return buf;
            }
        }
        else if(!iscntrl(c)  &&  c < 128){
            if(buflen == bufsize - 1){
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

void edMoveCursor(int key){  // handle cursor movement by changing E.curx and E.cury
    erow *row = (E.cury >= E.nrows) ? NULL : &E.row[E.cury];
    
    switch (key){
        case ARROW_LEFT:
            if(E.curx != 0){
                E.curx--;
            }
            else if(E.cury > 0){
                E.cury--;
                E.curx = E.row[E.cury].size;
            }
            break;
        case ARROW_RIGHT:
        if(row  &&  (E.curx < row->size)){
            E.curx++;
        }
        else if(row  &&  (E.curx == row->size)){
            E.cury++;
            E.curx = 0;
        }
            break;
        case ARROW_UP:
            if(E.cury != 0){
                E.cury--;
            }
            break;
        case ARROW_DOWN:
            if(E.cury < E.nrows){
                E.cury++;
            }
            break;
    }
        row = (E.cury >= E.nrows) ? NULL : &E.row[E.cury];
        int rowlen = row ? row->size : 0;
        if(E.curx > rowlen){
            E.curx = rowlen;
        }
}

void edProcessKeypress(){
    static int quit_times = QUIT_TIMES;
    int c = edReadKey();
    switch(c){
        case '\r':  // Enter key
            edInsertNewline();
            break;

        case CTRL_KEY('q'):
            if(E.dirty  &&  quit_times > 0){
                edSetStatusMessage("WARNING!!! File has unsaved changes. "
                    "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        
        case CTRL_KEY('s'):
            edSave();
            break;

        case HOME_KEY:
            E.curx = 0;
            break;
        case END_KEY:
            if(E.cury < E.nrows){
                E.curx = E.row[E.cury].size;
            }
            break;  

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY){ edMoveCursor(ARROW_RIGHT); }
            edDelChar();
            break;   

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c == PAGE_UP){
                    E.cury = E.rowoff;
                }
                else if(c == PAGE_DOWN){
                    E.cury = E.rowoff + E.screenrows - 1;
                    if(E.cury > E.nrows){ E.cury = E.nrows; }
                }

                int times = E.screenrows;
                while(times--){
                    edMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
        break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            edMoveCursor(c);
            break;
        
        case CTRL_KEY('l'):
        case '\x1b':
        break;

        default: 
            edInsertChar(c);
            break;
    }

    quit_times = QUIT_TIMES;
}

/* output */

void edScroll(){  // handles offsets when cursor is moved
    E.renx = 0;
    if(E.cury < E.nrows){
        E.renx = edRowCurxToRenx(&E.row[E.cury], E.curx);
    }

    if(E.cury < E.rowoff){
        E.rowoff = E.cury;
    }
    if(E.cury >= E.rowoff + E.screenrows){
        E.rowoff = E.cury - E.screenrows + 1;
    }
    if(E.renx < E.coloff){
        E.coloff = E.renx;
    }
    if(E.renx >= E.coloff + E.screencols){
        E.coloff = E.renx - E.screencols + 1;
    }
}

void edDrawRows(struct abuf * ab){
    for(register int i = 0; i < E.screenrows; i++){
        int filerow = i + E.rowoff;
        if(filerow >= E.nrows){
            if(E.nrows == 0  &&  i == E.screenrows / 3){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "ASTE - A Small Text Editor");
            if(welcomelen > E.screencols){ welcomelen = E.screencols; }
            int padding = (E.screencols - welcomelen) / 2;
            if(padding){
                abufAppend(ab, "~", 1);
                padding--;
            }
            for (; padding > 0; abufAppend(ab, " ", 1), padding--);
        
            abufAppend(ab, welcome, welcomelen);
            }
            else{
                abufAppend(ab, "~", 1);
            }
        }
        else{
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if(len > E.screencols) { len = E.screencols; }
            abufAppend(ab, &E.row[filerow].render[E.coloff], len);
        }

        abufAppend(ab, "\x1b[K", 3);
        abufAppend(ab, "\r\n", 2);
    }
}

void edDrawStatusBar(struct abuf *ab) {
    abufAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.nrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cury + 1, E.nrows);
    if (len > E.screencols) len = E.screencols;
    abufAppend(ab, status, len);
    while(len < E.screencols){
        if(E.screencols - len == rlen){
            abufAppend(ab, rstatus, rlen);
            break;
        }
        else{
            abufAppend(ab, " ", 1);
            len++;
        }
    }
    abufAppend(ab, "\x1b[m", 3);
    abufAppend(ab, "\r\n", 2);
}

void edRefreshScreen(){
    edScroll();
    struct abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l", 6);
    abufAppend(&ab, "\x1b[H", 3);

    edDrawRows(&ab);
    edDrawStatusBar(&ab);
    edDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cury-E.rowoff) + 1, (E.renx-E.coloff) + 1);
    abufAppend(&ab, buf, strlen(buf));

    abufAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void edSetStatusMessage(const char *fmt, ...) {  // Format and store a temporary status message with its timestamp
  va_list ap;  // ap will hold all the extra arguments
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);  // Store the current Unix timestamp
}

void edDrawMessageBar(struct abuf *ab) {
  abufAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if(msglen > E.screencols){
    msglen = E.screencols;
  }
  if(msglen  &&  (time(NULL) - E.statusmsg_time) < 5){
    abufAppend(ab, E.statusmsg, msglen);
  }
}

/* Main */

int main(int argc, char * argv[]){
    enableRawMode();
    initEd();
    if(argc >= 2){
        edOpen(argv[1]);
    }

    edSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

    while(1){
        edRefreshScreen();
        edProcessKeypress();
    }
    return 0;
}