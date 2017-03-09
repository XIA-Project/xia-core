include ../../xia.mk

LDFLAGS += $(LIBS)

OUT=xregister
TARGETS=$(addprefix $(BINDIR)/, $(OUT))
SOURCES=$(addsuffix .c, $(OUT))

all: $(TARGETS)

$(TARGETS): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@-rm -f $(TARGETS)
