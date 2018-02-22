import SocketServer
import struct
import cdn_pb2


class RequestHandler(SocketServer.BaseRequestHandler):
    def handle(self):
        # get the length of the msg
        length = struct.unpack("H", self.request.recv(2))[0]

        try:
            msg = cdn_pb2.CDNMsg()
            msg.ParseFromString(self.request.recv(length))

            if msg.type == cdn_pb2.CDN_REQUEST_MSG:
                self.handle_request(msg)
            elif msg.type == cdn_pb2.PING_SCORES_MSG:
                self.handle_scores(msg)
            else:
                print 'unknown msg type'
        except:
            print 'boom!'



    def handle_request(self, msg):
        print 'Got cluster request for', msg.client

        # calculate here!
        cluster = 'cluster0'

        print 'Returning cluster: ', cluster
        msg.request.cluster = cluster

        print 'sending'
        self.send_message(msg)



    def handle_scores(self, msg):
        print 'got scores from %s' % msg.client
        for cluster in msg.scores.clusters:
            print cluster.name, cluster.rtt, cluster.loss



    def send_message(self, msg):
        msgstr = msg.SerializeToString()
        length = len(msgstr)
        self.request.send(struct.pack("H", length), 2)
        self.request.sendall(msgstr)


if __name__ == "__main__":
    HOST, PORT = "localhost", 9999
    SocketServer.TCPServer.allow_reuse_address = True
    server = SocketServer.TCPServer((HOST, PORT), RequestHandler)
    server.serve_forever()

