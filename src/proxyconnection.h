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
    QString address;

signals:
    void sendPacket(QString address, quint16 port, QVariant packet);
    void addPeer(QString address, ProxyConnection *socket);
    void removePeer(QString address);
    
public slots:
    void readData();
    void disconnection();
    
};

#endif // PROXYCONNECTION_H
