#include "../RCEX_enc/rcex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define RUNS 10
#define MB (1024ULL * 1024ULL)

/*
 * rcexencrypt() takes int16.
 * int16 is unsigned short in this implementation.
 *
 * Therefore the largest safe single call is 65535 bytes.
 */
#define RCEX_CHUNK 65535

#define NONCE_SIZE 16

static double now_seconds(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    return (double)ts.tv_sec +
           (double)ts.tv_nsec / 1000000000.0;
}

/*
 * Encrypt an arbitrary-size buffer using the actual
 * RCEX implementation in valid <=65535 byte chunks.
 */
static unsigned char *encrypt_large(
    const unsigned char *plaintext,
    size_t size,
    const unsigned char *key,
    size_t keylen,
    const unsigned char *nonce
) {
    unsigned char *ciphertext =
        malloc(size);

    if (!ciphertext) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    /*
     * One RCEX context is maintained across chunks.
     * This means the keystream continues naturally.
     */
    Rcex *ctx = rcexinit_nonce(
        (int8 *)key,
        (int16)keylen,
        (int8 *)nonce,
        NONCE_SIZE
    );

    size_t offset = 0;

    while (offset < size) {

        size_t remaining = size - offset;

        int16 chunk =
            remaining > RCEX_CHUNK ?
            RCEX_CHUNK :
            (int16)remaining;

        int8 *result =
            rcexencrypt(
                ctx,
                (int8 *)(plaintext + offset),
                chunk
            );

        memcpy(
            ciphertext + offset,
            result,
            chunk
        );

        /*
         * rcexencrypt allocates the result.
         */
        free(result);

        offset += chunk;
    }

    rcexwipe(ctx);

    return ciphertext;
}

/*
 * Decrypt using the same RCEX stream.
 */
static unsigned char *decrypt_large(
    const unsigned char *ciphertext,
    size_t size,
    const unsigned char *key,
    size_t keylen,
    const unsigned char *nonce
) {
    unsigned char *plaintext =
        malloc(size);

    if (!plaintext) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    Rcex *ctx = rcexinit_nonce(
        (int8 *)key,
        (int16)keylen,
        (int8 *)nonce,
        NONCE_SIZE
    );

    size_t offset = 0;

    while (offset < size) {

        size_t remaining = size - offset;

        int16 chunk =
            remaining > RCEX_CHUNK ?
            RCEX_CHUNK :
            (int16)remaining;

        /*
         * rcexdecrypt is defined as rcexencrypt.
         */
        int8 *result =
            rcexdecrypt(
                ctx,
                (int8 *)(ciphertext + offset),
                chunk
            );

        memcpy(
            plaintext + offset,
            result,
            chunk
        );

        free(result);

        offset += chunk;
    }

    rcexwipe(ctx);

    return plaintext;
}

// Generate deterministic input

static unsigned char *generate_data(size_t size) {

    unsigned char *data =
        malloc(size);

    if (!data) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < size; i++) {
        data[i] =
            (unsigned char)
            ((i * 31 + 17) & 0xff);
    }

    return data;
}

// Benchmark

static void benchmark_size(size_t size) {
 
    printf("\n");
    printf("Input size: %.2f MB\n",
           (double)size / MB); 
    printf("\n");

    unsigned char *plaintext =
        generate_data(size);

    unsigned char key[] =
        "ASTE-RCEX-Benchmark-Key";

    double enc_times[RUNS];
    double dec_times[RUNS];

    unsigned char nonce[NONCE_SIZE];

    /*
     * Deterministic nonce for reproducibility.
     * This is a benchmark only, not production encryption.
     */
    for (int i = 0; i < NONCE_SIZE; i++)
        nonce[i] = (unsigned char)(i + 1);

    unsigned char *ciphertext = NULL;

    /* ---------------- Encryption ---------------- */

    for (int run = 0; run < RUNS; run++) {

        double start = now_seconds();

        ciphertext =
            encrypt_large(
                plaintext,
                size,
                key,
                strlen((char *)key),
                nonce
            );

        double end = now_seconds();

        enc_times[run] =
            end - start;

        /*
         * Ensure ciphertext is actually used.
         */
        volatile unsigned char check =
            ciphertext[size / 2];

        (void)check;

        free(ciphertext);
        ciphertext = NULL;
    }

    // Generate ciphertext once for decryption.

    ciphertext =
        encrypt_large(
            plaintext,
            size,
            key,
            strlen((char *)key),
            nonce
        );

    // Decryption

    for (int run = 0; run < RUNS; run++) {

        double start = now_seconds();

        unsigned char *decrypted =
            decrypt_large(
                ciphertext,
                size,
                key,
                strlen((char *)key),
                nonce
            );

        double end = now_seconds();

        dec_times[run] =
            end - start;

        /*
         * Correctness verification.
         */
        if (memcmp(
                plaintext,
                decrypted,
                size
            ) != 0) {

            fprintf(
                stderr,
                "ERROR: RCEX decryption verification failed\n"
            );

            free(decrypted);
            free(ciphertext);
            free(plaintext);

            exit(EXIT_FAILURE);
        }

        free(decrypted);
    }

    free(ciphertext);

    // Stats

    double enc_sum = 0.0;
    double dec_sum = 0.0;

    double enc_min = enc_times[0];
    double dec_min = dec_times[0];

    double enc_max = enc_times[0];
    double dec_max = dec_times[0];

    for (int i = 0; i < RUNS; i++) {

        enc_sum += enc_times[i];
        dec_sum += dec_times[i];

        if (enc_times[i] < enc_min)
            enc_min = enc_times[i];

        if (enc_times[i] > enc_max)
            enc_max = enc_times[i];

        if (dec_times[i] < dec_min)
            dec_min = dec_times[i];

        if (dec_times[i] > dec_max)
            dec_max = dec_times[i];
    }

    double enc_avg =
        enc_sum / RUNS;

    double dec_avg =
        dec_sum / RUNS;

    printf("\nEncryption:\n");
    printf("  Average: %.6f s\n", enc_avg);
    printf("  Minimum: %.6f s\n", enc_min);
    printf("  Maximum: %.6f s\n", enc_max);
    printf("  Throughput: %.2f MB/s\n",
           ((double)size / MB) / enc_avg);

    printf("\nDecryption:\n");
    printf("  Average: %.6f s\n", dec_avg);
    printf("  Minimum: %.6f s\n", dec_min);
    printf("  Maximum: %.6f s\n", dec_max);
    printf("  Throughput: %.2f MB/s\n",
           ((double)size / MB) / dec_avg);

    free(plaintext);
}

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void) {

    const size_t sizes[] = {
        1 * MB,
        5 * MB,
        10 * MB,
        25 * MB,
        50 * MB
    };

    printf("\n");
    printf(" RCEX PERFORMANCE BENCHMARK\n");
    printf("\n");

    printf("\nRCEX chunk size: %d bytes\n",
           RCEX_CHUNK);

    printf("Runs per test: %d\n",
           RUNS);

    printf("\nNOTE:\n");
    printf("RCEX API accepts int16 lengths, so large inputs\n");
    printf("are processed in <=65535-byte chunks.\n");

    for (int i = 0; i < 5; i++)
        benchmark_size(sizes[i]);

    printf("\n");
    printf(" RCEX BENCHMARK COMPLETE\n");
    printf("\n");

    return 0;
}