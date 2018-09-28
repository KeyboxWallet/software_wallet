#include "walletinstance.h"
#include "keybox-errcodes.h"
extern "C" {
    #include "aes.h"
    #include "bip39.h"
    #include "pbkdf2.h"
    #include "memzero.h"
    // #include "bip32.h"
}

#include "crypto_utils.h"

void getSecretAndIv(QString const &password, uint8_t secret[64]){
    const char *pass = password.toUtf8().constData();
    PBKDF2_HMAC_SHA512_CTX pctx;
    pbkdf2_hmac_sha512_Init(&pctx, (const uint8_t*)pass, strlen(pass), (const uint8_t *)"KeyBox", 6);
    pbkdf2_hmac_sha512_Update(&pctx, 2048 );
    pbkdf2_hmac_sha512_Final(&pctx, secret);

}

WalletInstance::WalletInstance(QString const & mnemonic, QString const & password)
{
    // 生成 seed
    m_seed_plain.resize(512/8);
    const char * m = mnemonic.toUtf8().constData();
    const char * p = "";
    mnemonic_to_seed(m, p, (uint8_t*)m_seed_plain.data(), NULL);

    // 生成加密用的Key和IV
    uint8_t secret[64];
    getSecretAndIv(password, secret);

    // 加密
    aes_encrypt_ctx enc_ctx;
    aes_encrypt_key256(secret, &enc_ctx);
    m_seed_encrypted.resize(512/8 + 32);
    aes_cbc_encrypt((const uint8_t*)m_seed_plain.constData(), (uint8_t*)m_seed_encrypted.data(), 512/8, secret + 32, &enc_ctx);
    char  pattern[32] ;
    memset(pattern, 'K', 32);
    aes_cbc_encrypt((const uint8_t*)pattern, (uint8_t*)m_seed_encrypted.data() + 512/8, 32, secret + 32, &enc_ctx);
}

QString WalletInstance::generateMnemonic()
{
    // TODO: check memory leak
    char * m = (char*)mnemonic_generate(160);
    QString r = QString::fromUtf8(m);
    return r;
}

bool WalletInstance::isLocked(){
    return m_seed_plain.size() == 0;
}

bool WalletInstance::unlock(QString const &password){
    if( !this->isLocked()){
        return false;
    }

    // 生成加密用的Key和IV
    uint8_t secret[64];
    getSecretAndIv(password, secret);


    // 解密
    aes_decrypt_ctx dec_ctx;
    aes_decrypt_key256(secret, &dec_ctx);
    uint8_t result[512/8 + 32];
    // m_seed_plain.resize(512/8 + 32);
    aes_cbc_decrypt((const uint8_t*)m_seed_encrypted.constData(), result, 512/8 + 32, secret + 32, &dec_ctx);
    bool isOK = true;
    for( int index=64; index<64+32; index++){
        isOK =  (char)result[index] == 'K' && isOK;
    }
    if (!isOK){
        return isOK;
    }
    m_seed_plain.resize(512/8);
    memcpy(m_seed_plain.data(), result, m_seed_plain.size());
    return isOK;
}


void WalletInstance::lock(){
    if( this->isLocked()){
        return;
    }
    memzero(m_seed_plain.data(), m_seed_plain.size());
    m_seed_plain.resize(0);
}

void WalletInstance::getPublicKey(const QString &path, int32_t& errcode, QString &errMessage, QByteArray & pubkey){
    errMessage = "";
    errcode = 0;
    if( this->isLocked()){
        errcode = KEYBOX_ERROR_WALLET_LOCKED;
        errMessage = "wallet locked";
        return ;
    }
    if(!path.startsWith("bip32/")){
        errcode = KEYBOX_ERROR_NOT_SUPPORTED;
        errMessage = "path not supported, only support bip32/ now";
        return;
    }
    QString bip32path = path.right(path.length() - 6);

    if( !isValidBip32Path(bip32path.toUtf8())){
        errcode = KEYBOX_ERROR_INVALID_PARAMETER;
        errMessage = "invalid BIP32 path";
        return;
    }
    HDNode node;
    // QString privateKeyForDebug;
    if( getBip39NodeFromPath(m_seed_plain.data(), bip32path.toUtf8(), &node)){
        uint8_t pubkey_uncompressed[65];
        if ( ecdsa_uncompress_pubkey(node.curve->params, node.public_key, pubkey_uncompressed) ){
          pubkey.resize(64);
          memcpy(pubkey.data(), pubkey_uncompressed + 1, 64 );
        }
        else {
            errcode = KEYBOX_ERROR_SERVER_ISSUE;
            errMessage = "internal error";
        }
        // privateKeyForDebug = QString::fromUtf8( QByteArray((char*)node.private_key, 32).toHex() );
        // qDebug("private key: %s", QByteArray((char*)node.private_key, 32).toHex().constData());
        // memcpy(pubkey.data(), node.public_key + 1, 32);
    }
    else {
        errcode = KEYBOX_ERROR_SERVER_ISSUE;
        errMessage = "internal error";
    }
 }

