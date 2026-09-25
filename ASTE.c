// ASTE.c

#include"ASTE.h"

struct editorConfig E;

/* Filetypes */

char * C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };

char * C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*Init*/

void initEd() {
    E.curx = E.cury = E.renx = E.rowoff = E.coloff = E.nrows = E.statusmsg_time = E.dirty = 0;
    E.row = NULL; E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.syntax = NULL;
    E.brRow1 = E.brCol1 = E.brRow2 = E.brCol2 = -1;
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

/* Syntax Highlighting */

int edIsCFile(){  // true when the current file's syntax is C/C++, per E.syntax
  return E.syntax != NULL  &&  !strcmp(E.syntax->filetype, "c");
}

int is_separator(int c){
  return isspace(c)  ||  c == '\0'  ||  strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void edUpdateSyntax(erow * row){
  row->hl = realloc(row->hl, row->rsize);
  if(row->rsize > 0){ memset(row->hl, HL_NORMAL, row->rsize); }  // realloc(...,0) may return NULL; nothing to fill anyway

  if(E.syntax == NULL){ return; }

  char ** keywords = E.syntax->keywords;

  char * scs = E.syntax->singleline_comment_start;
  char * mcs = E.syntax->multiline_comment_start;
  char * mce = E.syntax->multiline_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0  &&  E.row[row->idx - 1].hl_open_comment);

  int i = 0;
  while(i < row->rsize){
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if(scs_len  &&  !in_string  &&  !in_comment){
      if(!strncmp(&row->render[i], scs, scs_len)){
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if(mcs_len  &&  mce_len  &&  !in_string){
      if(in_comment){
        row->hl[i] = HL_MLCOMMENT;
        if(!strncmp(&row->render[i], mce, mce_len)){
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        }
        else{
          i++;
          continue;
        }
      }
      else if(!strncmp(&row->render[i], mcs, mcs_len)){
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }
    }

    if(E.syntax->flags & HL_HIGHLIGHT_STRINGS){
      if(in_string){
        row->hl[i] = HL_STRING;
        if(c == '\\'  &&  i + 1 < row->rsize){
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if(c == in_string){ in_string = 0; }
        i++;
        prev_sep = 1;
        continue;
      }
      else{
        if(c == '"'  ||  c == '\''){
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
      if((isdigit(c)  &&  (prev_sep || prev_hl == HL_NUMBER))  ||
         (c == '.'  &&  prev_hl == HL_NUMBER)){
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if(prev_sep){
      int j;
      for(j = 0; keywords[j]; j++){
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if(kw2){ klen--; }

        if(!strncmp(&row->render[i], keywords[j], klen)  &&  is_separator(row->render[i + klen])){
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if(keywords[j] != NULL){
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if(changed  &&  row->idx + 1 < E.nrows){
    edUpdateSyntax(&E.row[row->idx + 1]);
  }
}

int edSyntaxToColor(int hl){
  switch(hl){
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;
    case HL_KEYWORD1: return 33;
    case HL_KEYWORD2: return 32;
    case HL_STRING: return 35;
    case HL_NUMBER: return 31;
    case HL_MATCH: return 34;
    default: return 37;
  }
}

void edSelectSyntaxHighlight(){
  E.syntax = NULL;
  if(E.filename == NULL){ return; }

  char * ext = strrchr(E.filename, '.');

  for(register unsigned int j = 0; j < HLDB_ENTRIES; j++){
    struct editorSyntax * s = &HLDB[j];
    unsigned int i = 0;
    while(s->filematch[i]){
      int is_ext = (s->filematch[i][0] == '.');
      if((is_ext  &&  ext  &&  !strcmp(ext, s->filematch[i]))  ||
         (!is_ext  &&  strstr(E.filename, s->filematch[i]))){
        E.syntax = s;

        for(register int filerow = 0; filerow < E.nrows; filerow++){
          edUpdateSyntax(&E.row[filerow]);
        }

        return;
      }
      i++;
    }
  }
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

int edRowRenxToCurx(erow *row, int renx) {  // Convert renx back to curx(opposite of edRowCurxToRenx)
  int cur_renx = 0;
  int cx;
  for(cx = 0; cx < row->size; cx++){
    if(row->chars[cx] == '\t'){
        cur_renx += (TAB_STOP - 1)  -  (cur_renx % TAB_STOP);
    }
    cur_renx++;
    if(cur_renx > renx){ return cx; }
  }
  return cx;
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
    edUpdateSyntax(row);
}

void edInsertRow(int at, char * s, size_t len){  // handles memory when a new row is inserted at index "at"
    if(at < 0  ||  at > E.nrows){ return; }

    E.row = realloc(E.row, sizeof(erow) * (E.nrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.nrows - at));
    for(register int j = at + 1; j <= E.nrows; j++){ E.row[j].idx++; }

    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = malloc(sizeof(char) * (len + 1));
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;

    edUpdateRow(&E.row[at]);
    E.nrows++;
    E.dirty++;
}

void edFreeRow(erow * row){  // frees the memory owned by a single erow
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void edDelRow(int at){  // removes row "at" from E.row, shifting the rest up
    if(at < 0  ||  at >= E.nrows){ return; }
    edFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.nrows - at - 1));
    for(register int j = at; j < E.nrows - 1; j++){ E.row[j].idx--; }
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

/* Bracket Matching */

int edIsOpenBracket(char c){  // true if c is an opening bracket character
  return c == '('  ||  c == '['  ||  c == '{';
}

int edIsCloseBracket(char c){  // true if c is a closing bracket character
  return c == ')'  ||  c == ']'  ||  c == '}';
}

char edMatchingBracket(char c){  // returns the bracket character that pairs with c
  switch(c){
    case '(': return ')';
    case ')': return '(';
    case '[': return ']';
    case ']': return '[';
    case '{': return '}';
    case '}': return '{';
  }
  return '\0';
}

int edIsRealBracketAt(erow * row, int cx){  // true if row->chars[cx] is a bracket that isn't inside a string/comment
  if(row == NULL  ||  cx < 0  ||  cx >= row->size){ return 0; }
  char c = row->chars[cx];
  if(!edIsOpenBracket(c)  &&  !edIsCloseBracket(c)){ return 0; }
  if(row->hl == NULL){ return 1; }

  int renx = edRowCurxToRenx(row, cx);
  if(renx < 0  ||  renx >= row->rsize){ return 1; }

  unsigned char h = row->hl[renx];
  return !(h == HL_STRING  ||  h == HL_COMMENT  ||  h == HL_MLCOMMENT);
}

void edFindMatchingBracket(){  // recomputes which bracket pair (if any) near the cursor should be highlighted
  E.brRow1 = -1; E.brCol1 = -1; E.brRow2 = -1; E.brCol2 = -1;
  if(!edIsCFile()  ||  E.cury >= E.nrows){ return; }

  erow *row = &E.row[E.cury];
  int cx = -1;
  if(E.curx < row->size  &&  edIsRealBracketAt(row, E.curx)){ cx = E.curx; }
  else if(E.curx > 0  &&  edIsRealBracketAt(row, E.curx - 1)){ cx = E.curx - 1; }
  if(cx == -1){ return; }

  char c = row->chars[cx];
  char want = edMatchingBracket(c);
  E.brRow1 = E.cury;
  E.brCol1 = cx;

  int depth = 1;
  if(edIsOpenBracket(c)){
    int ry = E.cury, rx = cx + 1;
    while(ry < E.nrows){
      erow *r = &E.row[ry];
      while(rx < r->size){
        if(edIsRealBracketAt(r, rx)){
          if(r->chars[rx] == c){ depth++; }
          else if(r->chars[rx] == want  &&  --depth == 0){
            E.brRow2 = ry; E.brCol2 = rx;
            return;
          }
        }
        rx++;
      }
      ry++;
      rx = 0;
    }
  }
  else{
    int ry = E.cury, rx = cx - 1;
    while(ry >= 0){
      erow *r = &E.row[ry];
      while(rx >= 0){
        if(edIsRealBracketAt(r, rx)){
          if(r->chars[rx] == c){ depth++; }
          else if(r->chars[rx] == want  &&  --depth == 0){
            E.brRow2 = ry; E.brCol2 = rx;
            return;
          }
        }
        rx--;
      }
      ry--;
      if(ry >= 0){ rx = E.row[ry].size - 1; }
    }
  }
}

/* Editor operations */

void edAutoOutdentOnCloseBracket(int c){  // removes one indent level when a closing bracket is typed as the first char on a line
  if(!edIsCloseBracket(c)){ return; }
  if(!edIsCFile()  ||  E.cury >= E.nrows){ return; }

  erow *row = &E.row[E.cury];
  for(register int i = 0; i < E.curx; i++){
    if(row->chars[i] != ' '  &&  row->chars[i] != '\t'){ return; }  // something other than whitespace precedes the cursor
  }
  if(E.curx == 0){ return; }

  edRowDelChar(row, E.curx - 1);
  E.curx--;
}

void edCheckAsciHelpCommand(){  // if the text just typed spells /ASCI_HELP, removes it and shows the help screen instead
  const char *cmd = "/ASCI_HELP";
  int cmdlen = strlen(cmd);
  if(E.cury >= E.nrows){ return; }

  erow *row = &E.row[E.cury];
  if(E.curx < cmdlen){ return; }
  if(strncmp(&row->chars[E.curx - cmdlen], cmd, cmdlen) != 0){ return; }

  for(register int i = 0; i < cmdlen; i++){
    edRowDelChar(row, E.curx - 1);
    E.curx--;
  }
  edShowHelp();
}

void edInsertChar(int c){
  edAutoOutdentOnCloseBracket(c);  // auto-dedent one level if this closing bracket starts the line
  if (E.cury == E.nrows){ edInsertRow(E.nrows, "", 0); }
  edRowInsertChar(&E.row[E.cury], E.curx, c);
  E.curx++;
  edCheckAsciHelpCommand();  // recognize /ASCI_HELP as a command rather than literal text
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

void edInsertNewline(){  // handles the Enter key: splits the current row at the cursor, auto-indenting for C/C++ files
  char indent[256];       // leading whitespace copied from the line being split
  int indent_len = 0;
  int opens = 0;           // true if a real open bracket sits just before the cursor
  int closes = 0;          // true if a real close bracket sits at/after the cursor (only whitespace between)

  if(edIsCFile()  &&  E.cury < E.nrows){
    erow *cur = &E.row[E.cury];

    while(indent_len < cur->size  &&  indent_len < (int)sizeof(indent) - 2  &&
          (cur->chars[indent_len] == ' '  ||  cur->chars[indent_len] == '\t')){
      indent[indent_len] = cur->chars[indent_len];
      indent_len++;
    }

    int j = E.curx - 1;
    while(j >= 0  &&  (cur->chars[j] == ' '  ||  cur->chars[j] == '\t')){ j--; }
    if(j >= 0  &&  edIsOpenBracket(cur->chars[j])  &&  edIsRealBracketAt(cur, j)){ opens = 1; }

    int k = E.curx;
    while(k < cur->size  &&  (cur->chars[k] == ' '  ||  cur->chars[k] == '\t')){ k++; }
    if(k < cur->size  &&  edIsCloseBracket(cur->chars[k])  &&  edIsRealBracketAt(cur, k)){ closes = 1; }
  }

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

  if(opens  &&  closes){
    // cursor sat directly between an empty pair, e.g. "{|}" -- give the closing bracket its own line
    edInsertRow(E.cury, "", 0);
    for(register int i = 0; i < indent_len; i++){ edRowInsertChar(&E.row[E.cury + 1], i, indent[i]); }
    for(register int i = 0; i < indent_len; i++){ edInsertChar(indent[i]); }
    edInsertChar('\t');
  }
  else if(opens){
    for(register int i = 0; i < indent_len; i++){ edInsertChar(indent[i]); }
    edInsertChar('\t');
  }
  else if(closes  &&  indent_len > 0){
    for(register int i = 0; i < indent_len - 1; i++){ edInsertChar(indent[i]); }
  }
  else{
    for(register int i = 0; i < indent_len; i++){ edInsertChar(indent[i]); }
  }
}

/* File IO */

void edFreeAllRows(){  // frees every row's memory and resets E.row/E.nrows back to empty
    for(register int i = 0; i < E.nrows; i++){
        edFreeRow(&E.row[i]);
    }
    free(E.row);
    E.row = NULL;
    E.nrows = 0;
}

void edLoadFileRows(FILE * fp){  // reads every line from an already-open stream, appending rows to E.row
    char * line = NULL;
    size_t linecap = 0;
    ssize_t llen;
    while((llen = getline(&line, &linecap, fp)) != -1){
        while(llen > 0  &&  (line[llen - 1] == '\n'  ||  line[llen - 1] == '\r')){  // remove newline
            llen--;
        }
        edInsertRow(E.nrows, line, llen);
    }
    free(line);
}

void edOpen(char * filename){  // open a file and start adding rows to editorconfig
    free(E.filename);
    E.filename = strdup(filename);
    edSelectSyntaxHighlight();
    FILE * fp = fopen(filename, "r");
    if(!fp){ die("fopen"); }
    edLoadFileRows(fp);
    fclose(fp);
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
    E.filename = edPrompt("Save as: %s (ESC to cancel)", NULL);
    if(E.filename == NULL){
      edSetStatusMessage("Save aborted");
      return;
    }
    edSelectSyntaxHighlight();
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

/* edWriteToFile: shared disk-write helper used by both normal and RCEX save.
 * Opens (creating if needed), truncates, then writes exactly datalen bytes.
 * Returns 0 on success, -1 on any error (errno is set). */
int edWriteToFile(const char *filename, const unsigned char *data, int datalen){
    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    if(fd == -1){ return -1; }
    if(ftruncate(fd, datalen) == -1){ close(fd); return -1; }

    /* Handle partial writes */
    int total = 0;
    while(total < datalen){
        ssize_t n = write(fd, data + total, (size_t)(datalen - total));
        if(n <= 0){ close(fd); return -1; }
        total += (int)n;
    }
    close(fd);
    return 0;
}

/* edPromptKey: reads a key/password from the status bar.
 * Characters are shown as '*' to avoid leaking the key on screen.
 * Returns a newly malloc'd NUL-terminated string the caller must free,
 * or NULL if the user pressed ESC (cancelled).
 * The returned string is zeroed before freeing by edSaveRCEX – do not
 * free it through any other path without wiping first. */
char *edPromptKey(const char *prompt_label){
    size_t bufsize = 128;
    char *buf  = malloc(bufsize);
    char *mask = malloc(bufsize);  /* same-length '*' string for display */
    if(!buf || !mask){
        free(buf); free(mask);
        return NULL;
    }

    size_t buflen = 0;
    buf[0] = mask[0] = '\0';

    while(1){
        /* Show stars in the status bar, never the key text */
        edSetStatusMessage("%s %s", prompt_label, mask);
        edRefreshScreen();

        int c = edReadKey();

        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
            if(buflen > 0){
                buf[--buflen]  = '\0';
                mask[buflen]   = '\0';
            }
        }
        else if(c == '\x1b'){
            /* ESC → cancel; wipe whatever was typed */
            memset(buf, 0, bufsize);
            free(buf); free(mask);
            edSetStatusMessage("");
            return NULL;
        }
        else if(c == '\r'){
            if(buflen > 0){
                free(mask);
                edSetStatusMessage("");
                return buf;   /* caller owns buf; caller must wipe+free */
            }
            /* empty key → force the user to type something */
        }
        else if(!iscntrl(c) && c < 128){
            if(buflen == bufsize - 1){
                bufsize *= 2;
                buf  = realloc(buf,  bufsize);
                mask = realloc(mask, bufsize);
                if(!buf || !mask){
                    free(buf); free(mask);
                    return NULL;
                }
            }
            buf[buflen]  = (char)c;
            mask[buflen] = '*';
            buflen++;
            buf[buflen] = mask[buflen] = '\0';
        }
        /* ignore all other keys (arrows, function keys, etc.) */
    }
}

/* edSaveRCEX: encrypts the current editor buffer with RCEX and writes:
 *   [ nonce (RCEX_NONCE_LEN bytes) ][ MAC tag (MACLEN bytes) ][ ciphertext ]
 * to the user-chosen output file.
 * The nonce is randomly generated per-save via rcexrandbytes().
 * The key is collected via edPromptKey() (masked) and immediately wiped
 * after the Rcex context has been initialised.
 * E.dirty is cleared only on full success. */
void edSaveRCEX(void){
    /* ---- 1. Ask where to save ---- */
    char default_name[512];
    if(E.filename){
        snprintf(default_name, sizeof(default_name), "%s.rcex", E.filename);
    } else {
        snprintf(default_name, sizeof(default_name), "untitled.rcex");
    }

    char prompt[640];
    snprintf(prompt, sizeof(prompt), "Save encrypted file as: %%s");
    char *outname = edPrompt(prompt, NULL);
    if(!outname){
        edSetStatusMessage("RCEX save cancelled");
        return;
    }
    if(outname[0] == '\0'){
        free(outname);
        outname = strdup(default_name);
    }

    /* ---- 2. Collect the encryption key (masked) ---- */
    char *key = edPromptKey("RCEX Key:");
    if(!key){
        free(outname);
        edSetStatusMessage("RCEX save cancelled");
        return;
    }

    int16 keylen = (int16)strlen(key);
    if(!rcexvalidate((int8 *)key, keylen)){
        memset(key, 0, (size_t)keylen);
        free(key);
        free(outname);
        edSetStatusMessage("RCEX encryption failed: invalid key");
        return;
    }

    /* ---- 3. Build plaintext from editor rows ---- */
    int ptlen = 0;
    char *plaintext = edRowsToString(&ptlen);
    if(!plaintext){
        memset(key, 0, (size_t)keylen);
        free(key);
        free(outname);
        edSetStatusMessage("RCEX encryption failed: out of memory");
        return;
    }

    /* ---- 4. Generate a fresh random nonce ---- */
    int8 nonce[RCEX_NONCE_LEN];
    if(rcexrandbytes(nonce, RCEX_NONCE_LEN) != RCEX_NONCE_LEN){
        free(plaintext);
        memset(key, 0, (size_t)keylen);
        free(key);
        free(outname);
        edSetStatusMessage("RCEX encryption failed: nonce generation error");
        return;
    }

    /* ---- 5. Initialise RCEX cipher context (key + nonce) ---- */
    Rcex *ctx = rcexinit_nonce((int8 *)key, keylen, nonce, RCEX_NONCE_LEN);
    if(!ctx){
        free(plaintext);
        memset(key, 0, (size_t)keylen);
        free(key);
        free(outname);
        edSetStatusMessage("RCEX encryption failed: context init error");
        return;
    }

    /* ---- 6. Encrypt ---- */
    int8 *ciphertext = rcexencrypt(ctx, (int8 *)plaintext, (int16)ptlen);
    rcexwipe(ctx);      /* wipe & free the cipher context immediately */
    free(plaintext);
    plaintext = NULL;

    if(!ciphertext){
        memset(key, 0, (size_t)keylen);
        free(key);
        free(outname);
        edSetStatusMessage("RCEX encryption failed");
        return;
    }

    /* ---- 7. Compute MAC keyed on key || nonce ----
     * mac_key = key bytes followed by nonce bytes.
     * This binds the MAC to the cipher key so that a wrong decryption key
     * will produce a different mac_key → different MAC → MAC mismatch
     * detected before any decrypted bytes are loaded into the editor. */
    int8 mac[MACLEN];
    {
        int16 mklen   = keylen + (int16)RCEX_NONCE_LEN;
        int8 *mac_key = malloc((size_t)mklen);
        if(!mac_key){
            free(ciphertext);
            memset(key, 0, (size_t)keylen);
            free(key);
            free(outname);
            edSetStatusMessage("RCEX encryption failed: out of memory");
            return;
        }
        memcpy(mac_key,         key,   keylen);
        memcpy(mac_key + keylen, nonce, RCEX_NONCE_LEN);
        rcexmac(mac_key, mklen, ciphertext, (int16)ptlen, mac);
        memset(mac_key, 0, (size_t)mklen);
        free(mac_key);
    }

    /* Wipe the user key — no longer needed */
    memset(key, 0, (size_t)keylen);
    free(key);
    key = NULL;

    /* ---- 8. Assemble output buffer: nonce | MAC | ciphertext ---- */
    int outlen = RCEX_NONCE_LEN + MACLEN + ptlen;
    unsigned char *outbuf = malloc((size_t)outlen);
    if(!outbuf){
        free(ciphertext);
        free(outname);
        edSetStatusMessage("RCEX encryption failed: out of memory");
        return;
    }
    memcpy(outbuf,                            nonce,      RCEX_NONCE_LEN);
    memcpy(outbuf + RCEX_NONCE_LEN,           mac,        MACLEN);
    memcpy(outbuf + RCEX_NONCE_LEN + MACLEN,  ciphertext, ptlen);

    free(ciphertext);
    ciphertext = NULL;

    /* ---- 9. Write to disk ---- */
    if(edWriteToFile(outname, outbuf, outlen) == 0){
        memset(outbuf, 0, (size_t)outlen);
        free(outbuf);
        E.dirty = 0;
        edSetStatusMessage("File encrypted and saved using RCEX: %s (%d bytes)", outname, outlen);
    } else {
        memset(outbuf, 0, (size_t)outlen);
        free(outbuf);
        edSetStatusMessage("RCEX encryption failed: I/O error: %s", strerror(errno));
    }

    free(outname);
}

/* edSaveOptions: intercepts Ctrl-S and presents a one-key menu:
 *   1 → normal plaintext save (calls edSave)
 *   2 → RCEX-encrypted save  (calls edSaveRCEX)
 *   ESC → cancel */
void edSaveOptions(void){
    edSetStatusMessage("Save: [1] Normal  [2] RCEX Encrypt  ESC=Cancel");
    edRefreshScreen();

    while(1){
        int c = edReadKey();
        if(c == '1'){
            edSave();
            return;
        }
        else if(c == '2'){
            edSaveRCEX();
            return;
        }
        else if(c == '\x1b'){
            edSetStatusMessage("Save cancelled");
            return;
        }
        /* any other key → re-display the prompt */
        edSetStatusMessage("Save: [1] Normal  [2] RCEX Encrypt  ESC=Cancel");
        edRefreshScreen();
    }
}

/* edLoadPlaintextBytes: binary-safe replacement for edLoadFileRows when the
 * source is already a raw byte buffer (e.g. decrypted ciphertext) rather than
 * an open FILE*.  Splits on '\n' (stripping any trailing '\r'), exactly like
 * edLoadFileRows does when reading a text file.
 *
 * Precondition: the editor buffer has already been cleared (edFreeAllRows).
 * The data pointer is read-only; no ownership is taken. */
void edLoadPlaintextBytes(const char *data, int datalen){
    const char *p   = data;
    const char *end = data + datalen;

    while(p <= end){
        /* find the next newline (or end-of-buffer) */
        const char *nl = p;
        while(nl < end && *nl != '\n'){ nl++; }

        int rowlen = (int)(nl - p);
        /* strip trailing \r (Windows line endings) */
        if(rowlen > 0 && p[rowlen - 1] == '\r'){ rowlen--; }

        edInsertRow(E.nrows, (char *)p, rowlen);

        if(nl >= end){ break; }
        p = nl + 1;   /* skip past the '\n' */
    }
}

/* edOpenFileRCEX: opens an RCEX-encrypted file, verifies its MAC, decrypts it,
 * and loads the resulting plaintext into the editor rows.
 *
 * Expected on-disk format (written by edSaveRCEX):
 *   [ nonce : RCEX_NONCE_LEN bytes ]
 *   [ mac   : MACLEN bytes         ]
 *   [ ciphertext : N bytes         ]
 *
 * The key is collected via edPromptKey() (masked).
 * Returns 1 on success, 0 on any failure (editor buffer is left empty on
 * failure; caller must not commit E.filename until this returns 1). */
int edOpenFileRCEX(const char *filename){
    /* ---- 1. Read the whole file as raw bytes ---- */
    FILE *fp = fopen(filename, "rb");
    if(!fp){
        edSetStatusMessage("Can't open file: %s", strerror(errno));
        return 0;
    }

    /* Determine file size */
    if(fseek(fp, 0, SEEK_END) != 0){ fclose(fp); edSetStatusMessage("Read error: %s", strerror(errno)); return 0; }
    long fsz = ftell(fp);
    if(fsz < 0){ fclose(fp); edSetStatusMessage("Read error: %s", strerror(errno)); return 0; }
    rewind(fp);

    int filelen = (int)fsz;
    int header  = RCEX_NONCE_LEN + MACLEN;   /* 16 + 16 = 32 bytes */

    if(filelen < header){
        fclose(fp);
        edSetStatusMessage("Not a valid RCEX file (too small)");
        return 0;
    }

    unsigned char *filebuf = malloc((size_t)filelen);
    if(!filebuf){
        fclose(fp);
        edSetStatusMessage("RCEX open failed: out of memory");
        return 0;
    }

    if((int)fread(filebuf, 1, (size_t)filelen, fp) != filelen){
        fclose(fp); free(filebuf);
        edSetStatusMessage("RCEX open failed: short read");
        return 0;
    }
    fclose(fp);

    /* ---- 2. Split into header fields ---- */
    int8 *nonce      = filebuf;                          /* first 16 bytes */
    int8 *stored_mac = filebuf + RCEX_NONCE_LEN;         /* next  16 bytes */
    int8 *ciphertext = filebuf + header;                 /* remaining bytes */
    int   ctlen      = filelen - header;

    /* ---- 3. Collect the decryption key (masked) ---- */
    char *key = edPromptKey("RCEX Key:");
    if(!key){
        free(filebuf);
        edSetStatusMessage("Open cancelled");
        return 0;
    }

    int16 keylen = (int16)strlen(key);
    if(!rcexvalidate((int8 *)key, keylen)){
        memset(key, 0, (size_t)keylen);
        free(key);
        free(filebuf);
        edSetStatusMessage("RCEX open failed: invalid key");
        return 0;
    }

    /* ---- 4. Verify MAC before decrypting (key||nonce-keyed, matches edSaveRCEX) ---- */
    int8 computed_mac[MACLEN];
    {
        int16 mklen   = keylen + (int16)RCEX_NONCE_LEN;
        int8 *mac_key = malloc((size_t)mklen);
        if(!mac_key){
            memset(key, 0, (size_t)keylen);
            free(key);
            memset(filebuf, 0, (size_t)filelen);
            free(filebuf);
            edSetStatusMessage("RCEX open failed: out of memory");
            return 0;
        }
        memcpy(mac_key,         key,   keylen);
        memcpy(mac_key + keylen, nonce, RCEX_NONCE_LEN);
        rcexmac(mac_key, mklen, ciphertext, (int16)ctlen, computed_mac);
        memset(mac_key, 0, (size_t)mklen);
        free(mac_key);
    }

    /* Constant-time comparison */
    int8 mac_diff = 0;
    for(int i = 0; i < MACLEN; i++){ mac_diff |= (computed_mac[i] ^ stored_mac[i]); }
    if(mac_diff != 0){
        /* MAC mismatch — wrong key or tampered file */
        memset(key, 0, (size_t)keylen);
        free(key);
        memset(filebuf, 0, (size_t)filelen);
        free(filebuf);
        edSetStatusMessage("RCEX open failed: wrong key or file is corrupted/tampered");
        return 0;
    }

    /* ---- 5. Initialise RCEX context (same key+nonce as encryption) ---- */
    Rcex *ctx = rcexinit_nonce((int8 *)key, keylen, nonce, RCEX_NONCE_LEN);

    /* Wipe key immediately after context is ready */
    memset(key, 0, (size_t)keylen);
    free(key);
    key = NULL;

    if(!ctx){
        memset(filebuf, 0, (size_t)filelen);
        free(filebuf);
        edSetStatusMessage("RCEX open failed: context init error");
        return 0;
    }

    /* ---- 6. Decrypt (stream cipher: same op as encrypt) ---- */
    edSetStatusMessage("Decrypting with RCEX...");
    edRefreshScreen();

    int8 *plaintext = rcexdecrypt(ctx, ciphertext, (int16)ctlen);
    rcexwipe(ctx);   /* wipe & free context immediately */

    /* Wipe and free the encrypted buffer */
    memset(filebuf, 0, (size_t)filelen);
    free(filebuf);
    filebuf = ciphertext = nonce = stored_mac = NULL;

    if(!plaintext){
        edSetStatusMessage("RCEX open failed: decryption error");
        return 0;
    }

    /* ---- 7. Load decrypted bytes into editor rows ---- */
    edLoadPlaintextBytes((const char *)plaintext, ctlen);

    /* Wipe and free the plaintext buffer (sensitive) */
    memset(plaintext, 0, (size_t)ctlen);
    free(plaintext);

    return 1;   /* success */
}

void edOpenFile(){  // Ctrl-O: prompts for a filename and replaces the current buffer with that file's contents
  /* ---- unsaved-changes guard (existing behaviour) ---- */
  if(E.dirty){
    char * confirm = edPrompt("Unsaved changes. Open anyway? (y/n): %s", NULL);
    if(confirm == NULL){ edSetStatusMessage("Open cancelled"); return; }
    int ok = (confirm[0] == 'y'  ||  confirm[0] == 'Y');
    free(confirm);
    if(!ok){ edSetStatusMessage("Open cancelled"); return; }
  }

  /* ---- collect filename ---- */
  char * filename = edPrompt("Open file: %s (ESC to cancel)", NULL);
  if(filename == NULL){ return; }

  /* ---- verify the file exists before touching the current buffer ---- */
  FILE * probe = fopen(filename, "r");
  if(!probe){
    edSetStatusMessage("Can't open file: %s", strerror(errno));
    free(filename);
    return;
  }
  fclose(probe);   /* just a probe; re-open below with the right mode */

  /* ---- ask whether to RCEX-decrypt ---- */
  char *decrypt_choice = edPrompt("Decrypt with RCEX? (y/n): %s", NULL);
  int do_decrypt = 0;
  if(decrypt_choice == NULL){
    /* ESC → cancel entire open */
    free(filename);
    edSetStatusMessage("Open cancelled");
    return;
  }
  do_decrypt = (decrypt_choice[0] == 'y' || decrypt_choice[0] == 'Y');
  free(decrypt_choice);

  /* ---- commit: clear the current buffer NOW ---- */
  edFreeAllRows();
  E.curx = 0; E.cury = 0; E.rowoff = 0; E.coloff = 0;
  free(E.filename);
  E.filename = filename;

  if(do_decrypt){
    /* edOpenFileRCEX prompts for the key, reads binary, verifies MAC,
     * decrypts, and calls edLoadPlaintextBytes internally. */
    if(!edOpenFileRCEX(filename)){
      /* Failed — buffer is empty; keep E.filename so the status bar is
       * informative, but mark dirty=0 so Ctrl-Q doesn't complain. */
      E.dirty = 0;
      return;
    }
    E.dirty = 0;
    edSelectSyntaxHighlight();
    edSetStatusMessage("File decrypted successfully: \"%s\"", E.filename);
  } else {
    /* Normal plaintext open (existing behaviour) */
    FILE * fp = fopen(filename, "r");
    if(!fp){
      edSetStatusMessage("Can't open file: %s", strerror(errno));
      E.dirty = 0;
      return;
    }
    edLoadFileRows(fp);
    fclose(fp);
    E.dirty = 0;
    edSelectSyntaxHighlight();
    edSetStatusMessage("Opened \"%s\"", E.filename);
  }
}

/* ---------- end RCEX-open helpers ---------- */

/* Find */

void edFindCallback(char * query, int key){  // runs after every keypress in the search prompt
  static int last_match = -1;
  static int direction = 1;
  static int saved_hl_line;
  static char * saved_hl = NULL;

  if(saved_hl){
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if(key == '\r'  ||  key == '\x1b'){
    last_match = -1;
    direction = 1;
    return;
  }
  else if(key == ARROW_RIGHT  ||  key == ARROW_DOWN){
    direction = 1;
  }
  else if(key == ARROW_LEFT  ||  key == ARROW_UP){
    direction = -1;
  }
  else{
    last_match = -1;
    direction = 1;
  }

  if(last_match == -1){ direction = 1; }
  int current = last_match;
  for(register int i = 0; i < E.nrows; i++){
    current += direction;
    if(current == -1){ current = E.nrows - 1; }
    else if(current == E.nrows){ current = 0; }

    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if(match){
      last_match = current;
      E.cury = current;
      E.curx = edRowRenxToCurx(row, match - row->render);
      E.rowoff = E.nrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void edFind(){  // search prompt, restores cursor/scroll position if the search is cancelled
  int saved_curx = E.curx;
  int saved_cury = E.cury;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char * query = edPrompt("Search: %s (Use ESC/Arrows/Enter)", edFindCallback);

  if(query){
    free(query);
  }
  else{
    E.curx = saved_curx;
    E.cury = saved_cury;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/* Go To Line */

void edGoToLine(){  // Ctrl-G: prompts for a line number and jumps the cursor there, preserving column and centering the view
  char * input = edPrompt("Go to line: %s", NULL);
  if(input == NULL){ return; }
  if(E.nrows == 0){ free(input); return; }

  int line = atoi(input);
  free(input);
  if(line < 1){ line = 1; }
  if(line > E.nrows){ line = E.nrows; }

  E.cury = line - 1;
  if(E.curx > E.row[E.cury].size){ E.curx = E.row[E.cury].size; }

  E.rowoff = E.cury - (E.screenrows / 2);
  if(E.rowoff < 0){ E.rowoff = 0; }
}

/* Help */

void edShowHelp(){  // displays a temporary full-screen help view; returns to the editor on ESC or 'q' without touching the file
  struct abuf ab = ABUF_INIT;
  abufAppend(&ab, "\x1b[?25l", 6);
  abufAppend(&ab, "\x1b[H", 3);
  abufAppend(&ab, "\x1b[2J", 4);

  const char * lines[] = {
    "ASTE - A Small Text Editor",
    "----------------------------------------",
    "",
    "Navigation",
    "  Arrow Keys       Move cursor",
    "  Home             Start of line",
    "  End              End of line",
    "  Page Up          Move one page up",
    "  Page Down        Move one page down",
    "  Ctrl-G           Go to line",
    "",
    "Editing",
    "  Enter            Insert new line (auto-indents in C/C++ files)",
    "  Backspace        Delete previous character",
    "  Delete           Delete next character",
    "",
    "File",
    "  Ctrl-O           Open file",
    "  Ctrl-S           Save file",
    "  Ctrl-Q           Quit",
    "",
    "Search",
    "  Ctrl-F           Find text",
    "",
    "Help",
    "  /ASCI_HELP       Show this help",
    "",
    "Press ESC or q to return to the editor",
    NULL
  };

  for(register int i = 0; lines[i] != NULL  &&  i < E.screenrows; i++){
    abufAppend(&ab, lines[i], strlen(lines[i]));
    abufAppend(&ab, "\x1b[K", 3);
    abufAppend(&ab, "\r\n", 2);
  }

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);

  int c;
  do{
    c = edReadKey();
  } while(c != '\x1b'  &&  c != 'q');

  edRefreshScreen();  // redraw the normal editor exactly where the user left off
}

/* Input */

char * edPrompt(char * prompt, void (*callback)(char *, int)){  // shows a prompt in the status bar and reads a line of input for it
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
            if(callback){ callback(buf, c); }
            free(buf);
            return NULL;
        }
        else if(c == '\r'){
            if(buflen != 0){
                edSetStatusMessage("");
                if(callback){ callback(buf, c); }
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

        if(callback){ callback(buf, c); }
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
            edSaveOptions();
            break;

        case CTRL_KEY('o'):
            edOpenFile();
            break;

        case HOME_KEY:
            E.curx = 0;
            break;
        case END_KEY:
            if(E.cury < E.nrows){
                E.curx = E.row[E.cury].size;
            }
            break;  

        case CTRL_KEY('f'):
            edFind();
            break;

        case CTRL_KEY('g'):
            edGoToLine();
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

int edLineNumWidth(){  // digit width needed to display E.nrows as a line number (never smaller than 1)
    int n = E.nrows;
    int width = 1;
    while(n >= 10){ n /= 10; width++; }
    return width;
}

void edDrawLineNumber(struct abuf * ab, int filerow, int numwidth){  // draws the left-hand gutter for one screen row (blank past EOF)
    char buf[32];
    int len;
    abufAppend(ab, "\x1b[90m", 5);
    if(filerow < E.nrows){
        len = snprintf(buf, sizeof(buf), "%*d ", numwidth, filerow + 1);
    }
    else{
        len = snprintf(buf, sizeof(buf), "%*s ", numwidth, "");
    }
    abufAppend(ab, buf, len);
    abufAppend(ab, "\x1b[39m", 5);
}

void edScroll(){  // handles offsets when cursor is moved
    edFindMatchingBracket();  // recompute the active bracket-match pair for the new cursor position

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

    int content_cols = E.screencols - (edLineNumWidth() + 1);  // the gutter eats into the visible content width
    if(content_cols < 1){ content_cols = 1; }

    if(E.renx < E.coloff){
        E.coloff = E.renx;
    }
    if(E.renx >= E.coloff + content_cols){
        E.coloff = E.renx - content_cols + 1;
    }
}

void edDrawRows(struct abuf * ab){
    int numwidth = edLineNumWidth();   // digit width recalculated each frame since E.nrows can change
    int gutter = numwidth + 1;         // digits + one trailing space
    for(register int i = 0; i < E.screenrows; i++){
        int filerow = i + E.rowoff;
        edDrawLineNumber(ab, filerow, numwidth);
        int content_cols = E.screencols - gutter;
        if(content_cols < 0){ content_cols = 0; }

        if(filerow >= E.nrows){
            if(E.nrows == 0  &&  i == E.screenrows / 3){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "ASTE - A Small Text Editor");
            if(welcomelen > content_cols){ welcomelen = content_cols; }
            int padding = (content_cols - welcomelen) / 2;
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
            if(len > content_cols) { len = content_cols; }
            char * c = &E.row[filerow].render[E.coloff];
            unsigned char * hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;

            int br1_renx = (filerow == E.brRow1) ? edRowCurxToRenx(&E.row[filerow], E.brCol1) : -1;
            int br2_renx = (filerow == E.brRow2) ? edRowCurxToRenx(&E.row[filerow], E.brCol2) : -1;

            for(register int j = 0; j < len; j++){
                int abscol = j + E.coloff;
                if(abscol == br1_renx  ||  abscol == br2_renx){
                    // bracket-match overlay: bold+reverse for a matched pair, plus red if the bracket has no partner
                    if(E.brRow2 == -1){ abufAppend(ab, "\x1b[1;7;31m", 9); }
                    else{ abufAppend(ab, "\x1b[1;7m", 6); }
                    abufAppend(ab, &c[j], 1);
                    abufAppend(ab, "\x1b[m", 3);
                    current_color = -1;
                    continue;
                }
                if(iscntrl(c[j])){
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, "\x1b[7m", 4);
                    abufAppend(ab, &sym, 1);
                    abufAppend(ab, "\x1b[m", 3);
                    if(current_color != -1){
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abufAppend(ab, buf, clen);
                    }
                }
                else if(hl[j] == HL_NORMAL){
                    if(current_color != -1){
                        abufAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abufAppend(ab, &c[j], 1);
                }
                else{
                    int color = edSyntaxToColor(hl[j]);
                    if(color != current_color){
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abufAppend(ab, buf, clen);
                    }
                    abufAppend(ab, &c[j], 1);
                }
            }
            abufAppend(ab, "\x1b[39m", 5);
        }

        abufAppend(ab, "\x1b[K", 3);
        abufAppend(ab, "\r\n", 2);
    }
}

void edDrawStatusBar(struct abuf *ab) {
    abufAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.nrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.cury + 1, E.nrows);
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
    int gutter = edLineNumWidth() + 1;
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cury-E.rowoff) + 1, (E.renx-E.coloff) + 1 + gutter);
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

    edSetStatusMessage("HELP: Ctrl-O = open | Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-G = go to line | /ASCI_HELP = help");

    while(1){
        edRefreshScreen();
        edProcessKeypress();
    }
    return 0;
}