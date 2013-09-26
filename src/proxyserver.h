#ifndef PROXYSERVER_H
#define PROXYSERVER_H



#include <QtNetwork/QTcpServer>


class ProxyConnection;


class Server : public QTcpServer
{
    Q_OBJECT
public:
    Server(QObject * parent = 0);

private:
    QHash<uint, ProxyConnection*> peers;


signals:
    void newConnection(ProxyConnection *connection);

public slots:
    void sendPacket(quint16 uid, quint16 port, QVariant packet);
    void addPeer(quint16 uid, ProxyConnection* socket);
    void removePeer(quint16 uid);

protected:
    void incomingConnection(int socketDescriptor);


};

#endif // PROXYSERVER_H
