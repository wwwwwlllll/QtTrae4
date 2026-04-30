#ifndef TUNNEL_H
#define TUNNEL_H

#include <QObject>
#include <QTcpSocket>
#include <QBuffer>

class Tunnel : public QObject
{
    Q_OBJECT

public:
    explicit Tunnel(QObject *parent = nullptr);
    ~Tunnel();

    void setBrowserSocketDescriptor(qintptr socketDescriptor);
    void connectToProxyServer(const QString &host, quint16 port);

signals:
    void disconnected();

private slots:
    void onBrowserReadyRead();
    void onBrowserDisconnected();
    void onBrowserError(QAbstractSocket::SocketError error);

    void onProxyServerConnected();
    void onProxyServerReadyRead();
    void onProxyServerDisconnected();
    void onProxyServerError(QAbstractSocket::SocketError error);

private:
    void closeConnection();
    void flushBuffer(QTcpSocket *to);

    QTcpSocket *m_browserSocket;
    QTcpSocket *m_proxyServerSocket;
    qintptr m_browserSocketDescriptor;
    QByteArray m_browserBuffer;
    QString m_proxyServerHost;
    quint16 m_proxyServerPort;
    bool m_proxyServerConnected;
};

#endif // TUNNEL_H
