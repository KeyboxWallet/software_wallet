#ifndef WALLETINSTANCE_H
#define WALLETINSTANCE_H

#include <QtCore>
#include <QByteArray>

#include "messages.pb.h"
extern "C"
{
    #include "bip32.h"
};

struct EccSignature {
    QByteArray R;
    QByteArray S;
    uint32_t recovery_param;
};


class WalletInstance
{
public:
    WalletInstance(QString const & mnemonic, QString const & password);
    WalletInstance(const QByteArray & seedEncrypted, const QString &name);

    static QString generateMnemonic();
    void setName(const QString &name){
        m_name = name;
    }
    const QString & getName(){
        return m_name;
    }

    bool unlock(QString const & password);
    void lock();
    bool isLocked();
    void getPublicKey(QString const & path, int32_t& errcode, QString &errMessage, QByteArray &pubkey);
    void getExtendedPubKey(QString const &path, int32_t& errcode, QString &errMessage, QByteArray &pubkey, QByteArray &chainCode);
    void getBip32MasterKeyId(QByteArray &id);
    void eccSign(QString const & path,
                 const QByteArray &hash_digest,
                 const EccSignOptions &options,
                 int32_t& errcode, QString &errMessage,
                 EccSignature &sig,
                 QByteArray &pubkey
                 );
    void ecdhMultiply(QString const & path,
                      const QByteArray &pubkey,
                      int32_t & errcode,
                      QString & errMessage,
                      QByteArray &result,
                      QByteArray &outPubkey );
    bool saveToFile(const QString &path);
    static WalletInstance* loadFromFile(const QString &path);
    bool getBip32NodeFromUint32Array(HDNode *node, const uint32_t * path, size_t count);

private:
    QByteArray m_seed_encrypted;
    QByteArray m_seed_plain;
    QString m_name;

};

#endif // WALLETINSTANCE_H
