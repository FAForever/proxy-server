
#include "proxyconnection.h"

ProxyConnection::ProxyConnection(int socketDescriptor, QObject *parent) :
    QTcpSocket(parent)
{

    blocksize = 0;

    if (this->setSocketDescriptor(socketDescriptor))
        qDebug("socket set");
    else
        qDebug("socket failed");

    this->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    //address = QString(this->peerAddress().toString());

    connect(this, SIGNAL(readyRead()),this,SLOT(readData()));
    connect(this, SIGNAL(disconnected()), this, SLOT(disconnection()));

    connect(this, SIGNAL(sendPacket(QString,quint16,QVariant)), this->parent(), SLOT(sendPacket(uint,quint16,QVariant)));

    connect(this, SIGNAL(addPeer(uint,ProxyConnection*)), this->parent(), SLOT(addPeer(uint,ProxyConnection*)));
    connect(this, SIGNAL(removePeer(uint)), this->parent(), SLOT(removePeer(uint)));

    //emit addPeer(address, this);
    uidSet = false;

}

void ProxyConnection::readData()
{
    QDataStream ins(this);
    ins.setVersion(QDataStream::Qt_4_2);

    while (ins.atEnd() == false)
    {
        if (blocksize == 0)
        {
            if (this->bytesAvailable() < (int)sizeof(quint32))
                return;


            ins >> (quint32&) blocksize;

        }
        if (this->bytesAvailable() < blocksize)
            return;

        if (uidSet)
        {
            quint16 port;
            quint16 uid;
            QVariant packet;

            ins >> port;
            ins >> uid;
            ins >> packet;
            emit sendPacket(uid, port, packet);
        }
        else
        {
            quint16 uid;
            ins >> uid;
            uidUser = uid;
            emit addPeer(uid, this);
            uidSet = true;

        }
        blocksize = 0;
    }

}

void ProxyConnection::send(quint16 port, QVariant packet)
{

    QByteArray reply;
    QDataStream stream(&reply, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_2);

    stream << (quint32)0;
    stream << port;
    stream << packet;

    stream.device()->seek(0);
    stream <<(quint32)(reply.size() - sizeof(quint32));

    if (this->write(reply) == -1)
        this->abort();
}

void ProxyConnection::disconnection()
{
    emit removePeer(uidUser);
    deleteLater();
}
