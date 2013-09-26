#ifndef PROXYCONNECTION_H
#define PROXYCONNECTION_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>




class ProxyConnection : public QTcpSocket
{
    Q_OBJECT
public:
    explicit ProxyConnection(int socketDescriptor, QObject *parent = 0);
    void send(quint16 port, QVariant packet);

private:
    quint32 blocksize;
    quint16 uidUser;
    bool uidSet;

signals:
    void sendPacket(uint uid, quint16 port, QVariant packet);
    void addPeer(uint uid, ProxyConnection *socket);
    void removePeer(uint uid);
    
public slots:
    void readData();
    void disconnection();
    
};

#endif // PROXYCONNECTION_H
