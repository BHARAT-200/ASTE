CC     = gcc
CFLAGS = -Wall -Wextra -Werror -std=c23

SRCS   = ASTE.c RCEX_enc/rcex.c

ASTE: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o ASTE

clean:
	rm -f ASTE

run: ASTE
	./ASTE