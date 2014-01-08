#include "peerconnection.h"



PeerConnection::PeerConnection(QObject *parent) :
    QTcpSocket(parent)
{

    blocksize = 0;

    this->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(this, SIGNAL(readyRead()),this,SLOT(readData()));
    connect(this, SIGNAL(disconnected()), this, SLOT(disconnection()));

    connect(this, SIGNAL(removeRelay(QHostAddress)), this->parent(), SLOT(removePeerConnection(QHostAddress)));


}

void PeerConnection::readData()
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

        blocksize = 0;
    }

}

void PeerConnection::send(quint16 uid, quint16 port, QVariant packet)
{
    QByteArray reply;
    QDataStream stream(&reply, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_2);

    stream << (quint32)0;
    stream << uid;
    stream << port;
    stream << packet;

    stream.device()->seek(0);
    stream <<(quint32)(reply.size() - sizeof(quint32));

    if (this->write(reply) == -1)
        this->abort();
}

void PeerConnection::disconnection()
{
    emit removeRelay(this->peerAddress());
    deleteLater();

}
