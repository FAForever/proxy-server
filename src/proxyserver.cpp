#include "proxyserver.h"
#include "proxyconnection.h"


Server::Server(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 0))
        qDebug("Unable to start the server");
        qDebug() << "Proxy Server Listening to" << this->serverAddress().toString() << "on port" << this->serverPort();

    enslaver = false;
    blocksize = 0;
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

bool Server::setSlave(QString masterAddress)
{

    master = QHostAddress(masterAddress);

    if(!master.isNull())
        qDebug() << "master is :" << masterAddress;
    masterConnection = new QTcpSocket(this);
    masterConnection->connectToHost(master, 9125);
    while (masterConnection->state() != QAbstractSocket::ConnectedState)
    {
        if (masterConnection->state() != QAbstractSocket::ConnectingState)
        {
            qDebug("connecting to master ...");
            masterConnection->connectToHost(master, 9125);
        }
        QCoreApplication::processEvents();
    }

    connect(masterConnection, SIGNAL(readyRead()),this,SLOT(readDataFromMaster()));
    connect(masterConnection, SIGNAL(disconnected()),this,SLOT(disconnectedFromMaster()));


    return !master.isNull();

}

bool Server::isSlave()
{
    return !master.isNull();
}

bool Server::setMaster()
{
    masterServer = new masterserver(this);
    enslaver = true;
    return enslaver;
}

void Server::readDataFromMaster()
{
    QDataStream ins(masterConnection);
    ins.setVersion(QDataStream::Qt_4_2);

    while (ins.atEnd() == false)
    {
        if (blocksize == 0)
        {

            if (masterConnection->bytesAvailable() < (int)sizeof(quint32))
                return;


            ins >> (quint32&) blocksize;

        }
        if (masterConnection->bytesAvailable() < blocksize)
            return;

        // read data here


        blocksize = 0;
    }
}

void Server::disconnectedFromMaster()
{
    // we should reconnect
    masterConnection->connectToHost(master, 9125);
    while (masterConnection->state() != QAbstractSocket::ConnectedState)
    {
        if (masterConnection->state() != QAbstractSocket::ConnectingState)
        {
            qDebug("Re-connecting to master ...");
            masterConnection->connectToHost(master, 9125);
        }
         QCoreApplication::processEvents();

    }
    qDebug("reconnected to master");
}
