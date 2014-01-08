#include "relayconnection.h"

RelayConnection::RelayConnection(int socketDescriptor, QObject *parent) :
    QTcpSocket(parent)
{

    blocksize = 0;

    if (this->setSocketDescriptor(socketDescriptor))
        qDebug("socket set");
    else
        qDebug("socket failed");

    this->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(this, SIGNAL(readyRead()),this,SLOT(readData()));
    connect(this, SIGNAL(disconnected()), this, SLOT(disconnection()));

    connect(this, SIGNAL(sendPacket(quint16,quint16,QVariant)), this->parent(), SLOT(sendPacket(quint16,quint16,QVariant)));

}

void RelayConnection::readData()
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

        quint16 uid;
        quint16 port;
        QVariant packet;
        ins >> uid;
        ins >> port;
        ins >> packet;

        emit sendPacket(uid, port, packet);


        blocksize = 0;
    }

}

void RelayConnection::disconnection()
{
    deleteLater();
}
