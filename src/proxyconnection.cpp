
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

    address = QString(this->peerAddress().toString());



    connect(this, SIGNAL(readyRead()),this,SLOT(readData()));
    connect(this, SIGNAL(disconnected()), this, SLOT(disconnection()));

    connect(this, SIGNAL(sendPacket(QString,quint16,QVariant)), this->parent(), SLOT(sendPacket(QString,quint16,QVariant)));

    connect(this, SIGNAL(addPeer(QString,ProxyConnection*)), this->parent(), SLOT(addPeer(QString,ProxyConnection*)));
    connect(this, SIGNAL(removePeer(QString)), this->parent(), SLOT(removePeer(QString)));

    emit addPeer(address, this);

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


        quint16 port;
        QString address;
        QVariant packet;

        ins >> port;
        ins >> address;
        ins >> packet;


        emit sendPacket(address, port, packet);
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

    this->write(reply);
}

void ProxyConnection::disconnection()
{
    emit removePeer(address);
    deleteLater();
}
