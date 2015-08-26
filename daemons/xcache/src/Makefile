CC=g++
OBJS = xcache.o
OBJS += controller.o
OBJS += slice.o
OBJS += meta.o
OBJS += store_manager.o
OBJS += logger.o
OBJS += cache.o

all: $(OBJS)

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^


clean:
	rm -f xcache
	rm -f *.o
