#ifndef WALLETMANAGER_H
#define WALLETMANAGER_H

#include <QObject>
#include "walletinstance.h"
#include "localtcpserver.h"

class WalletManager : public QObject
{
    Q_OBJECT
public:
    explicit WalletManager(QObject *parent = nullptr);
    void createWallet(const QString& mnemonic, const QString & password, const QString &name);
    void loadWallet(const QString &fileName);
    WalletInstance * getWalletInstance();
    ~WalletManager();
signals:
    void walletChanged();
public slots:

private slots:
    void clientConnect();
    void clientDisconnected();
    void messageReceived(uint32_t type, const QByteArray & buffer);

private:
    void replyError(int32_t code, const QString &message);
    WalletInstance *mWallet;
    LocalTcpServer *mServer;
    bool mHasClient;
    int reqId;
};

#endif // WALLETMANAGER_H
