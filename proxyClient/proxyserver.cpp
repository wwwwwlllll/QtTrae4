#include "proxyserver.h"
#include "tunnel.h"
#include <QDebug>

ProxyServer::ProxyServer(QObject *parent)
    : QTcpServer(parent)
{
}

ProxyServer::~ProxyServer()
{
    stopServer();
}

bool ProxyServer::startServer(quint16 port)
{
    if (isListening()) {
        qWarning() << "Server is already listening";
        return false;
    }

    if (!listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to start server on port" << port << ":" << errorString();
        return false;
    }

    qDebug() << "ProxyClient started, listening on port" << port;
    return true;
}

void ProxyServer::stopServer()
{
    if (isListening()) {
        close();
        qDebug() << "Server stopped listening";
    }

    qDeleteAll(m_tunnels.keys());
    m_tunnels.clear();
}

void ProxyServer::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "New connection from browser, socket descriptor:" << socketDescriptor;

    Tunnel *tunnel = new Tunnel(this);
    tunnel->setBrowserSocketDescriptor(socketDescriptor);
    tunnel->connectToProxyServer("127.0.0.1", 8901);

    connect(tunnel, &Tunnel::disconnected, this, &ProxyServer::onTunnelDisconnected);

    m_tunnels[tunnel] = tunnel;
}

void ProxyServer::onTunnelDisconnected()
{
    Tunnel *tunnel = qobject_cast<Tunnel*>(sender());
    if (tunnel) {
        qDebug() << "Tunnel disconnected";
        m_tunnels.remove(tunnel);
        tunnel->deleteLater();
    }
}
