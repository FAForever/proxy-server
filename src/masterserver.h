#ifndef MASTERSERVER_H
#define MASTERSERVER_H


#include <QtNetwork/QTcpServer>


class masterserver : public QTcpServer
{
    Q_OBJECT
public:
    explicit masterserver(QObject *parent = 0);




};

#endif // MASTERSERVER_H
