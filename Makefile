IDIR = include
SDIR = src
ODIR = obj
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -Wall -Werror -Wextra

_OBJS=main.o chat.o communication.o config.o linkedlist.o logging.o
OBJS=$(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean: 
	rm -f obj/*.o server
