import socket

def getxiaclickhostname():
    return ''.join(char for char in socket.gethostname().split('.')[0] if char.isalnum())
