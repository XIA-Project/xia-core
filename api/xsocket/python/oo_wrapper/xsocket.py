import c_xsocket

XSOCK_STREAM = c_xsocket.XSOCK_STREAM

XOPT_HLIM = c_xsocket.XOPT_HLIM

class xsocket:

    def __init__(self, transport_type):
        self.type = transport_type  # TODO: make read-only?
        self._sock = c_xsocket.Xsocket(transport_type)

    def accept(self):
        if c_xsocket.Xaccept(self._sock) < 0:
            raise error('Could not accept new connection')

    def bind(self, sDAG):
        if c_xsocket.Xbind(self._sock, sDAG) < 0:
            raise error('Could not bind to %s' % sDAG)

    def close(self):
        if c_xsocket.Xclose(self._sock) < 0:
            raise error('Could not close socket')

    def connect(self, dag):
        if c_xsocket.Xconnect(self._sock, dag) < 0:
            raise error('Could not connect to %s' % dag)

    def fileno(self):
        return self._sock

    def getpeername(self):
        raise error('getpeername() has not been implemented')

    def getsockname(self):
        raise error('getsockname() has not been implemented')

    def getsockopt(self, optname, buflen=0):
        try:
            return c_xsocket.Xgetsockopt(self._sock, optname)
        except Exception, msg:
            raise error(msg)

    def listen(self, backlog):
        raise error('listen() has not been implemented')

    def makefile(self, mode=0, bufsize=0):
        raise error('listen() has not been implemented')

    def recv(self, bufsize, flags=0):
        try:
            return c_xsocket.Xrecv(self._sock, bufsize, flags)
        except IOError, msg:
            raise error(msg)

    def recvfrom(self, bufsize, flags=0):
        try:
            return c_xsocket.Xrecvfrom(self._sock, bufsize, flags)
        except IOError, msg:
            raise error(msg)

    def recvfrom_into(self, buffer, nbytes=0, flags=0):
        raise error('recvfrom_into() has not been implemented')

    def recv_into(self, buffer, nbytes=0, flags=0):
        raise error('recv_into() has not been implemented')

    def send(self, string, flags=0):
        if c_xsocket.Xsend(self._sock, string, flags) < 0:
            raise error('Could not send data')

    def sendall(self, string, flags=0):
        # TODO: should this be different from send?
        if c_xsocket.Xsend(self._sock, string, flags) < 0:
            raise error('Could not send data')

    def sendto(self, string, dDAG, flags=0):  #TODO: how to put flags before dDAG?
        if c_xsocket.Xsendto(self._sock, string, flags, dDAG) < 0:
            raise error('Could not send data to %s' % dDAG)

    def setblocking(self, flag):
        raise error('setblocking() has not been implemented')

    def settimeout(self, value):
        raise error('settimeout() has not been implemented')

    def gettimeout(self):
        raise error('gettimeout() has not been implemented')

    def setsockopt(self, optname, optval, buflen=0):
        try:
            return c_xsocket.Xsetsockopt(self._sock, optname, optval)
        except Exception, msg:
            raise error(msg)

    def shutdown(self, how):
        raise error('shutdown() has not been implemented')


class error(Exception):
    pass
