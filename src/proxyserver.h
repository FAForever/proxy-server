#ifndef PROXYSERVER_H
#define PROXYSERVER_H



#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QCoreApplication>

#include "masterserver.h"


class ProxyConnection;


class Server : public QTcpServer
{
    Q_OBJECT
public:
    Server(QObject * parent = 0);
    bool setSlave(QString master);
    bool isSlave();
    bool setMaster();

private:
    QHash<quint16, ProxyConnection*> peers;
    QHostAddress master;
    masterserver* masterServer;
    bool enslaver;
    QTcpSocket* masterConnection;
    quint32 blocksize;


signals:
    void newConnection(ProxyConnection *connection);

public slots:
    void sendPacket(quint16 uid, quint16 port, QVariant packet);
    void addPeer(quint16 uid, ProxyConnection* socket);
    void removePeer(quint16 uid);

    void readDataFromMaster();
    void disconnectedFromMaster();

protected:
    void incomingConnection(int socketDescriptor);


};

#endif // PROXYSERVER_H
