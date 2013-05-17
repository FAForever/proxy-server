#include "proxyserver.h"
#include "proxyconnection.h"

Server::Server(QObject* parent): QTcpServer(parent)
{

    if (!listen(QHostAddress::Any, 9123))
        qDebug("Unable to start the server");

}


void Server::incomingConnection( int socketDescriptor )
{
    qDebug("incoming connection");
    ProxyConnection *connection    = new ProxyConnection( this );
    connection->setSocketDescriptor(socketDescriptor);
    emit newConnection(connection);
}


void Server::sendPacket(QString address, quint16 port, QVariant packet)
{
    qDebug("send to peer");
    QMap<QString, ProxyConnection*>::const_iterator socket = peers.find(address);
    while (socket != peers.end() && socket.key() == address)
    {
        socket.value()->send(port, packet);
        ++socket;
    }

}

void Server::addPeer(QString address, ProxyConnection *socket)
{
    qDebug("adding peer");
    peers.insert(address, socket);
}

void Server::removePeer(QString address)
{
    qDebug("remove peer");
    peers.remove(address);

}
