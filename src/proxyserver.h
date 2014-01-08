#ifndef PROXYSERVER_H
#define PROXYSERVER_H



#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QCoreApplication>


#include "masterserver.h"
#include "relayserver.h"
#include "peerconnection.h"

class ProxyConnection;

class Server : public QTcpServer
{
    Q_OBJECT

public:
    Server(QObject * parent = 0);
    bool setSlave(QString master);
    bool isSlave();
    bool setMaster();
    void sendDataToMaster(QList<QVariant>);

private:
    QHash<quint16, ProxyConnection*> peers;
    QHash<quint16, QHostAddress> peerBook;

    // these are for replaying info
    QHash<QHostAddress, PeerConnection*> peerConnections;

    QHostAddress master;
    masterserver* masterServer;
    relayserver* relayServer;
    bool enslaver;
    QTcpSocket* masterConnection;
    quint32 blocksize;

signals:
    void newConnection(ProxyConnection *connection);

public slots:
    void sendPacket(quint16 uid, quint16 port, QVariant packet);
    void addPeer(quint16 uid, ProxyConnection* socket);
    void removePeer(quint16 uid);

    void removePeerConnection(QHostAddress address);

    void addPeerBook(quint16 uid, QHostAddress address);
    void removePeerBook(quint16 uid);

    void readDataFromMaster();
    void disconnectedFromMaster();

protected:
    void incomingConnection(int socketDescriptor);


};

#endif // PROXYSERVER_H
