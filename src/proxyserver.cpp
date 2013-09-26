#include "proxyserver.h"
#include "proxyconnection.h"

Server::Server(QObject* parent): QTcpServer(parent)
{

    if (!listen(QHostAddress::Any, 9124))
        qDebug("Unable to start the server");

}


void Server::incomingConnection( int socketDescriptor )
{
    ProxyConnection *connection    = new ProxyConnection(socketDescriptor, this );

    emit newConnection(connection);
}


void Server::sendPacket(quint16 uid, quint16 port, QVariant packet)
{
    if(peers.contains(uid))
        peers.value(uid)->send(port, packet);
}

void Server::addPeer(quint16 uid, ProxyConnection *socket)
{
    peers.insert(uid, socket);
}

void Server::removePeer(quint16 uid)
{
    peers.remove(uid);

}
