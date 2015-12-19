CC	= gcc
CFLAGS	= -Wall -g $(shell curl-config --cflags --libs) -ljansson
PROG	= geowifi_main
OBJS	= geowifi.o


all: $(OBJS) $(PROG)

%.o: %.c %.h
	$(CC) -c $^ $(CFLAGS)

%: %.c $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONLY: clean
clean:
	$(RM) $(PROG) $(OBJS) $(wildcard *.h.gch)
