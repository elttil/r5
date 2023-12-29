OBJ=main.o mmu.o
CFLAGS=-std=c99 -g -Wall -Wextra -pedantic -Werror -lubsan -lasan

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

r5: $(OBJ)
	$(CC) -lubsan -lasan $(LDFLAGS) $^ -o $@

clean:
	rm r5 $(OBJ)
