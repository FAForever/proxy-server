#include "masterserver.h"


masterserver::masterserver(QObject* parent): QTcpServer(parent)
{
    if (!listen(QHostAddress::Any, 9125))
        qDebug("Unable to start the master server");
    else
        qDebug() << "Master Server listening to" << this->serverAddress().toString() << "on port" << this->serverPort();
}

