# IRC Chat Demo

## Setup

    cd <xia-core>
    bin/xianet start

## Server
[miniircd](https://github.com/jrosdahl/miniircd)

    export PYTHONPATH="<xia-core>/api/lib"
    cd <xia-core>/applications/irc
    python miniircd/miniircd --debug

## Client
[Simple IRC Client (sic)](http://git.suckless.org/sic)

    export LD_LIBRARY_PATH="<xia-core>/api/lib"
    cd <xia-core>/applications/irc
    ./sic/sic -n <nickname> -h www_s.irc.aaa.xia

### Usage:
Join channel:

    :j #<channel>

Leave channel:

    :l #<channel>

Man page:

    man sic/sic.1
