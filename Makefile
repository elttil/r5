OBJ=main.o
CFLAGS=-std=c99 -g -Wall -Wextra -pedantic -Werror -lubsan -lasan -DDEBUG

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

emu: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ -lubsan -lasan

clean:
	rm emu $(OBJ)
