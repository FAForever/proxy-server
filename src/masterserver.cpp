#include "masterserver.h"


masterserver::masterserver(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 9125))
        qDebug("Unable to start the master server");
    else
        qDebug() << "Master Server listening to" << this->serverAddress().toString() << "on port" << this->serverPort();


    connect(this, SIGNAL(addPeerBook(quint16,QHostAddress)), this->parent(), SLOT(addPeerBook(quint16,QHostAddress)));
    connect(this, SIGNAL(removePeerBook(quint16)), this->parent(), SLOT(removePeerBook(quint16)));
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
    // Another server has a new peer connected, we make every slave aware of it.

    qDebug() << "Adding peer" << uid << "on server" << address.toString();

    //form the packet
    QList<QVariant> data;
    data << QString("ADD_TO_PEERBOOK");
    data << uid;
    data << address.toString();

    foreach (MasterConnection* conn, slaves)
    {
        // we dont want to send it to the peer.
        if(conn->peerAddress() != address)
            conn->send(data);
    }
    // And of course to our own proxy
    emit addPeerBook(uid, address);
}

void masterserver::removePeer(quint16 uid, QHostAddress address)
{
    // Another server has a peer disconnection, we make every slave aware of it.
    qDebug() << "Removing peer" << uid ;

    //form the packet
    QList<QVariant> data;
    data << QString("REMOVE_FROM_PEERBOOK");
    data << uid;

    foreach (MasterConnection* conn, slaves)
    {
        // we dont want to send it to the peer.
        if(conn->peerAddress() != address)
            conn->send(data);
    }
    // And of course to our own proxy
    emit removePeerBook(uid);
}
