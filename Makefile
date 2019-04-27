SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

CC = clang
CC_FLAGS = -MMD -c -Wall
CC_LINK_FLAGS = -O3
CC_LINK = -lpng -lz -lm -lpthread

buddhabrot: $(OBJ)
	$(CC) -o $@ $^ $(CC_LINK) $(CC_LINK_FLAGS)

-include $(OBJ:.o=.d)

%.o: %.c
	$(CC) $(CC_FLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJ) buddhabrot
