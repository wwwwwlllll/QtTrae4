#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QUrl>

class ConnectionHandler : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionHandler(QObject *parent = nullptr);
    ~ConnectionHandler();

    void setClientSocketDescriptor(qintptr socketDescriptor);

signals:
    void disconnected();

private slots:
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QAbstractSocket::SocketError error);

    void onTargetConnected();
    void onTargetReadyRead();
    void onTargetDisconnected();
    void onTargetError(QAbstractSocket::SocketError error);

private:
    void closeConnection();
    bool parseRequest(const QByteArray &data);
    bool parseHttpRequest(const QByteArray &data);
    bool parseHttpsConnect(const QByteArray &data);
    void connectToTarget();
    void sendHttpsResponse();
    void forwardData(QTcpSocket *from, QTcpSocket *to);

    QTcpSocket *m_clientSocket;
    QTcpSocket *m_targetSocket;
    qintptr m_clientSocketDescriptor;
    QByteArray m_clientBuffer;
    QByteArray m_httpRequest;

    QString m_targetHost;
    quint16 m_targetPort;
    bool m_isHttps;
    bool m_targetConnected;
    bool m_httpsResponseSent;
};

#endif // CONNECTIONHANDLER_H
