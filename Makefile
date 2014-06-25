CFLAGS=-std=c99 -Wall -Wextra -Werror

LIBS=-lpulse -lX11 -lXtst

headphone-event: main.c
	gcc $(CFLAGS) $(LIBS) $< -o $@
