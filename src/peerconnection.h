#ifndef PEERCONNECTION_H
#define PEERCONNECTION_H

#include <QObject>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>


class PeerConnection : public QTcpSocket
{
    Q_OBJECT
public:
    explicit PeerConnection(QObject *parent = 0);

public:
    void send(quint16 uid, quint16 port, QVariant packet);

private:
    quint32 blocksize;

public slots:
    void readData();
    void disconnection();

signals:
    void removeRelay(QHostAddress address);

};
#endif // PEERCONNECTION_H
