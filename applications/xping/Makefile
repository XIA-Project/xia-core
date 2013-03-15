include ../../xia.mk

CC=g++
LDFLAGS += $(LIBS)

OUT=xping
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean: 
	-rm $(TARGETS)
