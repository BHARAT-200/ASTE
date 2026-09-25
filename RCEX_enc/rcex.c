// rcex.c
#include"rcex.h"


void zero(int8 *str, int16 size) {
    int8 *p;
    int16 n;

    for (n=0, p=str; n<size; n++, p++){
        *p = 0;
    }

    return;
}

export Rcex * rcexinit(int8 * key, int16 size){
    // Initialize RCEX context and perform key scheduling
    int16 tmp1, tmp2;
    Rcex * p = (Rcex*) malloc(sizeof(struct s_rcex));
    int32 n;

    if(!p){
        printf("Error: memory allocation failed for Rcex\n");
        exit(EXIT_FAILURE);
    }
    
    zero(p->s, 256);
    p->i=p->j=p->k = 0;
    tmp1=tmp2 = 0;
    
    for(p->i = 0; p->i < 256; p->s[p->i] = p->i, p->i++);  // Initialize S array

    // Key Scheduling Algorithm (KSA)
    for(p->i = 0; p->i < 256; p->i++){
        tmp1 = p->i % size;
        tmp2 = p->j + p->s[p->i] + key[tmp1];
        p->j = tmp2 % 256;  // j = (j + s[i] + key[i mod size]) mod 256

        tmp1 = p->s[p->i];  // swap
        p->s[p->i] = p->s[p->j];
        p->s[p->j] = tmp1;
    }

    p->i=p->j=p->k = 0; 
    rcexwhitewash(n, p);  // Discard first bytes to remove key bias

    return p;
}

int8 rcexbyte(Rcex * p){  // create 1 byte of keystream, will be used to encrypt and decrypt
    int16 tmp1, tmp2;

    p->i = (p->i + 1) % 256;
    p->j = (p->j + p->s[p->i]) % 256;

    tmp1 = p->s[p->i];  // swap
    p->s[p->i] = p->s[p->j];
    p->s[p->j] = tmp1;

    // Select keystream byte
    tmp2 = (p->s[p->i] + p->s[p->j]) % 256;
    p->k = p->s[tmp2];

    return p->k;
}

export int8 * rcexencrypt(Rcex * p, int8 * cleartext, int16 size){ 
    // XOR each plaintext byte with one byte of keystream
    int8 * ciphertext;
    int16 x;

    ciphertext = (int8*) malloc(size + 1);
    if(!ciphertext){
        printf("Error: memory allocation failed for ciphertext\n");
        exit(EXIT_FAILURE);
    }

    for(x = 0; x < size; x++){
        ciphertext[x] = cleartext[x] ^ rcexbyte(p);  // Ciphertext = cleartext XOR keystream
    }

    return ciphertext;

}

export int16 rcexvalidate(int8 * key, int16 size){  // Checks the key isn't NULL or empty before rcexinit() runs.
    if(!key){
        printf("Error: key pointer is NULL\n");
        return 0;
    }
    if(size == 0){
        printf("Error: key size must not be zero\n");
        return 0;
    }
    return 1;
}

export int16 rcexrandbytes(int8 * buf, int16 size){  // Fills buf with random bytes from the OS, used to make the random nonce for rcexinit_nonce() below.
    FILE * f;
    int16 n;

    f = fopen("/dev/urandom", "rb");
    if(!f){
        printf("Error: unable to open /dev/urandom\n");
        exit(EXIT_FAILURE);
    }

    n = fread(buf, 1, size, f);
    fclose(f);

    if(n != size){
        printf("Error: short read from /dev/urandom\n");
        exit(EXIT_FAILURE);
    }

    return n;
}

export Rcex * rcexinit_nonce(int8 * key, int16 keysize, int8 * nonce, int16 noncesize){  // Mixes a random nonce into the key so every message gets a fresh keystream, send the nonce with the ciphertext, the receiver needs it to decrypt.
    int8 * combined;
    int16 combinedsize;
    Rcex * p;

    combinedsize = keysize + noncesize;
    combined = (int8*) malloc(combinedsize);
    if(!combined){
        printf("Error: memory allocation failed for combined key\n");
        exit(EXIT_FAILURE);
    }

    memcpy(combined, key, keysize);
    memcpy(combined + keysize, nonce, noncesize);

    p = rcexinit(combined, combinedsize);  // main KSA, untouched

    zero(combined, combinedsize);  // wipe the temporary combined key
    free(combined);

    return p;
}

export void rcexwipe(Rcex * p){  // Clears the S-box and indices before freeing, so nothing sensitive is left sitting in freed memory. Use instead of rcexuninit().
    if(!p){
        return;
    }

    zero(p->s, 256);
    p->i = p->j = p->k = 0;

    free(p);

    return;
}

export void rcexmac(int8 * key, int16 keysize, int8 * data, int16 size, int8 * tag){
    // Builds a tamper-check code over data using a separate, independently-keyed stream.
    // Call it on the ciphertext; a mismatch on the other end means it was altered.
    int8 * dkey;
    int16 dkeysize;
    Rcex * m;
    int16 x;

    dkeysize = keysize + 3;
    dkey = (int8*) malloc(dkeysize);
    if(!dkey){
        printf("Error: memory allocation failed for MAC key\n");
        exit(EXIT_FAILURE);
    }

    memcpy(dkey, key, keysize);
    dkey[keysize]     = 'M';
    dkey[keysize + 1] = 'A';
    dkey[keysize + 2] = 'C';

    m = rcexinit(dkey, dkeysize);  // independent keystream, domain-separated key

    zero(tag, MACLEN);
    for(x = 0; x < size; x++){
        tag[x % MACLEN] ^= (data[x] ^ rcexbyte(m));
    }

    rcexwipe(m);
    zero(dkey, dkeysize);
    free(dkey);

    return;
}

export int16 rcexverify(int8 * key, int16 keysize, int8 * data, int16 size, int8 * tag){
    // Recomputes the tamper-check code and compares it byte by byte, no early exit.
    // Returns 1 if it matches, 0 if not; avoids leaking info via timing.
    int8 check[MACLEN];
    int8 diff;
    int16 x;

    rcexmac(key, keysize, data, size, check);

    diff = 0;
    for(x = 0; x < MACLEN; x++){
        diff |= (check[x] ^ tag[x]);
    }

    zero(check, MACLEN);

    return (diff == 0);
}