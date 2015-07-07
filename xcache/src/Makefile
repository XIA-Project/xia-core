CC=g++
OBJS = xcache.o
OBJS += controller.o
OBJS += core.o
OBJS += helpers.o
OBJS += api.o
OBJS += slice.o
OBJS += meta.o
OBJS += timer.o
OBJS += cli.o
OBJS += store_manager.o

all: $(OBJS)

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^


clean:
	rm -f xcache
	rm -f *.o
