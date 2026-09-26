#define _GNU_SOURCE

#include "../ASTE.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define RUNS 10
#define MB (1024ULL * 1024ULL)

extern struct editorConfig E;

// Timer

static double now_seconds(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    return (double)ts.tv_sec +
           (double)ts.tv_nsec / 1000000000.0;
}

// File util
static size_t file_size(const char *filename) {
    struct stat st;

    if (stat(filename, &st) != 0) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    return (size_t)st.st_size;
}

/*
 * Generate a deterministic benchmark file.

 * Most lines contain function_123.
 * Special markers are inserted at:
 *
 *   ~25%  -> function_2500
 *   ~50%  -> function_5000
 *   ~75%  -> function_7500
 *   ~100% -> function_9999
 *
 * This gives us actual strings at different locations
 * for search benchmarking.
 */

static void generate_file( const char *filename, size_t target_bytes){
    FILE *fp = fopen(filename, "w");
    if (!fp){
        perror(filename);
        exit(EXIT_FAILURE);
    }

    const char *normal_line =
        "int function_123(int value) { "
        "if (value > 100) return value * 2; "
        "return value + 1; }\n";

    const char *middle_line =
        "int function_5000(int value) { "
        "if (value > 100) return value * 2; "
        "return value + 1; }\n";

    const char *late_line =
        "int function_7500(int value) { "
        "if (value > 100) return value * 2; "
        "return value + 1; }\n";

    const char *last_line =
        "int function_9999(int value) { "
        "if (value > 100) return value * 2; "
        "return value + 1; }\n";

    size_t normal_len = strlen(normal_line);
    size_t middle_len = strlen(middle_line);
    size_t late_len = strlen(late_line);
    size_t last_len = strlen(last_line);

    size_t written = 0;
    size_t line_number = 0;

    /*
     * Write approximately target_bytes of data.
     */
    while (written < target_bytes) {

        const char *line = normal_line;
        size_t line_len = normal_len;

        /*
         * Insert unique search targets at controlled positions.
         *
         * Every 4th quarter gets a different marker.
         */
        size_t estimated_lines =
            target_bytes / normal_len;

        if (estimated_lines > 0) {

            size_t quarter1 = estimated_lines / 4;
            size_t quarter2 = estimated_lines / 2;
            size_t quarter3 = (estimated_lines * 3) / 4;
            size_t final_line = estimated_lines - 1;

            if (line_number == quarter1) {
                line = "int function_2500(int value) { "
                       "if (value > 100) return value * 2; "
                       "return value + 1; }\n";

                line_len = strlen(line);
            }
            else if (line_number == quarter2) {
                line = middle_line;
                line_len = middle_len;
            }
            else if (line_number == quarter3) {
                line = late_line;
                line_len = late_len;
            }
            else if (line_number == final_line) {
                line = last_line;
                line_len = last_len;
            }
        }

        size_t remaining = target_bytes - written;

        if (line_len <= remaining) {
            fwrite(line, 1, line_len, fp);
            written += line_len;
        }
        else {
            fwrite(line, 1, remaining, fp);
            written += remaining;
        }

        line_number++;
    }

    fclose(fp);
}

static void generate_test_files(void) {

    const size_t sizes[] = {
        1ULL * MB,
        5ULL * MB,
        10ULL * MB,
        25ULL * MB,
        50ULL * MB
    };

    char filename[128];

    printf("Generating benchmark files...\n");

    for (size_t i = 0; i < 5; i++) {

        snprintf(
            filename,
            sizeof(filename),
            "test_%lluMB.txt",
            (unsigned long long)(sizes[i] / MB)
        );

        generate_file(filename, sizes[i]);

        printf(
            "  %-20s %llu MB\n",
            filename,
            (unsigned long long)(file_size(filename) / MB)
        );
    }

    printf("\n");
}

// ASTE state rest

static void reset_editor(void) {

    edFreeAllRows();

    free(E.filename);
    E.filename = NULL;

    E.syntax = NULL;

    E.curx = 0;
    E.cury = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.dirty = 0;
}

// Stats

static void print_stats(
    const double *times,
    int count,
    size_t bytes
) {

    double sum = 0.0;
    double min = times[0];
    double max = times[0];

    for (int i = 0; i < count; i++) {

        sum += times[i];

        if (times[i] < min)
            min = times[i];

        if (times[i] > max)
            max = times[i];
    }

    double avg = sum / count;

    double throughput =
        ((double)bytes / (double)MB) / avg;

    printf("  Average : %.6f s\n", avg);
    printf("  Minimum : %.6f s\n", min);
    printf("  Maximum : %.6f s\n", max);
    printf("  Throughput: %.2f MB/s\n", throughput);

    printf("  Runs: ");

    for (int i = 0; i < count; i++) {

        printf("%.6f", times[i]);

        if (i != count - 1)
            printf(", ");
    }

    printf("\n");
}

// ASTE file loading

