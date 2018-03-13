import socket
import xroute_pb2

"""Interface between Xnetjd and Xrouted

The NetjoinXrouted class is an interface that provides communication
between Xnetjd, the Network Joining Daemon and Xrouted, the Routing Daemon.

For now, it is a one-way communication from Xnetjd to Xrouted but that
may change in the near future.

All communication uses google protocol buffers defined in
    daemons/xrouted/xroute.proto

Attributes:
    _rsockfd: IP Datagram socket to send messages to xrouted on localhost
    _xrouted_addr: Localhost IP address and port for xrouted

"""
class NetjoinXrouted():
    def __init__(self):
        self._rsockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._xrouted_addr = ('127.0.0.1', 1510) # From xrouted/RouteModule.hh

    def __send_xrouted_msg(self, xrmsg):
        """Send an XrouteMsg"""
        packet = xrmsg.SerializeToString()
        self._rsockfd.sendto(packet, self._xrouted_addr)

    def __get_xrouted_msg(self):
        """Get an XrouteMsg from Xrouted"""
        (message, addr) = self._rsockfd.recvfrom(1024)
        xrmsg = xroute_pb2.XrouteMsg()
        xrmsg.ParseFromString(message)
        return xrmsg

    def __new_xrmsg(self):
        """Create a new XrouteMsg"""
        xrmsg = xroute_pb2.XrouteMsg()
        xrmsg.version = xroute_pb2.XROUTE_PROTO_VERSION
        return xrmsg

    def send_host_join(self, client_hid, interface, flags):
        """Notify xrouted that a host just joined this router"""
        xrmsg = self.__new_xrmsg()
        xrmsg.type = xroute_pb2.HOST_JOIN_MSG
        xrmsg.host_join.flags = flags
        xrmsg.host_join.hid = client_hid
        xrmsg.host_join.interface = interface
        self.__send_xrouted_msg(xrmsg)

    def send_config(self, ad, controller_dag):
        """Notify Xrouted of our AD and Controller's DAG"""
        xrmsg = self.__new_xrmsg()
        xrmsg.type = xroute_pb2.CONFIG_MSG
        xrmsg.config.ad = ad
        xrmsg.config.controller_dag = controller_dag
        self.__send_xrouted_msg(xrmsg)

    def send_host_config(self, ad, hid, sid, iface):
        """Notify Xrouted of our AD, router's HID, router's SID and interface"""
        xrmsg = self.__new_xrmsg()
        xrmsg.type = xroute_pb2.HOST_CONFIG_MSG
        xrmsg.host_config.ad = ad
        xrmsg.host_config.hid = hid
        xrmsg.host_config.sid = sid
        xrmsg.host_config.iface = iface
        self.__send_xrouted_msg(xrmsg)

    def send_foreign_ad(self, interface, ad):
        """Notify Xrouted of a foreign network seen on an interface"""
        xrmsg = self.__new_xrmsg()
        xrmsg.type = xroute_pb2.FOREIGN_AD_MSG
        xrmsg.foreign.iface = interface
        xrmsg.foreign.ad = ad
        self.__send_xrouted_msg(xrmsg)

    def get_sid(self):
        """Get SID of the Xrouted process on this system"""
        xrmsg = self.__new_xrmsg()
        xrmsg.type = xroute_pb2.SID_REQUEST_MSG
        xrmsg.sid_request.SetInParent()
        self.__send_xrouted_msg(xrmsg)
        xrresp = self.__get_xrouted_msg()
        if xrresp.type != xroute_pb2.SID_REQUEST_MSG:
            return ""
        if not xrresp.HasField("sid_request"):
            return ""
        if not xrresp.sid_request.HasField("sid"):
            return ""
        return xrresp.sid_request.sid

