import SocketServer
import struct
import cdn_pb2

PORT = 44444
HOST = 'localhost'

class RequestHandler(SocketServer.BaseRequestHandler):
    def handle(self):
        length = struct.unpack("!L", self.request.recv(4))[0]
        print length

        try:
            msg = cdn_pb2.CDNMsg()
            msg.ParseFromString(self.request.recv(length))

            if msg.type == cdn_pb2.CDN_REQUEST_MSG:
                self.handle_request(msg)
            elif msg.type == cdn_pb2.PING_SCORES_MSG:
                self.handle_scores(msg)
            else:
                print 'unknown msg type'
        except Exception as err:
            print str(err)


    def handle_request(self, msg):
        print 'Got cluster request for', msg.client

        # calculate here!
        cluster = 'cluster0'

        print 'Returning cluster: ', cluster
        msg.request.cluster = cluster

        self.send_message(msg)



    def handle_scores(self, msg):
        print 'got scores from %s' % msg.client
        for cluster in msg.scores.clusters:
            print cluster.name, cluster.rtt, cluster.loss



    def send_message(self, msg):
        msgstr = msg.SerializeToString()
        length = len(msgstr)
        self.request.send(struct.pack("!L", length), 4)
        self.request.sendall(msgstr)


if __name__ == "__main__":
    SocketServer.TCPServer.allow_reuse_address = True
    server = SocketServer.TCPServer((HOST, PORT), RequestHandler)
    server.serve_forever()

