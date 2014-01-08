#include <QtCore/QCoreApplication>
#include <QStringList>

#include "proxyserver.h"


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server server;

    QStringList args = a.arguments();
    for (int i = 0; i < args.size(); ++i)
        if (QString(args.at(i)) == QString("-slave"))
        {
            i++;
            bool enslaved = server.setSlave(QString(args.at(i)));
            if(enslaved)
                qDebug("start as slave");

        }

    if(!server.isSlave())
        server.setMaster();

    return a.exec();
}
