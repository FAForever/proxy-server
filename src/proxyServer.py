#!/usr/bin/env python

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

import functools

import proxylogger
loggerInstance = proxylogger.instance
import logging

import connector


UNIT16 = 8

class start(QtCore.QObject):

    def __init__(self, parent=None):

        super(start, self).__init__(parent)

        self.log = logging.getLogger('proxyserver.main')
        self.log.setLevel( logging.DEBUG )
        self.log.addHandler(loggerInstance.getHandler())
    

        self.proxies = {}
        self.proxiesDestination = {}
        
        for i in range(11) :
            self.proxies[i] = QtNetwork.QUdpSocket(self)
            self.proxies[i].bind(12000 + i)
            self.proxies[i].readyRead.connect(functools.partial(self.processPendingDatagrams, i))
            self.proxiesDestination[i] = {}
            
            
        
        
        self.connector =  connector.ConnectorServer(self)
        if not self.connector.listen(QtNetwork.QHostAddress.Any, 12001):
            #self.logger.error ("Unable to start the server: %s." % self.errorString())
            #self.close()
            return        
        else:
            self.log.info ("starting the connector server on  %s:%i" % (self.connector.serverAddress().toString(),self.connector.serverPort()))  


    def processPendingDatagrams(self, i):

        udpSocket = self.proxies[i]
        while udpSocket.hasPendingDatagrams():
            self.log.debug("receiving UDP packet : " + str(udpSocket.pendingDatagramSize()))
            datagram, host, port = udpSocket.readDatagram(udpSocket.pendingDatagramSize())
            
            if host in self.proxiesDestination[i] :
                destination = self.proxiesDestination[i][host]           
                udpSocket.writeDatagram(datagram, destination.address, destination.port)



if __name__ == '__main__':
    import sys
    app = QtCore.QCoreApplication(sys.argv)
    server = start()
    app.exec_()


