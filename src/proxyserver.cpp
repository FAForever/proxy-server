#include "proxyserver.h"

#include "proxyconnection.h"

Server::Server(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 9124))
        qDebug("Unable to start the server");
        qDebug() << "Proxy Server Listening to" << this->serverAddress().toString() << "on port" << this->serverPort();

    enslaver = false;
    blocksize = 0;

    relayServer = new relayserver(this);
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
    //if it's not a local connection, we are searching to whom we have to send the packet for relaying.
    else if (peerBook.contains(uid))
    {
        QHostAddress peerAddress = peerBook.value(uid);

        if(peerConnections.contains(peerAddress))
        {
            peerConnections.value(peerAddress)->send(uid, port, packet);

        }
        else
        {
            // We open a new connection to a fellow relay server
            PeerConnection* peerConnection = new PeerConnection(this);
            peerConnections.insert(peerAddress, peerConnection);

            peerConnection->connectToHost(peerAddress, 9126);
            while (peerConnection->state() != QAbstractSocket::ConnectedState)
            {
                if (peerConnection->state() != QAbstractSocket::ConnectingState)
                {
                    qDebug("connecting to master ...");
                    peerConnection->connectToHost(peerAddress, 9126);
                }
                QCoreApplication::processEvents();
            }
            peerConnection->send(uid, port, packet);


        }
    }
    else
    {
        qDebug() << "No peer found" << uid;
    }
}

void Server::removePeerConnection(QHostAddress address)
{
    peerConnections.remove(address);

}

void Server::addPeerBook(quint16 uid, QHostAddress address)
{
    qDebug() << "Adding peer in book:" << uid << "located on" << address.toString() ;
    peerBook.insert(uid, address);
}

void Server::removePeerBook(quint16 uid)
{
    qDebug() << "Removing peer in book:" << uid;
    peerBook.remove(uid);
}

void Server::addPeer(quint16 uid, ProxyConnection *socket)
{


    //Closing all previous connections
    if(peers.contains(uid))
        peers.value(uid)->abort();

    qDebug() << "Adding peer:" << uid;
    peers.insert(uid, socket);

    // if we are slave, we should inform the master server of that new peer.
    if(isSlave())
    {
        QList<QVariant> data;
        data << QString("ADD_PEER");
        data << uid;
        sendDataToMaster(data);
    }
    else
    {
        // If we are not a slave we are a master (duh) and should still make all others slave aware of that peer.
        masterServer->addPeer(uid, socket->peerAddress(), true);

    }
}

void Server::removePeer(quint16 uid)
{
    qDebug() << "Removing peer :" << uid;
    peers.remove(uid);

    // if we are slave, we should inform the master server of that new peer.
    if(isSlave())
    {
        QList<QVariant> data;
        data << QString("REMOVE_PEER");
        data << uid;
        sendDataToMaster(data);
    }
    else
    {
        // If we are not a slave we are a master (duh) and should still make all others slave aware of that peer.
        masterServer->removePeer(uid, this->serverAddress(), true);
    }
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
    qDebug() << "Reading from master server";
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

        QVariant command;
        ins >> command;
        qDebug() << "command:" << command;
        if(command == "PING")
        {

            QList<QVariant> data;
            data << QString("PONG");
            sendDataToMaster(data);
        }
        else if(command == "ADD_TO_PEERBOOK")
        {
            // We add it to the peer book.
            QVariant uid;
            QVariant address;
            ins >> uid;
            ins >> address;
            addPeerBook(uid.toInt(), QHostAddress(address.toString()));

        }
        else if (command == "REMOVE_FROM_PEERBOOK")
        {
            // We remove it from the book.
            QVariant uid;
            ins >> uid;
            removePeerBook(uid.toInt());
        }


        blocksize = 0;
    }
}

void Server::sendDataToMaster(QList<QVariant> packet)
{
    QByteArray reply;
    QDataStream stream(&reply, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_2);

    stream << (quint32)0;

    for (int i = 0; i < packet.size(); ++i)
        stream << packet.at(i);

    stream.device()->seek(0);
    stream <<(quint32)(reply.size() - sizeof(quint32));

    if (masterConnection->write(reply) == -1)
        masterConnection->abort();

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
