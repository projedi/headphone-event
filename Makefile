CFLAGS=-std=c99 -Wall -Wextra -Werror

LIBS=-lpulse

headphone-event: main.c
	gcc $(CFLAGS) $(LIBS) $< -o $@
