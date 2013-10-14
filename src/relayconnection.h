#ifndef RELAYCONNECTION_H
#define RELAYCONNECTION_H

#include <QtNetwork/QTcpSocket>

class RelayConnection : public QTcpSocket
{
    Q_OBJECT
public:
    explicit RelayConnection(int socketDescriptor, QObject *parent = 0);

private:
    quint32 blocksize;

public slots:
    void readData();
    void disconnection();

signals:
    void sendPacket(quint16 uid, quint16 port, QVariant packet);

};


#endif // RELAYCONNECTION_H
