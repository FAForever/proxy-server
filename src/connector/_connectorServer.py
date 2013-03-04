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

import connectorClient


class Server(QtNetwork.QTcpServer):
    def __init__(self, db, parent=None):
        super(Server, self).__init__(parent)
        
        self.log = logging.getLogger('proxyserver.main')
        
        self.log.setLevel( logging.DEBUG )
        self.log.addHandler(loggerInstance.getHandler())


        self.log.info("initialize server dispatcher")

        self.parent = parent

        self.connected = []
        

    def incomingConnection(self, socketId):
        self.log.debug("Incoming client Connection")
        reload(connectorClient) 
        self.connected.append(connectorClient.ClientModule(socketId, self))           

    def removeConnected(self, lobby):
        if lobby in self.connected:
            self.connected.remove(lobby)
            lobby.deleteLater()


