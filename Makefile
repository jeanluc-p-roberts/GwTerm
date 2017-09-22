_SRC=term.c signal.c
OBJS=$(_SRC:%.c=%.o)
CFLAGS?=-Wall
CC?=gcc

all: libgwterm.a

libgwterm.a: $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $^ -o $@

clean:
	rm -rf *.o
	rm -rf *.a
