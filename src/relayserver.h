#ifndef RELAYSERVER_H
#define RELAYSERVER_H

#include <QtNetwork/QTcpServer>

#include "relayconnection.h"

class relayserver : public QTcpServer
{
    Q_OBJECT
public:
    explicit relayserver(QObject *parent = 0);

protected:
    void incomingConnection(int socketDescriptor);


signals:
    void newConnection(RelayConnection *connection);

};


#endif // RELAYSERVER_H