void WalletInstance::eccSign(const QString &path, const QByteArray &hash_digest, const EccSignOptions &options, int32_t&errcode, QString &errMessage, EccSignature &sig,  QByteArray & pubkey) {
    errMessage = "";
    errcode = 0;
    if( this->isLocked()){
        errcode = KEYBOX_ERROR_WALLET_LOCKED;
        errMessage = "wallet locked";
        return ;
    }
    if(!path.startsWith("bip32/")){
        errcode = KEYBOX_ERROR_NOT_SUPPORTED;
        errMessage = "path not supported, only support bip32/ now";
        return;
    }
    QString bip32path = path.right(path.length() - 6);

    if( !isValidBip32Path(bip32path.toUtf8())){
        errcode = KEYBOX_ERROR_INVALID_PARAMETER;
        errMessage = "invalid BIP32 path";
        return;
    }

    if( !options.rfc6979 ) {
        errcode = KEYBOX_ERROR_NOT_SUPPORTED;
        errMessage = "only support RFC6979 mode";
        return;
    }

    if( hash_digest.size() != 32){
        errcode = KEYBOX_ERROR_INVALID_PARAMETER;
        errMessage = "you must provide exact 32 byte hash";
        return;
    }

    HDNode node;
    if( getBip39NodeFromPath(m_seed_plain.data(), bip32path.toUtf8(), &node)){
        // pubkey.resize(32);
        // memcpy(pubkey.data(), node.public_key + 1, 32);
        uint8_t usig[64];
        uint8_t by;
        if( 0 == ecdsa_sign_digest(node.curve->params, node.private_key, (uint8_t*)hash_digest.data(), usig, &by, options.graphene_canonize ? isSigGrapheneCanonical: NULL)) {
           sig.R = QByteArray((char*)usig, 32);
           sig.S = QByteArray((char*)usig+32, 32);
           sig.recovery_param = by;

           uint8_t pubkey_uncompressed[65];
           if ( ecdsa_uncompress_pubkey(node.curve->params, node.public_key, pubkey_uncompressed) ){
             pubkey.resize(64);
             memcpy(pubkey.data(), pubkey_uncompressed + 1, 64 );
           }
           else {
               errcode = KEYBOX_ERROR_SERVER_ISSUE;
               errMessage = "internal error";
           }
        }
        else{
           errcode = KEYBOX_ERROR_GENERAL;
           errMessage = "internal error";
        }
    }
    else {
        errcode = KEYBOX_ERROR_GENERAL;
        errMessage = "internal error";
    }
}

void WalletInstance::ecdhMultiply(QString const & path, const QByteArray &pubkey, int32_t & errcode, QString & errMessage, QByteArray &result, QByteArray &outPubkey )
{
    errMessage = "";
    errcode = 0;
    if( this->isLocked()){
        errcode = KEYBOX_ERROR_WALLET_LOCKED;
        errMessage = "wallet locked";
        return ;
    }
    if(!path.startsWith("bip32/")){
        errcode = KEYBOX_ERROR_NOT_SUPPORTED;
        errMessage = "path not supported, only support bip32/ now";
        return;
    }
    QString bip32path = path.right(path.length() - 6);

    if( !isValidBip32Path(bip32path.toUtf8())){
        errcode = KEYBOX_ERROR_INVALID_PARAMETER;
        errMessage = "invalid BIP32 path";
        return;
    }

    if (pubkey.size() != 64) {
        errcode = KEYBOX_ERROR_INVALID_PARAMETER;
        errMessage = "pubkey must be exactly 512bit (64 byte)";
        return;
    }

    HDNode node;
    if( getBip39NodeFromPath(m_seed_plain.data(), bip32path.toUtf8(), &node)){
        // pubkey.resize(32);
        // memcpy(pubkey.data(), node.public_key + 1, 32);
        uint8_t pubkey_array[65];
        uint8_t result_array[65];
        pubkey_array[0] = 4;

        memcpy(pubkey_array+1, pubkey.constData(), 64);
        if (0 == ecdh_multiply(node.curve->params, node.private_key, pubkey_array, result_array)) {
            result.resize(64);
            memcpy(result.data(), result_array + 1, 64);

            uint8_t pubkey_uncompressed[65];
            if ( ecdsa_uncompress_pubkey(node.curve->params, node.public_key, pubkey_uncompressed) ){
              outPubkey.resize(64);
              memcpy(outPubkey.data(), pubkey_uncompressed + 1, 64 );
            }
            else {
                errcode = KEYBOX_ERROR_SERVER_ISSUE;
                errMessage = "internal error";
            }
        }
        else{
           errcode = KEYBOX_ERROR_CLIENT_ISSUE;
           errMessage = "you must specify a valid pubkey";
        }
    }
    else {
        errcode = KEYBOX_ERROR_GENERAL;
        errMessage = "internal error";
    }
}

bool WalletInstance::saveToFile(const QString &path)
{
    QFile f(path);
    if( f.open(QIODevice::WriteOnly) ){

        QDataStream s(&f);
        s << m_name;
        s << m_seed_encrypted;
        f.close();
        return true;
    }
    return false;
}

WalletInstance::WalletInstance(const QByteArray &seedEncrypted, const QString &name)
{
    m_name = name;
    m_seed_encrypted = seedEncrypted;
    m_seed_plain.resize(0);
}

WalletInstance * WalletInstance::loadFromFile(const QString & path)
{
    QFile f(path);
    QString name;
    QByteArray seedEncryped;
    if( f.open(QIODevice::ReadOnly) ){

        QDataStream s(&f);

        s >> name >> seedEncryped;
        f.close();
        return new WalletInstance(seedEncryped, name);

    }
    return NULL;
}
