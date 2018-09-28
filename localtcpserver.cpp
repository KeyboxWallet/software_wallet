#include <arpa/inet.h>
#include "localtcpserver.h"
#include <iostream>

LocalTcpServer::LocalTcpServer(QObject *parent) : QObject(parent)
{
    m_conn = NULL;
    cachedData.resize(0);
    m_server = new QTcpServer(this);
    if( !m_server->listen(QHostAddress::LocalHost, 15320)){
        qFatal("can't listen on local socket");
        return;
    }
    m_server ->setMaxPendingConnections(1);
    connect(m_server, &QTcpServer::newConnection, this, &LocalTcpServer::acceptConnection);
}

void LocalTcpServer::abortConnection()
{
    if( m_conn ){
        m_conn -> abort();
    }
}

bool LocalTcpServer::sendMessage(uint32_t type, const QByteArray & buffer)
{
    if( m_conn ){
        QByteArray b;
        qint64 r;
        b.resize(8);
        uint32_t temp = htonl(type);
        memcpy(b.data(), &temp, 4);
        temp = htonl(buffer.size());
        memcpy(b.data()+ 4, &temp, 4);
        b += buffer;

        r = m_conn->write(b);
        return ( r == b.size() ); // TODO: queue
    }
    return false;
}

void LocalTcpServer::acceptConnection()
{
    QTcpSocket * socket;
    socket = m_server->nextPendingConnection();

    if( socket ){
        if( !m_conn) {
            m_conn = socket;
            connect(m_conn, &QTcpSocket::stateChanged, this, &LocalTcpServer::socketStateChanged);
            connect(m_conn, &QIODevice::readyRead, this, &LocalTcpServer::readBuffer );
            emit clientConnected();
            this->readBuffer();
        }
        else {
            socket->abort();
        }
    }

}

void LocalTcpServer::socketStateChanged(QTcpSocket::SocketState state)
{
    if(  state != QTcpSocket::ConnectedState) {
        disconnect(m_conn, &QTcpSocket::stateChanged, this, &LocalTcpServer::socketStateChanged);
        cachedData.resize(0);
        m_conn = NULL;
    }
}

void LocalTcpServer::readBuffer()
{
    QByteArray  d = m_conn -> readAll();
    // parse data
    cachedData += d;
    parse();
}

void LocalTcpServer::parse()
{
    if( cachedData.size() < 8){
        return;
    }
    uint32_t type;
    uint32_t length;
    memcpy(&type, cachedData.constData(), 4);
    memcpy(&length, cachedData.constData() + 4, 4);
    type = ntohl(type);
    length = ntohl(length);
    if( (uint32_t)cachedData.size() < length + 8){
        return;
    }
    cachedData = cachedData.right(cachedData.size() - 8);
    QByteArray msgData = cachedData.left(length);
    emit messageReceived(type, msgData);
    cachedData = cachedData.right(cachedData.length() - length);
    parse();
}

