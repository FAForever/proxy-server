#ifndef MASTERSERVER_H
#define MASTERSERVER_H


#include <QtNetwork/QTcpServer>
#include "masterconnection.h"

class masterserver : public QTcpServer
{
    Q_OBJECT
public:
    explicit masterserver(QObject *parent = 0);

private:
    QHash<QHostAddress, MasterConnection*> slaves;
    QHash<quint16, QHostAddress> peerBook;

signals:
    void newConnection(MasterConnection *connection);

protected:
    void incomingConnection(int socketDescriptor);

public slots:

    void addSlave(MasterConnection* socket);
    void removeSlave(QHostAddress address);
    void addPeer(quint16 uid, QHostAddress address);
    void removePeer(quint16 uid);
};


#endif // MASTERSERVER_H
