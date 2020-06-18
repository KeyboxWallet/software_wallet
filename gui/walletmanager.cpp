#include <QMessageBox>
#include "walletmanager.h"
#include "keybox-errcodes.h"
extern "C" {
    #include "pb.h"
    #include "pb_encode.h"
    #include "pb_decode.h"
    #include "messages.pb.h"
}
#include "btc/tx.h"
#include "btc/psbt.h"

WalletManager::WalletManager(QObject *parent) : QObject(parent)
{
    mWallet = NULL;
    mServer = new LocalTcpServer(this);
    connect(mServer, &LocalTcpServer::clientConnected, this, &WalletManager::clientConnect);
    connect(mServer, &LocalTcpServer::clientDisconnected, this, &WalletManager::clientDisconnected);
    connect(mServer, &LocalTcpServer::messageReceived, this, &WalletManager::messageReceived);
    mHasClient = false;
    reqId = 0;
}

void WalletManager::createWallet(const QString &mnemonic, const QString &password, const QString &name)
{
    if(mWallet){
        delete mWallet;
    }
    mWallet = new WalletInstance(mnemonic, password);
    mWallet->setName(name);
    emit walletChanged();
}

void WalletManager::loadWallet(const QString &fileName)
{
    mWallet = WalletInstance::loadFromFile(fileName);
    emit walletChanged();
}

WalletInstance *WalletManager::getWalletInstance()
{
    return mWallet;
}

void WalletManager::clientConnect()
{
    mHasClient = true;
}

void WalletManager::clientDisconnected()
{
    mHasClient = false;
}

QString btc_value_pretty(uint64_t value)
{
    QString ret;
    if( value < 1000000 ){
        ret = QString::fromUtf8("%1mBTC").arg(value/100000.0);
    }
    else{
        ret = QString::fromUtf8("%1BTC").arg(value / 100000000.0);
    }
    return ret;
}



