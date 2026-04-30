#include "connectionhandler.h"
#include <QDebug>
#include <QRegularExpression>

ConnectionHandler::ConnectionHandler(QObject *parent)
    : QObject(parent)
    , m_clientSocket(nullptr)
    , m_targetSocket(nullptr)
    , m_clientSocketDescriptor(-1)
    , m_targetPort(0)
    , m_isHttps(false)
    , m_targetConnected(false)
    , m_httpsResponseSent(false)
{
}

ConnectionHandler::~ConnectionHandler()
{
    closeConnection();
}

void ConnectionHandler::setClientSocketDescriptor(qintptr socketDescriptor)
{
    m_clientSocketDescriptor = socketDescriptor;

    m_clientSocket = new QTcpSocket(this);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &ConnectionHandler::onClientReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &ConnectionHandler::onClientDisconnected);
    connect(m_clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ConnectionHandler::onClientError);

    if (!m_clientSocket->setSocketDescriptor(m_clientSocketDescriptor)) {
        qCritical() << "Failed to set socket descriptor for client socket:" << m_clientSocket->errorString();
        closeConnection();
        return;
    }

    qDebug() << "Client (proxyClient) connected, descriptor:" << m_clientSocketDescriptor;
}

void ConnectionHandler::onClientReadyRead()
{
    if (!m_clientSocket) return;

    QByteArray data = m_clientSocket->readAll();
    qDebug() << "Received" << data.size() << "bytes from proxyClient";

    if (!m_targetConnected) {
        m_clientBuffer.append(data);
        
        if (parseRequest(m_clientBuffer)) {
            qDebug() << "Parsed target:" << m_targetHost << ":" << m_targetPort << "(HTTPS:" << m_isHttps << ")";
            connectToTarget();
        }
    } else if (m_targetSocket && m_targetSocket->state() == QAbstractSocket::ConnectedState) {
        m_targetSocket->write(data);
    }
}

void ConnectionHandler::onClientDisconnected()
{
    qDebug() << "Client (proxyClient) disconnected";
    closeConnection();
}

void ConnectionHandler::onClientError(QAbstractSocket::SocketError error)
{
    qCritical() << "Client socket error:" << error << "-" << m_clientSocket->errorString();
    closeConnection();
}

void ConnectionHandler::onTargetConnected()
{
    qDebug() << "Connected to target server:" << m_targetHost << ":" << m_targetPort;
    m_targetConnected = true;

    if (m_isHttps && !m_httpsResponseSent) {
        sendHttpsResponse();
    }

    if (!m_httpRequest.isEmpty()) {
        if (m_targetSocket && m_targetSocket->state() == QAbstractSocket::ConnectedState) {
            m_targetSocket->write(m_httpRequest);
            m_httpRequest.clear();
        }
    }

    if (!m_clientBuffer.isEmpty() && m_targetSocket) {
        m_targetSocket->write(m_clientBuffer);
        m_clientBuffer.clear();
    }
}

void ConnectionHandler::onTargetReadyRead()
{
    if (!m_targetSocket || !m_clientSocket) return;

    QByteArray data = m_targetSocket->readAll();
    qDebug() << "Received" << data.size() << "bytes from target server";

    if (m_clientSocket->state() == QAbstractSocket::ConnectedState) {
        m_clientSocket->write(data);
    }
}

void ConnectionHandler::onTargetDisconnected()
{
    qDebug() << "Target server disconnected";
    closeConnection();
}

void ConnectionHandler::onTargetError(QAbstractSocket::SocketError error)
{
    qCritical() << "Target socket error:" << error << "-" << m_targetSocket->errorString();
    closeConnection();
}

void ConnectionHandler::closeConnection()
{
    if (m_clientSocket) {
        if (m_clientSocket->state() == QAbstractSocket::ConnectedState) {
            m_clientSocket->disconnectFromHost();
            if (m_clientSocket->state() != QAbstractSocket::UnconnectedState) {
                m_clientSocket->waitForDisconnected(3000);
            }
        }
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }

    if (m_targetSocket) {
        if (m_targetSocket->state() == QAbstractSocket::ConnectedState) {
            m_targetSocket->disconnectFromHost();
            if (m_targetSocket->state() != QAbstractSocket::UnconnectedState) {
                m_targetSocket->waitForDisconnected(3000);
            }
        }
        m_targetSocket->deleteLater();
        m_targetSocket = nullptr;
    }

    m_clientBuffer.clear();
    m_httpRequest.clear();
    m_targetHost.clear();
    m_targetPort = 0;
    m_isHttps = false;
    m_targetConnected = false;
    m_httpsResponseSent = false;

    emit disconnected();
}

