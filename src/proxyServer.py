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
from address import address
import json


UNIT16 = 8

class start(QtCore.QObject):

    def __init__(self, parent=None):

        super(start, self).__init__(parent)

        self.log = logging.getLogger('proxyserver.main')
        self.log.setLevel( logging.DEBUG )
        self.log.addHandler(loggerInstance.getHandler())
    

        self.proxies = {}
        self.proxiesDestination = {}
        self.proxiesByUser = {}
        self.pairConnections = {}
        
        self.fixingPort = {}
        
        self.forwardPacket = {}
        
        for i in range(11) :
            self.proxies[i] = QtNetwork.QUdpSocket(self)
            if not self.proxies[i].bind(12001 + i) :
                self.log.warn("Can't bind socket %i" % i)
            else :
                self.log.info("binding socket %i" % i)
                self.proxies[i].readyRead.connect(functools.partial(self.processPendingDatagrams, i))
                self.proxiesDestination[i] = {}
                self.fixingPort[i] = {}
                self.forwardPacket[i] = {}
            
            
        
        
        self.connector =  QtNetwork.QUdpSocket(self)
        
        if not self.connector.bind(12000) :
            self.log.warn("Can't bind connector socket")
        else :
            self.log.info("binding connector socket")
            self.connector.readyRead.connect(self.processConnectorPendingDatagrams)

    def command_cleanup(self, message):
        ''' We use this for cleaning up all infos about a connection (because it drop from game)'''
        
        self.log.debug(message)
        sourceip    = message["sourceip"]
   

        for i in range(11) :
            if sourceip in self.proxiesDestination[i] :
                del self.proxiesDestination[i][sourceip]

            if sourceip in self.fixingPort[i] :
                del self.fixingPort[i][sourceip]


        self.log.debug("cleanup done for %s" % (sourceip))

    def command_connect_to(self, message):
        '''The client is asking for the permission to connect to someone'''
        
        
        self.log.debug(message)
        
        sourceip    = message["sourceip"]
        proxyPort   = message["proxy"]
        ip          = message["ip"]
        port        = message["port"]

        self.log.debug("binding %s for %s on proxy port number %i" % (ip, sourceip, proxyPort))

        self.proxiesDestination[proxyPort][sourceip] = address.fullAddress(ip, port)

    def processConnectorPendingDatagrams(self):
        
        while self.connector.hasPendingDatagrams():
            self.log.debug("receiving UDP packet : " + str(self.connector.pendingDatagramSize()))
            datagram, _, _ = self.connector.readDatagram(self.connector.pendingDatagramSize())
            message = json.loads(str(datagram))
            cmd = "command_" + message['command']
            if hasattr(self, cmd):
                    getattr(self, cmd)(message)              
                
    def processPendingDatagrams(self, i):

        udpSocket = self.proxies[i]
        while udpSocket.hasPendingDatagrams():
            #self.log.debug("receiving UDP packet : " + str(udpSocket.pendingDatagramSize()))
            datagram, host, port = udpSocket.readDatagram(udpSocket.pendingDatagramSize())
            hostString = host.toString()
           
            if hostString in self.proxiesDestination[i] :

                
                
                destination = self.proxiesDestination[i][hostString]
                
                destport = destination.port
                
                if not destination.address.toString() in self.fixingPort[i] or "NAT_SOLVING" in str(datagram):
                    
                    #self.log.debug(datagram)
                    
                    self.proxiesDestination[i][hostString].port = port
                    destport = port
                     
                    
                    if "NAT_SOLVING" in str(datagram) :
                        self.fixingPort[i][destination.address.toString()] = port
                        udpSocket.writeDatagram("\x08PROXY_SOLVING_PACKET_RECEIVED %i" % port, host, port)
                        self.log.info("binding port %i for source %s and dest %s and sending NAT solving " %(port, hostString, destination.address.toString()))
                    else :
                        udpSocket.writeDatagram("\x08PROXY_PACKET_RECEIVED %i" % port, host, port)
                        self.log.info("binding port %i for source %s and dest %s and sending NAT" %(port, hostString, destination.address.toString()))
                
                if not "NAT" in datagram :
                    
                    if not hostString in self.forwardPacket[i] :
                        self.forwardPacket[i][hostString] = 1
                    else :
                        self.forwardPacket[i][hostString] = self.forwardPacket[i][hostString] + 1
                        
                        if self.forwardPacket[i][hostString] < 5 :
                            self.log.debug("Sending a packet to %s on proxy port number %i (%i)" % (destination.address.toString(), i, destport))
                        
                    
                    if udpSocket.writeDatagram(datagram, destination.address, destport) == -1 :
                        self.log.warn("FAILED sending a packet to %s on proxy port number %i (%i)" % (destination.address.toString(), i, destport))
                else :
                    self.log.debug("ignoring NAT packet.")


if __name__ == '__main__':
    import sys
    app = QtCore.QCoreApplication(sys.argv)
    server = start()
    app.exec_()


