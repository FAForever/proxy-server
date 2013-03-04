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


import logging
from logging import handlers

class logger(object):
    def __init__(self):
        self.logHandler = handlers.RotatingFileHandler("proxyserver.log", backupCount=15, maxBytes=524288)
        self.logFormatter = logging.Formatter('%(asctime)s %(levelname)-8s %(name)-20s %(message)s')
        self.logHandler.setFormatter( self.logFormatter )  
        
        self.logger = logging.getLogger('server.logging')
        
        self.logger.addHandler( self.logHandler )
        self.logger.setLevel( logging.DEBUG )
        self.logger.propagate = True
        
        self.logger.debug("Start logger")
        
        
    def getHandler(self):
        return self.logHandler