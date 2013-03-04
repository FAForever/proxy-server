#-------------------------------------------------------------------------------
# Copyright (c) 2012 Gael Honorez.
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the GNU Public License v3.0
# which accompanies this distribution, and is available at
# http://www.gnu.org/licenses/gpl.html
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#-------------------------------------------------------------------------------


from PySide import QtCore, QtNetwork
from types import IntType, FloatType, ListType, DictType
import json


import proxylogger
loggerInstance = proxylogger.instance
import logging

class fullAddress(object):
    '''
    Save the address and port of a client.
    '''
    def __init(self, address, port):
        self.address    = QtNetwork.QHostAddress(address)
        self.port       = int(port)

class ClientModule(QtCore.QObject):
    def __init__(self, socket, parent=None):
        
        super(ClientModule, self).__init__(parent)

        self.log = logging.getLogger('proxyserver.clientHandler')
        self.log.setLevel( logging.DEBUG )
        self.log.addHandler(loggerInstance.getHandler())
        
        self.parent = parent
        
        self.socket = QtNetwork.QTcpSocket(self)
        self.socket.setSocketDescriptor(socket)

            
        self.ip = self.socket.peerAddress().toString()
        self.port = self.socket.peerPort()
        self.peerName = self.socket.peerName()
          
        self.proxies = []
        
        
        self.socket.readyRead.connect(self.readDatas)
        self.socket.disconnected.connect(self.disconnection)
        self.socket.error.connect(self.displayError)
        self.blockSize = 0        

    def command_connect_to(self, message):
        '''The client is asking for the permission to connect to someone'''
        
        who     = message["who"]
        ip      = message["ip"]
        port    = message["port"]
        
        # let's attributte a UDP socket for that client and destination

        
        numProxy = None
        
        # first, check the first available port.
        for i in range(11) :
            if not i in self.proxies :
                self.proxies.append(i)
                numProxy = i
                break
            
        if numProxy :
            self.parent.parent.proxiesDestination[numProxy][self.ip] = fullAddress(ip, port)
        
            if not who in self.connectionList :
                self.connectionList[who] = 
        
        
        

    def sendJSON(self, data_dictionary):
        '''
        Simply dumps a dictionary into a string and feeds it into the QTCPSocket
        '''
        data_string = ""
        try :           
            data_string = json.dumps(data_dictionary)
        except :
            return
        self.sendReply(data_string)


    def receiveJSON(self, data_string, stream):
        '''
        A fairly pythonic way to process received strings as JSON messages.
        '''
        message = json.loads(data_string)
        cmd = "command_" + message['command']
        if hasattr(self, cmd):
                getattr(self, cmd)(message)  
        else:
            self.log.error("command unknown : %s", cmd)
  

    def sendReply(self, action, *args, **kwargs) :
        if self in self.parent.recorders :
            if self.socket.bytesToWrite() > 16 * 1024 * 1024 :
                self.log.error("too many to write already")
                self.socket.abort()
                return 
            reply = QtCore.QByteArray()
            stream = QtCore.QDataStream(reply, QtCore.QIODevice.WriteOnly)
            stream.setVersion(QtCore.QDataStream.Qt_4_2)
            stream.writeUInt32(0)
            stream.writeQString(action)
            for arg in args :
                if type(arg) is LongType :
                    stream.writeQString(str(arg))
                elif type(arg) is IntType:
                    stream.writeInt(arg)
                elif isinstance(arg, basestring):                       
                    stream.writeQString(arg)                  
                elif type(arg) is StringType  :
                    stream.writeQString(arg)
                elif type(arg) is FloatType:
                    stream.writeFloat(arg)
                elif type(arg) is ListType:
                    stream.writeQString(str(arg))
            stream.device().seek(0)         
            stream.writeUInt32(reply.size() - 4)
            if self.socket.write(reply) == -1 :
                self.socket.abort()

    def disconnection(self):  
        self.done()
        
    def done(self):
        if self in self.parent.connected :
            if self.socket != None :
                self.socket.readyRead.disconnect(self.readDatas)
                self.socket.disconnected.disconnect(self.disconnection)
                self.socket.error.disconnect(self.displayError)
                self.socket.close()
            self.parent.removeConnected(self)

    def displayError(self, socketError):
        if socketError == QtNetwork.QAbstractSocket.RemoteHostClosedError:
            self.log.warning("RemoteHostClosedError")
        elif socketError == QtNetwork.QAbstractSocket.HostNotFoundError:
            self.log.warning("HostNotFoundError")
        elif socketError == QtNetwork.QAbstractSocket.ConnectionRefusedError:
            self.log.warning("ConnectionRefusedError")
        else:
            self.log.warning("The following Error occurred: %s." % self.socket.errorString())