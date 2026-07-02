#include<unistd.h>

int main(){
    char c;
    while(read(STDERR_FILENO, &c, 1) == 1  &&  c != 'q');  // read() returns 0 when reached EOF
    return 0;
}