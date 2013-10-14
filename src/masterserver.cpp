#include "masterserver.h"


masterserver::masterserver(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 9125))
        qDebug("Unable to start the master server");
    else
        qDebug() << "Master Server listening to" << this->serverAddress().toString() << "on port" << this->serverPort();
}

void masterserver::incomingConnection( int socketDescriptor )
{
    MasterConnection *connection    = new MasterConnection(socketDescriptor, this );
    emit newConnection(connection);
}

void masterserver::addSlave(MasterConnection *socket)
{
    qDebug() << "Adding slave" << socket->peerAddress().toString() << "on port" << socket->peerPort();
    slaves.insert(socket->peerAddress(), socket);
}

void masterserver::removeSlave(QHostAddress address)
{
    qDebug() << "Removing slave" << address.toString();
    slaves.remove(address);

}

void masterserver::addPeer(quint16 uid, QHostAddress address)
{
    // Another server has a new peer connected, we add it in our book.
    qDebug() << "Adding peer" << uid << "on server" << address.toString();
    peerBook.insert(uid, address);
}

void masterserver::removePeer(quint16 uid)
{
    // Another server has a peer disconnection, we remove it from our book.
    qDebug() << "Removing peer" << uid ;
    peerBook.remove(uid);

}
