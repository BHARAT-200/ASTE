CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c23

ASTE: ASTE.c
	$(CC) $(CFLAGS) ASTE.c -o ASTE

clean:
	rm -f ASTE

run: ASTE
	./ASTE