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
    
        self.requestid = {}

        self.socket.readyRead.connect(self.readDatas)
        self.socket.disconnected.connect(self.disconnection)
        self.socket.error.connect(self.displayError)
        self.blockSize = 0        


    def sendJSON(self, data_dictionary, requestid = None):
        '''
        Simply dumps a dictionary into a string and feeds it into the QTCPSocket
        '''
        data_string = ""
        try :           
            if self.relay :   
                data_dictionary["relay"] = self.relay
            if requestid :
                data_dictionary["requestid"] = requestid
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
        self.relay = message.get("relay", None)
        if hasattr(self, cmd):
            check = False
            login = stream.readQString()
            session = stream.readQString()
            if cmd == "command_ask_session" :  
                getattr(self, cmd)(message)
            elif cmd != "command_hello" and cmd != "command_create_account" :               
                check = self.parent.listUsers.checkSession(login, session)
            else :
                check = True
            if check :
                getattr(self, cmd)(message)  
        else:
            self.log.error("command unknown : %s", cmd)
            login = stream.readQString()
            session = stream.readQString()       

    def sendReply(self, action, *args, **kwargs) :
        if self in self.parent.recorders :
            if self.socket.bytesToWrite() > 16 * 1024 * 1024 :
                self.log.error("too many to write already")
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
                self.noSocket = True

    def disconnection(self):
        if self.user != None :
            self.parent.listUsers.removeUser(self.user)
            self.sendToAll(self.user.userInfo())       
        self.done()
        
    def done(self):
        if self in self.parent.recorders :
            if self.pingTimer != None :
                self.pingTimer.stop()
            if self.socket != None :
                self.socket.readyRead.disconnect(self.readDatas)
                self.socket.disconnected.disconnect(self.disconnection)
                self.socket.error.disconnect(self.displayError)
                self.socket.close()
            self.parent.removeRecorder(self)

    def displayError(self, socketError):
        if socketError == QtNetwork.QAbstractSocket.RemoteHostClosedError:
            self.log.warning("RemoteHostClosedError")
        elif socketError == QtNetwork.QAbstractSocket.HostNotFoundError:
            self.log.warning("HostNotFoundError")
        elif socketError == QtNetwork.QAbstractSocket.ConnectionRefusedError:
            self.log.warning("ConnectionRefusedError")
        else:
            self.log.warning("The following Error occurred: %s." % self.socket.errorString())