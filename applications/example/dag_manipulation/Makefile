#TODO set this automatically?
APIDIR=../../../api
XLIB=$(APIDIR)/lib
XINC=$(APIDIR)/include

# TODO: Why isn't it working with just these?
#CXXFLAGS=-I$(XIADIR)/include -std=c++0x 
#LDFLAGS=-L$(XIADIR)/lib -ldagaddr

all: dagaddr_example

dagaddr_example:
	g++ -I$(XINC) -std=c++0x -L$(XLIB) dagaddr_example.cpp -ldagaddr -o dagaddr_example

clean:
	-rm -f dagaddr_example
