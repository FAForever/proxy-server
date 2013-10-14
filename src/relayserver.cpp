#include "relayserver.h"


relayserver::relayserver(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 9126))
        qDebug("Unable to start the relay server");
    else
        qDebug() << "relay Server listening to" << this->serverAddress().toString() << "on port" << this->serverPort();
}

void relayserver::incomingConnection( int socketDescriptor )
{
    RelayConnection *connection    = new RelayConnection(socketDescriptor, this );
    emit newConnection(connection);
}
