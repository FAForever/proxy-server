
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

    peerAddress = QString(this->peerAddress().toString());



    connect(this, SIGNAL(readyRead()),this,SLOT(readData()));
    connect(this, SIGNAL(disconnected()), this, SLOT(disconnection()));

    connect(this, SIGNAL(sendPacket(QString,quint16,QVariant)), this->parent(), SLOT(sendPacket(QString,quint16,QVariant)));

    connect(this, SIGNAL(addPeer(QString,ProxyConnection*)), this->parent(), SLOT(addPeer(QString,ProxyConnection*)));
    connect(this, SIGNAL(removePeer(QString)), this->parent(), SLOT(removePeer(QString)));

    emit addPeer(peerAddress, this);

}

void ProxyConnection::readData()
{
    qDebug("reading data");
    QDataStream ins(this);
    ins.setVersion(QDataStream::Qt_4_2);

    while (ins.atEnd() == false)
    {
        qDebug("looping over data");
        if (blocksize == 0)
        {
             qDebug("No blocksize.");
            if (this->bytesAvailable() < (int)sizeof(quint32))
            {
                qDebug("Not enough data.");
                return;

            }

            ins >> (quint32&) blocksize;
            qDebug("blocksize : %i", blocksize);
        }
        if (this->bytesAvailable() < blocksize)
        {
            qDebug("Not enough data for this blocksize.");
            return;
        }

        quint16 port;
        QString address;
        QVariant packet;

        ins >> port;
        ins >> address;
        ins >> packet;

        qDebug("send packet to..");
        emit sendPacket(address, port, packet);
        blocksize = 0;
    }

}

void ProxyConnection::send(quint16 port, QVariant packet)
{
    qDebug("send to peer on port %i", port);
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
    emit removePeer(peerAddress);
    deleteLater();
}
