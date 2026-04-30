#include <QCoreApplication>
#include <QDebug>
#include "proxyserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "========================================";
    qDebug() << "      Proxy Client Starting...";
    qDebug() << "========================================";

    ProxyServer proxyServer;
    
    if (!proxyServer.startServer(8900)) {
        qCritical() << "Failed to start proxy client on port 8900";
        return 1;
    }

    qDebug() << "Proxy Client running on port 8900";
    qDebug() << "Connecting to Proxy Server at 127.0.0.1:8901";
    qDebug() << "Press Ctrl+C to stop...";

    return a.exec();
}
