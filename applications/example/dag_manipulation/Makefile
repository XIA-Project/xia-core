#TODO set this for real
XIADIR=../..
#XIADIR=$(dirname $(dirname $(cd ${0%/*} && echo $PWD/${0##*/})))

# TODO: Why isn't it working with just these?
#CXXFLAGS=-I$(XIADIR)/include -std=c++0x 
#LDFLAGS=-L$(XIADIR)/lib -ldagaddr

all: dagaddr_example

dagaddr_example:
	g++ -I$(XIADIR)/include -std=c++0x -L$(XIADIR)/lib dagaddr_example.cpp -ldagaddr  -o dagaddr_example

clean:
	rm -f dagaddr_example
