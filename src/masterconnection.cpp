#include "masterconnection.h"

MasterConnection::MasterConnection(int socketDescriptor, QObject *parent) :
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

    //connect(this, SIGNAL(sendPacket(quint16,quint16,QVariant)), this->parent(), SLOT(sendPacket(quint16,quint16,QVariant)));

    connect(this, SIGNAL(addSlave(MasterConnection*)), this->parent(), SLOT(addSlave(MasterConnection*)));
    connect(this, SIGNAL(removeSlave(QHostAddress)), this->parent(), SLOT(removeSlave(QHostAddress)));

    connect(this, SIGNAL(addPeer(quint16,QHostAddress,bool)), this->parent(), SLOT(addPeer(quint16,QHostAddress,bool)));
    connect(this, SIGNAL(removePeer(quint16,QHostAddress,bool)), this->parent(), SLOT(removePeer(quint16,QHostAddress,bool)));



    emit addSlave(this);

    pingTimer = new QTimer(this);

    connect(pingTimer, SIGNAL(timeout()), this, SLOT(ping()));
    pingTimer->start(60000);

}


void MasterConnection::ping()
{
    QList<QVariant> data;
    data << QString("PING");
    send(data);
}


void MasterConnection::readData()
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

        // readData
        QVariant command;
        ins >> command;

        if(command == "PONG")
            qDebug() << "pong!";
        else if(command == "ADD_PEER")
        {
            // We should send to all slaves the info...
            QVariant uid;
            ins >> uid;
            emit addPeer(uid.toInt(), this->peerAddress(), false);
        }
        else if (command == "REMOVE_PEER")
        {
            // We should send to all slaves the info...
            QVariant uid;
            ins >> uid;
            emit removePeer(uid.toInt(), this->peerAddress(), false);
        }

        blocksize = 0;
    }

}

void MasterConnection::send(QList<QVariant> packet)
{
    QByteArray reply;
    QDataStream stream(&reply, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_2);

    stream << (quint32)0;

    for (int i = 0; i < packet.size(); ++i)
        stream << packet.at(i);

    stream.device()->seek(0);
    stream <<(quint32)(reply.size() - sizeof(quint32));

    if (this->write(reply) == -1)
        this->abort();
}

void MasterConnection::disconnection()
{
    emit removeSlave(this->peerAddress());
    pingTimer->stop();
    pingTimer->deleteLater();
    deleteLater();
}
