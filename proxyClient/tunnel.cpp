#include "tunnel.h"
#include <QDebug>

Tunnel::Tunnel(QObject *parent)
    : QObject(parent)
    , m_browserSocket(nullptr)
    , m_proxyServerSocket(nullptr)
    , m_browserSocketDescriptor(-1)
    , m_proxyServerPort(0)
    , m_proxyServerConnected(false)
{
}

Tunnel::~Tunnel()
{
    closeConnection();
}

void Tunnel::setBrowserSocketDescriptor(qintptr socketDescriptor)
{
    m_browserSocketDescriptor = socketDescriptor;

    m_browserSocket = new QTcpSocket(this);
    connect(m_browserSocket, &QTcpSocket::readyRead, this, &Tunnel::onBrowserReadyRead);
    connect(m_browserSocket, &QTcpSocket::disconnected, this, &Tunnel::onBrowserDisconnected);
    connect(m_browserSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &Tunnel::onBrowserError);

    if (!m_browserSocket->setSocketDescriptor(m_browserSocketDescriptor)) {
        qCritical() << "Failed to set socket descriptor for browser socket:" << m_browserSocket->errorString();
        closeConnection();
        return;
    }

    qDebug() << "Browser socket connected, descriptor:" << m_browserSocketDescriptor;
}

void Tunnel::connectToProxyServer(const QString &host, quint16 port)
{
    m_proxyServerHost = host;
    m_proxyServerPort = port;

    m_proxyServerSocket = new QTcpSocket(this);
    connect(m_proxyServerSocket, &QTcpSocket::connected, this, &Tunnel::onProxyServerConnected);
    connect(m_proxyServerSocket, &QTcpSocket::readyRead, this, &Tunnel::onProxyServerReadyRead);
    connect(m_proxyServerSocket, &QTcpSocket::disconnected, this, &Tunnel::onProxyServerDisconnected);
    connect(m_proxyServerSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &Tunnel::onProxyServerError);

    qDebug() << "Connecting to proxy server at" << host << ":" << port;
    m_proxyServerSocket->connectToHost(host, port);
}

void Tunnel::onBrowserReadyRead()
{
    if (!m_browserSocket) return;

    QByteArray data = m_browserSocket->readAll();
    qDebug() << "Received" << data.size() << "bytes from browser";

    if (m_proxyServerConnected && m_proxyServerSocket 
        && m_proxyServerSocket->state() == QAbstractSocket::ConnectedState) {
        if (!m_browserBuffer.isEmpty()) {
            m_proxyServerSocket->write(m_browserBuffer);
            m_browserBuffer.clear();
        }
        m_proxyServerSocket->write(data);
    } else {
        m_browserBuffer.append(data);
        qDebug() << "Buffered" << m_browserBuffer.size() << "bytes (proxy server not connected yet)";
    }
}

void Tunnel::onBrowserDisconnected()
{
    qDebug() << "Browser disconnected";
    closeConnection();
}

void Tunnel::onBrowserError(QAbstractSocket::SocketError error)
{
    qCritical() << "Browser socket error:" << error << "-" << m_browserSocket->errorString();
    closeConnection();
}

void Tunnel::onProxyServerConnected()
{
    qDebug() << "Connected to proxy server";
    m_proxyServerConnected = true;

    flushBuffer(m_proxyServerSocket);
}

void Tunnel::onProxyServerReadyRead()
{
    if (!m_proxyServerSocket || !m_browserSocket) return;

    QByteArray data = m_proxyServerSocket->readAll();
    qDebug() << "Received" << data.size() << "bytes from proxy server";

    if (m_browserSocket->state() == QAbstractSocket::ConnectedState) {
        m_browserSocket->write(data);
    }
}

void Tunnel::onProxyServerDisconnected()
{
    qDebug() << "Proxy server disconnected";
    closeConnection();
}

void Tunnel::onProxyServerError(QAbstractSocket::SocketError error)
{
    qCritical() << "Proxy server socket error:" << error << "-" << m_proxyServerSocket->errorString();
    closeConnection();
}

void Tunnel::closeConnection()
{
    if (m_browserSocket) {
        if (m_browserSocket->state() == QAbstractSocket::ConnectedState) {
            m_browserSocket->disconnectFromHost();
            if (m_browserSocket->state() != QAbstractSocket::UnconnectedState) {
                m_browserSocket->waitForDisconnected(3000);
            }
        }
        m_browserSocket->deleteLater();
        m_browserSocket = nullptr;
    }

    if (m_proxyServerSocket) {
        if (m_proxyServerSocket->state() == QAbstractSocket::ConnectedState) {
            m_proxyServerSocket->disconnectFromHost();
            if (m_proxyServerSocket->state() != QAbstractSocket::UnconnectedState) {
                m_proxyServerSocket->waitForDisconnected(3000);
            }
        }
        m_proxyServerSocket->deleteLater();
        m_proxyServerSocket = nullptr;
    }

    m_browserBuffer.clear();
    m_proxyServerConnected = false;

    emit disconnected();
}

void Tunnel::flushBuffer(QTcpSocket *to)
{
    if (!m_browserBuffer.isEmpty() && to && to->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "Flushing buffered" << m_browserBuffer.size() << "bytes";
        to->write(m_browserBuffer);
        m_browserBuffer.clear();
    }
}
