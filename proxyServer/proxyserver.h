#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include <QTcpServer>
#include <QMap>

class ConnectionHandler;

class ProxyServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit ProxyServer(QObject *parent = nullptr);
    ~ProxyServer();

    bool startServer(quint16 port);
    void stopServer();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onHandlerDisconnected();

private:
    QMap<ConnectionHandler*, ConnectionHandler*> m_handlers;
};

#endif // PROXYSERVER_H
