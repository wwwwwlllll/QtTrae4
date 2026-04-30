#include <QCoreApplication>
#include <QDebug>
#include "proxyserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "========================================";
    qDebug() << "      Proxy Server Starting...";
    qDebug() << "========================================";

    ProxyServer proxyServer;
    
    if (!proxyServer.startServer(8901)) {
        qCritical() << "Failed to start proxy server on port 8901";
        return 1;
    }

    qDebug() << "Proxy Server running on port 8901";
    qDebug() << "Waiting for connections from Proxy Client...";
    qDebug() << "Press Ctrl+C to stop...";

    return a.exec();
}