bool ConnectionHandler::parseRequest(const QByteArray &data)
{
    if (!data.contains("\r\n\r\n")) {
        return false;
    }

    if (parseHttpsConnect(data)) {
        return true;
    }

    if (parseHttpRequest(data)) {
        return true;
    }

    return false;
}

bool ConnectionHandler::parseHttpRequest(const QByteArray &data)
{
    QByteArray headerEnd = "\r\n\r\n";
    int headerEndPos = data.indexOf(headerEnd);
    if (headerEndPos == -1) {
        return false;
    }

    QByteArray headers = data.left(headerEndPos + headerEnd.length());
    QString headerStr = QString::fromUtf8(headers);

    QRegularExpression requestLineRegex("^(GET|POST|PUT|DELETE|HEAD|OPTIONS|PATCH)\\s+(http://[^\\s]+)\\s+HTTP/");
    QRegularExpressionMatch match = requestLineRegex.match(headerStr);

    if (match.hasMatch()) {
        QString fullUrl = match.captured(2);
        QUrl url(fullUrl);

        if (url.isValid() && !url.host().isEmpty()) {
            m_targetHost = url.host();
            m_targetPort = url.port(80);
            m_isHttps = false;

            QString path = url.path();
            if (path.isEmpty()) {
                path = "/";
            }
            if (url.hasQuery()) {
                path += "?" + url.query();
            }

            QString method = match.captured(1);
            QString httpVersion = headerStr.mid(match.capturedEnd(2)).trimmed().split(" ").first();

            QString newRequestLine = QString("%1 %2 %3\r\n").arg(method, path, httpVersion);
            QStringList headerLines = headerStr.split("\r\n");
            QString newHeaders = newRequestLine;

            for (int i = 1; i < headerLines.size(); ++i) {
                QString line = headerLines[i];
                if (!line.isEmpty() && !line.startsWith("Proxy-Connection:")) {
                    newHeaders += line + "\r\n";
                }
            }
            newHeaders += "\r\n";

            m_httpRequest = newHeaders.toUtf8();
            m_clientBuffer = data.mid(headerEndPos + headerEnd.length());

            return true;
        }
    }

    return false;
}

bool ConnectionHandler::parseHttpsConnect(const QByteArray &data)
{
    QByteArray headerEnd = "\r\n\r\n";
    int headerEndPos = data.indexOf(headerEnd);
    if (headerEndPos == -1) {
        return false;
    }

    QByteArray headers = data.left(headerEndPos);
    QString headerStr = QString::fromUtf8(headers);

    QRegularExpression connectRegex("^CONNECT\\s+([^:\\s]+):(\\d+)\\s+HTTP/");
    QRegularExpressionMatch match = connectRegex.match(headerStr);

    if (match.hasMatch()) {
        m_targetHost = match.captured(1);
        m_targetPort = match.captured(2).toUInt();
        m_isHttps = true;

        m_clientBuffer = data.mid(headerEndPos + headerEnd.length());

        return true;
    }

    return false;
}

void ConnectionHandler::connectToTarget()
{
    if (m_targetHost.isEmpty() || m_targetPort == 0) {
        qCritical() << "Invalid target host/port";
        closeConnection();
        return;
    }

    m_targetSocket = new QTcpSocket(this);
    connect(m_targetSocket, &QTcpSocket::connected, this, &ConnectionHandler::onTargetConnected);
    connect(m_targetSocket, &QTcpSocket::readyRead, this, &ConnectionHandler::onTargetReadyRead);
    connect(m_targetSocket, &QTcpSocket::disconnected, this, &ConnectionHandler::onTargetDisconnected);
    connect(m_targetSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ConnectionHandler::onTargetError);

    qDebug() << "Connecting to target:" << m_targetHost << ":" << m_targetPort;
    m_targetSocket->connectToHost(m_targetHost, m_targetPort);
}

void ConnectionHandler::sendHttpsResponse()
{
    QByteArray response("HTTP/1.1 200 Connection Established\r\n\r\n");
    if (m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
        m_clientSocket->write(response);
        qDebug() << "Sent 200 Connection Established to client (for HTTPS)";
        m_httpsResponseSent = true;
    }
}

void ConnectionHandler::forwardData(QTcpSocket *from, QTcpSocket *to)
{
    if (!from || !to) return;

    QByteArray data = from->readAll();
    if (!data.isEmpty() && to->state() == QAbstractSocket::ConnectedState) {
        to->write(data);
    }
}
