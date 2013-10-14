#ifndef MASTERCONNECTION_H
#define MASTERCONNECTION_H

#include <QObject>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

class MasterConnection : public QTcpSocket
{
    Q_OBJECT
public:
    explicit MasterConnection(int socketDescriptor, QObject *parent = 0);
    void send(QList<QVariant>);

private:
    quint32 blocksize;
    QTimer* pingTimer;


signals:
    void addSlave(MasterConnection *socket);
    void removeSlave(QHostAddress address);
    void addPeer(quint16 uid, QHostAddress address);
    void removePeer(quint16 uid,QHostAddress address);

public slots:
    void ping();
    void readData();
    void disconnection();

};

#endif // MASTERCONNECTION_H
