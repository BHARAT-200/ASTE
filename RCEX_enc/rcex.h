#pragma once
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<errno.h>

#define TS                  50
#define export              __attribute__((visibility("default")))
#define rcexdecrypt(x,y,z)   rcexencrypt(x,y,z)
#define rcexuninit(x)        free(x)
#define rcexwhitewash(x, y)  for(x = 0; x < (TS * 100); x++)     \
                                (volatile int8)rcexbyte(y);      \
// rcexwhitewash: Drops the first 5000 bytes of rcexkeystream to improve randomness as they were too biased and leaks info about the key.
// both encryption and decryption performs the same whitewash, skipping the same 5000 bytes and start XORing at the same position in the keystream.


typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;
struct s_rcex{
    int16 i, j, k;
    int8 s[256];
};

typedef struct s_rcex Rcex;


export Rcex * rcexinit(int8 *, int16);
int8 rcexbyte(Rcex *);
export int8 * rcexencrypt(Rcex *, int8 *, int16);

#define MACLEN              16   // bytes in the tag produced by rcexmac()

// added more security layers below;
export int16   rcexvalidate(int8 *, int16);
export int16   rcexrandbytes(int8 *, int16);
export Rcex *  rcexinit_nonce(int8 *, int16, int8 *, int16);
export void    rcexwipe(Rcex *);
export void    rcexmac(int8 *, int16, int8 *, int16, int8 *);
export int16   rcexverify(int8 *, int16, int8 *, int16, int8 *);