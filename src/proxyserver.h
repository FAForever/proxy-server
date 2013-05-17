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
    void addPeer(QString address, ProxyConnection* socket);
    void removePeer(QString address);

private:
    QMap<QString, ProxyConnection*> peers;


signals:
    void newConnection(ProxyConnection *connection);

public slots:
    void sendPacket(QString address, quint16 port, QVariant packet);

protected:
    void incomingConnection(int socketDescriptor);


};

#endif // PROXYSERVER_H