void WalletManager::messageReceived(uint32_t type, const QByteArray &buffer)
{
#define CHECK_MWALLET   \
if( !mWallet ){ \
    replyError(KEYBOX_ERROR_WALLET_NOT_INITIALIZED, "wallet not initialzed"); \
    break;  \
} \
if( mWallet->isLocked() ) {          \
    replyError(KEYBOX_ERROR_WALLET_LOCKED, "wallet locked."); \
}
    reqId ++;
    pb_istream_t stream = pb_istream_from_buffer((uint8_t*)buffer.constData(), buffer.size());
    bool decode_status;
    EccSignRequest signReq = EccSignRequest_init_default;
    BitcoinSignRequest bitcoinSignReq = BitcoinSignRequest_init_default;
    EccGetPublicKeyRequest getPubKeyReq = EccGetPublicKeyRequest_init_default;
    EccGetExtendedPublicKeyRequest getXpubReq = EccGetExtendedPublicKeyRequest_init_default;
    EccMultiplyRequest multiplyReq = EccMultiplyRequest_init_default;

    QByteArray rb;
    bool encode_status;
    size_t size;

    rb.resize(1024*10);

    switch( type ){
    case MsgTypeGetWalletIdentifierRequest:
        CHECK_MWALLET
        {
            QByteArray id;
            mWallet->getBip32MasterKeyId(id);
            GetWalletIdentifierReply reply;
            reply.bip32MasterKeyId.size = id.size();
            memcpy(reply.bip32MasterKeyId.bytes, id.constData(), id.size());
            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, GetWalletIdentifierReply_fields, &reply);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeGetWalletIdentifierReply, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }

         }

        break;
    case MsgTypeEccGetPublicKeyRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, EccGetPublicKeyRequest_fields, &getPubKeyReq);
        if( decode_status ){
            if( getPubKeyReq.algorithm != EccAlgorithm_SECP256K1){
                replyError(KEYBOX_ERROR_NOT_SUPPORTED, "unsupported algorithm");
                break;
            }
            QByteArray array;
            int32_t errcode;
            QString errMessage;
            mWallet ->getPublicKey(QString::fromUtf8(getPubKeyReq.hdPath), errcode, errMessage, array);
            if( errcode != 0 ){
                replyError(errcode, errMessage);
                break;
            }
            EccGetPublicKeyReply reply;
            reply.algorithm = EccAlgorithm_SECP256K1;
            qstrncpy(reply.hdPath, getPubKeyReq.hdPath, sizeof(reply.hdPath));
            reply.pubkey.size = array.size();
            memcpy(reply.pubkey.bytes, array.constData(), array.size());

            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, EccGetPublicKeyReply_fields, &reply);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeEccGetPublicKeyReply, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }

        }
        else {
            qDebug("%s %d", getPubKeyReq.hdPath , getPubKeyReq.algorithm );
            // m_conn->abort();
            mServer->abortConnection();
        }
        break;
    case MsgTypeEccGetExtendedPubkeyRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, EccGetExtendedPublicKeyRequest_fields, &getXpubReq);
        if( decode_status ){
            if( getXpubReq.algorithm != EccAlgorithm_SECP256K1){
                replyError(KEYBOX_ERROR_NOT_SUPPORTED, "unsupported algorithm");
                break;
            }
            QByteArray array;
            QByteArray chainCode;
            int32_t errcode;
            QString errMessage;
            mWallet ->getExtendedPubKey(QString::fromUtf8(getXpubReq.hdPath), errcode, errMessage, array, chainCode);
            if( errcode != 0 ){
                replyError(errcode, errMessage);
                break;
            }
            EccGetExtendedPublicKeyReply reply;
            reply.algorithm = EccAlgorithm_SECP256K1;
            qstrncpy(reply.hdPath, getPubKeyReq.hdPath, sizeof(reply.hdPath));
            reply.pubkey.size = array.size();
            memcpy(reply.pubkey.bytes, array.constData(), array.size());
            reply.chainCode.size = chainCode.size();
            memcpy(reply.chainCode.bytes, chainCode.constData(), chainCode.size());

            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, EccGetExtendedPublicKeyReply_fields, &reply);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeEccGetExtendedPubkeyReply, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }
        }
        else{
            qDebug("%s %d", getXpubReq.hdPath , getXpubReq.algorithm );
            // m_conn->abort();
            mServer->abortConnection();
        }
        break;
    case MsgTypeEccSignRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, EccSignRequest_fields, &signReq);
        if( decode_status ){
            if( signReq.algorithm != EccAlgorithm_SECP256K1){
                replyError(KEYBOX_ERROR_NOT_SUPPORTED, "unsupported algorithm");
                break;
            }

            if( signReq.hash.size!= 32){
                replyError(  KEYBOX_ERROR_INVALID_PARAMETER, "you must provide exact 32 byte hash");
                return;
            }

            QByteArray inputHash((char*)signReq.hash.bytes, signReq.hash.size);
            QMessageBox msgBox;
            msgBox.setText("确认签名请求");
            msgBox.setInformativeText(QString::fromUtf8("是否确认签名？\n") + inputHash.toHex());
            msgBox.setStandardButtons(QMessageBox::Ok  | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            int ret = msgBox.exec();
            if(ret == QMessageBox::Cancel ){
                replyError(KEYBOX_ERROR_USER_CANCELLED, "用户拒绝签名");
                return;
            }

            QByteArray pubkey;
            int32_t errcode;
            QString errMessage;
            EccSignature sig;
            mWallet ->eccSign(QString::fromUtf8(signReq.hdPath), QByteArray((char*)signReq.hash.bytes, signReq.hash.size), signReq.options, errcode, errMessage, sig, pubkey);
            if( errcode != 0 ){
                replyError(errcode, errMessage);
                break;
            }
            EccSignResult reply;
            // reply.algorithm = EccAlgorithm_SECP256K1;
            // reply.hash = signReq.hash;
            // reply.hdPath = signReq.hdPath;
            memcpy(reply.pubkey.bytes, pubkey.constData(), pubkey.size() );
            reply.pubkey.size = pubkey.size();

            qstrncpy(reply.hdPath, signReq.hdPath, sizeof(reply.hdPath));
            memcpy(reply.hash.bytes, signReq.hash.bytes, signReq.hash.size);
            reply.hash.size = signReq.hash.size;

            memcpy(reply.R.bytes, sig.R.constData(), sig.R.size());
            reply.R.size = sig.R.size();

            memcpy(reply.S.bytes, sig.S.constData(), sig.S.size());
            reply.S.size = sig.S.size();

            reply.recover_param = sig.recovery_param;

            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, EccSignResult_fields, &reply);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeEccSignResult, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }

        }
        else {
            qDebug("%s %d", signReq.hdPath , signReq.algorithm );
            // m_conn->abort();
            mServer->abortConnection();
        }
        break;
    case MsgTypeBitcoinSignRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, BitcoinSignRequest_fields, &bitcoinSignReq);
        if( decode_status ){
            // check path first
            psbt p;
            struct const_buffer b;
            b.p = bitcoinSignReq.psbt.bytes ;
            b.len = bitcoinSignReq.psbt.size;
            psbt_init(&p);
            btc_tx *tx = nullptr;
            QString outinfo;
            outinfo.append("付款信息:\n");
            char address[98];
            size_t i,j,k;
            int ret;
            bool someSigned = false;
            psbt_map_elem * elem;
            uint32_t * bip32_path;
            uint256 hash;
            size_t path_size;
            HDNode signNode;
            QMessageBox msgBox;
            uint32_t hashType = SIGHASH_ALL;
            vector * ivec;
            char * errMsg;
            uint8_t sig[65];
            cstring * outStr;
            BitcoinSignResult result;
            pb_ostream_t ostream ;

            if( !psbt_deserialize(&p, &b) ){
               replyError(  KEYBOX_ERROR_INVALID_PARAMETER, "you must provide valid psbt buffer.");
               goto _psbt_err_ret;
            }
            // output message, let user decide.
            tx = psbt_get_unsigned_tx(&p);
            Q_ASSERT(tx);
            for(i=0; i<tx->vout->len; i++){
                // todo: check change, and if it is, omit info
                btc_tx_out * tx_out = (btc_tx_out*)vector_idx(tx->vout, i);
                btc_tx_get_output_address(address, 
                    tx_out,
                    bitcoinSignReq.testnet ? & btc_chainparams_test : & btc_chainparams_main );
                    outinfo.append(address);
                    outinfo.append(" ");
                    outinfo.append(btc_value_pretty(tx_out->value));
                    outinfo.append("\n");   
            }
            msgBox.setText("Bitcoin 确认签名:");
            msgBox.setInformativeText(outinfo);
            msgBox.setStandardButtons(QMessageBox::Ok  | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            ret = msgBox.exec();
            if(ret == QMessageBox::Cancel ){
                replyError(KEYBOX_ERROR_USER_CANCELLED, "用户拒绝签名");
                goto _psbt_err_ret;
            }
                
            // match which input can be signed, and sign it.
            for( i=0; i<tx->vin->len; i++){
                ivec = (vector*)vector_idx(p.input_data, i);
                for(j=0; j<ivec->len; j++){
                    elem = (psbt_map_elem*) vector_idx(ivec, j);
                    if( elem->type.input == PSBT_IN_BIP32_DERIVATION ){

                       //bip32_path = (const uint32_t*)elem->value.p;
                        if( (elem->value.len & 3) != 0 || elem->value.len < 8){
                            replyError(KEYBOX_ERROR_CLIENT_ISSUE, "非法的bip32路径");
                            goto _psbt_err_ret;
                        }
                        path_size = elem->value.len / 4;
                        bip32_path = (uint32_t*)malloc( elem->value.len);
                        memcpy(bip32_path, elem->value.p, elem->value.len);
                        bip32_path[0] = be32toh(bip32_path[0]);
                        for( k=1; k<path_size; k++){
                            //memcpy(bip32_path[k
                            bip32_path[i] = le32toh(bip32_path[i]);
                        }
                        if( !mWallet->getBip32NodeFromUint32Array(&signNode, bip32_path, path_size )){
                            free(bip32_path);
                            continue;
                        }
                        if( memcmp(signNode.public_key, (uint8_t*)elem->key.p + 1, 33) != 0 ){
                            free(bip32_path);
                            continue;
                        }
                        // sign it.
                        if( !psbt_check_for_sig(&p, i, &hashType, &errMsg)
                           || !psbt_get_sighash(&p, i, hashType, hash, &errMsg) ){
                            free(bip32_path);
                            replyError(KEYBOX_ERROR_CLIENT_ISSUE, "psbt can't be signed.");
                            goto _psbt_err_ret;
                        }
                        if( hdnode_sign_digest(&signNode, hash, sig, sig+64, nullptr)==0 ){
                            psbt_add_partial_sig(&p, i, signNode.public_key, sig);
                            someSigned = true;
                        }
                    }

                }

            }
            // serialize and back the psbt.
            if( someSigned ){
                outStr = cstr_new_sz(2048);
                if( psbt_serialize(outStr, &p) && outStr->len < 8192 ){
                    result.psbt.size = outStr->len;
                    memcpy(result.psbt.bytes, outStr->str, outStr->len);
                    ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
                    encode_status = pb_encode(&ostream, BitcoinSignResult_fields, &result);
                    size = ostream.bytes_written;

                    if( encode_status ){
                        rb.resize(size);
                        mServer -> sendMessage(MsgTypeBitcoinSignResult, rb);
                    }
                    else {
                        qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
                    }
                }
                else{
                    replyError(KEYBOX_ERROR_SERVER_ISSUE, "can not serialze output psbt");
                }
                cstr_free(outStr, true);
            }
            else{
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "none of inputs can be signed");
            }
_psbt_err_ret:
            psbt_reset(&p); // free psbt memory
        }
        else{
           //  qDebug("%s", bitcoinSignReq.hdPath );
            mServer->abortConnection();
        }
        break;
    case MsgTypeEccMultiplyRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, EccMultiplyRequest_fields, &multiplyReq);
        if( decode_status ){
            if( multiplyReq.algorithm != EccAlgorithm_SECP256K1){
                replyError(KEYBOX_ERROR_NOT_SUPPORTED, "unsupported algorithm");
                break;
            }

            if( multiplyReq.input_pubkey.size!= 64){
                replyError(  KEYBOX_ERROR_INVALID_PARAMETER, "you must provide exact 64 byte pbukey");
                return;
            }


            QByteArray result, outPubkey;
            int32_t errcode;
            QString errMessage;
            mWallet -> ecdhMultiply(QString::fromUtf8(multiplyReq.hdPath), QByteArray((char*)multiplyReq.input_pubkey.bytes, multiplyReq.input_pubkey.size), errcode, errMessage, result, outPubkey);
            if( errcode != 0 ){
                replyError(errcode, errMessage);
                break;
            }
            EccMultiplyReply reply;
            reply.algorithm = EccAlgorithm_SECP256K1;
            // reply.hash = signReq.hash;
            // reply.hdPath = signReq.hdPath;
            memcpy(reply.input_pubkey.bytes, multiplyReq.input_pubkey.bytes, multiplyReq.input_pubkey.size );
            reply.input_pubkey.size = multiplyReq.input_pubkey.size;

            memcpy(reply.dev_pubkey.bytes, outPubkey.constData(), outPubkey.size());
            reply.dev_pubkey.size = outPubkey.size();

            qstrncpy(reply.hdPath, signReq.hdPath, sizeof(reply.hdPath));


            memcpy(reply.result.bytes, result.constData(), result.size());
            reply.result.size = result.size();


            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, EccMultiplyReply_fields, &reply);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeEccMultiplyReply, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }

        }
        else {
            qDebug("%s %d", getPubKeyReq.hdPath , getPubKeyReq.algorithm );
            // m_conn->abort();
            mServer->abortConnection();
        }
        break;
    default:
        replyError(KEYBOX_ERROR_NOT_SUPPORTED, "unsupported Method");
        break;
    }
}

void WalletManager::replyError(int32_t code, const QString &message)
{
    RequestRejected rej = RequestRejected_init_default;
    rej.errCode = code;
    rej.requestId = this->reqId;
    QString m = "[wallet]";
    m.append(message);
    qstrncpy(rej.errMessage, m.toUtf8().constData(), sizeof(rej.errMessage));
    QByteArray r;
    bool encode_status;
    size_t size;

    r.resize(1024*8);
    pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)r.data(), r.size());
    encode_status = pb_encode(&ostream, RequestRejected_fields, &rej);
    size = ostream.bytes_written;

    if( encode_status ){
        r.resize(size);
        mServer -> sendMessage(MsgTypeRequestRejected, r);
    }
    else {
        qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
    }
}
