#include "proxyserver.h"
#include "connectionhandler.h"
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

    qDebug() << "ProxyServer started, listening on port" << port;
    return true;
}

void ProxyServer::stopServer()
{
    if (isListening()) {
        close();
        qDebug() << "Server stopped listening";
    }

    qDeleteAll(m_handlers.keys());
    m_handlers.clear();
}

void ProxyServer::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "New connection from proxyClient, socket descriptor:" << socketDescriptor;

    ConnectionHandler *handler = new ConnectionHandler(this);
    handler->setClientSocketDescriptor(socketDescriptor);

    connect(handler, &ConnectionHandler::disconnected, this, &ProxyServer::onHandlerDisconnected);

    m_handlers[handler] = handler;
}

void ProxyServer::onHandlerDisconnected()
{
    ConnectionHandler *handler = qobject_cast<ConnectionHandler*>(sender());
    if (handler) {
        qDebug() << "Connection handler disconnected";
        m_handlers.remove(handler);
        handler->deleteLater();
    }
}
