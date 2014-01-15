include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xcontrold.cc csclient.cc ControlMessage.cc XIARouter.cc rot_parser.cc scioncryptolib.cc tinyxml2.cc
XCONTROLD=$(BINDIR)/xcontrold
LDFLAGS += $(LIBS) -lpolarssl

all: $(XCONTROLD)

$(XCONTROLD): $(SOURCES)
	-patch --follow-symlinks -N -r - --verbose < tinyxml2.patch
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)
	-patch --follow-symlinks -N -r - --verbose -R < tinyxml2.patch

clean:
	-rm $(XCONTROLD)
