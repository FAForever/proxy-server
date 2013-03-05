
from PySide import QtNetwork

class fullAddress(object):
    '''
    Save the address and port of a client.
    '''
    def __init__(self, address, port):
        self.address    = QtNetwork.QHostAddress(address)
        self.port       = int(port)