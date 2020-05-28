#ifndef LOCALTCPSERVER_H
#define LOCALTCPSERVER_H

#include <QByteArray>
#include "keybox-proto-types.h"
#include <QtNetwork>

class LocalTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit LocalTcpServer(QObject *parent = nullptr);
    void abortConnection();

signals:
    void clientConnected();
    void clientDisconnected();

    void messageReceived(uint32_t type, const QByteArray & buffer); // see keybox-proto-types.h

public slots:
    bool sendMessage(uint32_t type, const QByteArray & buffer); // see keybox-proto-types.h

private slots:
    void acceptConnection();
    void socketStateChanged(QTcpSocket::SocketState state);
    void readBuffer();

private :
    void parse();
    QTcpServer *m_server;
    QTcpSocket *m_conn;
    QByteArray cachedData;
};

#endif // LOCALTCPSERVER_H
