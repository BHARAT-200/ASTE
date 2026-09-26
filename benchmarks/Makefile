CC = clang
CFLAGS = -O2 -Wall -Wextra -std=c23

all: benchmark benchmark_rcex

# Build ASTE.c as a library-like object, renaming its main()
aste_benchmark_impl.o: ../ASTE.c
	$(CC) $(CFLAGS) -Dmain=aste_original_main -c ../ASTE.c -o aste_benchmark_impl.o

# ASTE benchmark + actual RCEX implementation
benchmark: benchmark.c aste_benchmark_impl.o ../RCEX_enc/rcex.c
	$(CC) $(CFLAGS) benchmark.c aste_benchmark_impl.o ../RCEX_enc/rcex.c -o benchmark

# Standalone RCEX benchmark
benchmark_rcex: benchmark_rcex.c ../RCEX_enc/rcex.c
	$(CC) $(CFLAGS) benchmark_rcex.c ../RCEX_enc/rcex.c -o benchmark_rcex

clean:
	rm -f benchmark benchmark_rcex aste_benchmark_impl.o
	rm -f test_*.txt save_test_*.txt

.PHONY: all clean