static void benchmark_loading(const char *filename) {

    size_t bytes = file_size(filename);

    printf(
        "File: %s (%llu MB)\n",
        filename,
        (unsigned long long)(bytes / MB)
    );

    double times[RUNS];

    for (int run = 0; run < RUNS; run++) {

        reset_editor();

        double start = now_seconds();

        /*
         * Actual ASTE file-loading function.
         */
        edOpen((char *)filename);

        double end = now_seconds();

        times[run] = end - start;

        if (E.nrows <= 0) {

            fprintf(
                stderr,
                "ERROR: ASTE loaded zero rows\n"
            );

            exit(EXIT_FAILURE);
        }
    }

    printf(
        "  Rows loaded: %d\n",
        E.nrows
    );

    print_stats(
        times,
        RUNS,
        bytes
    );

    reset_editor();

    printf("\n");
}

// ASTE save

static void prepare_editor(const char *source) {

    reset_editor();

    edOpen((char *)source);

    if (E.nrows <= 0) {

        fprintf(
            stderr,
            "ERROR: could not prepare editor buffer\n"
        );

        exit(EXIT_FAILURE);
    }
}

static void benchmark_save(const char *source) {

    char output[256];

    const char *basename =
        strrchr(source, '/');

    if (basename)
        basename++;
    else
        basename = source;

    snprintf(
        output,
        sizeof(output),
        "benchmark_saved_%s",
        basename
    );

    size_t bytes = file_size(source);

    printf(
        "File: %s (%llu MB)\n",
        source,
        (unsigned long long)(bytes / MB)
    );

    double times[RUNS];

    /*
     * Load once outside the timed region.
     */
    prepare_editor(source);

    free(E.filename);

    E.filename = strdup(output);

    if (!E.filename) {

        perror("strdup");
        exit(EXIT_FAILURE);
    }

    for (int run = 0; run < RUNS; run++) {

        /*
         * edSave() resets dirty state,
         * so restore it before every run.
         */
        E.dirty = 1;

        double start = now_seconds();

        /*
         * Actual ASTE save function.
         */
        edSave();

        double end = now_seconds();

        times[run] = end - start;
    }

    print_stats(
        times,
        RUNS,
        bytes
    );

    reset_editor();

    unlink(output);

    printf("\n");
}

// ASTE search

static void benchmark_search(const char *source) {

    size_t bytes = file_size(source);

    printf(
        "File: %s (%llu MB)\n",
        source,
        (unsigned long long)(bytes / MB)
    );

    prepare_editor(source);

    /*
     * These strings were deliberately inserted into
     * the generated benchmark file.
     *
     * function_2500 -> approximately 25%
     * function_5000 -> approximately 50%
     * function_7500 -> approximately 75%
     * function_9999 -> near the end
     */
    const char *queries[] = {
        "function_2500",
        "function_5000",
        "function_7500",
        "function_9999",
        "THIS_STRING_DOES_NOT_EXIST_999999"
    };

    const char *labels[] = {
        "search near beginning",
        "search near middle",
        "search near 75%",
        "search near end",
        "search missing string"
    };

    for (int q = 0; q < 5; q++) {

        printf(
            "  %s:\n",
            labels[q]
        );

        double times[RUNS];

        for (int run = 0; run < RUNS; run++) {

            /*
             * Start from the beginning for every test.
             *
             * This makes the amount of scanning dependent
             * on where the target actually occurs.
             */
            E.cury = 0;
            E.curx = 0;

            double start =
                now_seconds();

            /*
             * Actual ASTE search function.
             */
            edFindCallback(
                (char *)queries[q],
                'x'
            );

            double end =
                now_seconds();

            times[run] =
                end - start;

            /*
             * Force the search result to be observed.
             *
             * This prevents the compiler from treating
             * the call/result as unused.
             */
            volatile int result =
                E.cury;

            (void)result;
        }

        double sum = 0.0;
        double min = times[0];
        double max = times[0];

        for (int i = 0; i < RUNS; i++) {

            sum += times[i];

            if (times[i] < min)
                min = times[i];

            if (times[i] > max)
                max = times[i];
        }

        printf(
            "    Average: %.9f s\n",
            sum / RUNS
        );

        printf(
            "    Minimum: %.9f s\n",
            min
        );

        printf(
            "    Maximum: %.9f s\n",
            max
        );

        printf("    Runs: ");

        for (int i = 0; i < RUNS; i++) {

            printf(
                "%.9f",
                times[i]
            );

            if (i != RUNS - 1)
                printf(", ");
        }

        printf("\n\n");
    }

    reset_editor();

    printf("\n");
}

// Main

int main(void) {

    printf("\n");
    printf(" ASTE REAL PERFORMANCE BENCHMARK\n");
    printf("\n\n");

    /*
     * Always regenerate the benchmark files.
     *
     * This is important because the new search benchmark
     * requires the special function markers.
     */
    generate_test_files();

    const char *files[] = {
        "test_1MB.txt",
        "test_5MB.txt",
        "test_10MB.txt",
        "test_25MB.txt",
        "test_50MB.txt"
    };
    
    // FILE LOADING 

    printf("\n\n\nFILE LOADING — ASTE edOpen()\n\n\n");

    for (int i = 0; i < 5; i++)
        benchmark_loading(files[i]);

    // File saving

    printf("\n\n\nFILE SAVING — ASTE edSave()\n\n\n");

    for (int i = 0; i < 5; i++)
        benchmark_save(files[i]);

    // Search

    printf("\n\n\nSEARCH — ACTUAL ASTE edFindCallback()\n\n\n");

    benchmark_search(
        "test_50MB.txt"
    );

    reset_editor();

    // Complete

    printf(" BENCHMARK COMPLETE\n");

    return 0;
}