OBJS = xcache.o
OBJS += controller.o
OBJS += meta.o
OBJS += store_manager.o
OBJS += logger.o
OBJS += cache.o

LIBXCACHED=libxcached.a

.PHONY: all

all: $(LIBXCACHED)

$(LIBXCACHED): $(OBJS)
	ar rcs $@ $^

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^


clean:
	rm -f *.o $(LIBXCACHED)
