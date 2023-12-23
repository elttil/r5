OBJ=main.o
CFLAGS=-std=c99 -g -Wall -Wextra -pedantic -Werror -lubsan -lasan -DDEBUG

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

r5: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ -lubsan -lasan

clean:
	rm r5 $(OBJ)
