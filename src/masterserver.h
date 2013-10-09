#ifndef MASTERSERVER_H
#define MASTERSERVER_H


#include <QtNetwork/QTcpServer>
#include "masterconnection.h"

class masterserver : public QTcpServer
{
    Q_OBJECT
public:
    explicit masterserver(QObject *parent = 0);

signals:
    void newConnection(MasterConnection *connection);

protected:
    void incomingConnection(int socketDescriptor);


};


#endif // MASTERSERVER_H
