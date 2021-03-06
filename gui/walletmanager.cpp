#include <QMessageBox>
#include "walletmanager.h"
#include "keybox-errcodes.h"
extern "C" {
    #include "pb.h"
    #include "pb_encode.h"
    #include "pb_decode.h"
    #include "messages.pb.h"
    #include "rlp.h"
    #include "cash_addr.h"
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

WalletManager::~WalletManager()
{
    delete mWallet;
    if( mServer ){
        delete mServer;
    }
}

void WalletManager::clientConnect()
{
    mHasClient = true;
}

void WalletManager::clientDisconnected()
{
    mHasClient = false;
}




QString btc_value_pretty(uint64_t value, BtcLikeCoins coin, bool testNet)
{
    QString ret;
    const char * coinName = "BTC";
    switch( coin ){
        case BtcLikeCoins_BITCOIN:
            if( !testNet){
                coinName = "BTC";
            }
            else{
                coinName = "tBTC";
            }
            break;
        case BtcLikeCoins_LITECOIN:
            if( !testNet){
                coinName = "LTC";
            }
            else{
                coinName = "tLTC";
            }
            break;
        case BtcLikeCoins_DASH:
            if( !testNet){
                coinName = "DASH";
            }
            else{
                coinName = "tDASH";
            }
            break;
        case BtcLikeCoins_BITCOINCASH:
            if (!testNet){
                coinName = "BCH";
            }
            else {
                coinName = "tBCH";
            }
            break;
        default:
            break;
    }
    if( value < 10000) {
        ret = QString::fromUtf8("%1SAT").arg(value);
    }
    else if( value < 1000000 ){
        ret = QString::fromUtf8("%1m%2").arg(value/100000.0).arg(coinName);
    }
    else{
        ret = QString::fromUtf8("%1%2").arg(value / 100000000.0).arg(coinName);
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
    EthereumSignRequest ethSignReq = EthereumSignRequest_init_default;
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
            BtcLikeCoins coin = bitcoinSignReq.coin;
            const btc_chainparams *params;
            switch(coin){
            case BtcLikeCoins_BITCOIN:
            case BtcLikeCoins_BITCOINCASH:
                if( !bitcoinSignReq.testnet){
                    params = &btc_chainparams_main;
                }
                else{
                    params = &btc_chainparams_test;
                }
                break;
            case BtcLikeCoins_LITECOIN:
                if( !bitcoinSignReq.testnet){
                    params = &ltc_chainparams_main;
                }
                else{
                    params = &ltc_chainparams_test;
                }
                break;
            case BtcLikeCoins_DASH:
                if( !bitcoinSignReq.testnet){
                    params = &dash_chainparams_main;
                }
                else{
                    params = &dash_chainparams_test;
                }
                break;
            default:
                params = &btc_chainparams_main;
                break;
            }
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
            vector * ovec;
            char * errMsg;
            uint8_t sig[65];
            cstring * outStr;
            BitcoinSignResult result;
            pb_ostream_t ostream ;
            uint64_t miner_fee;
            bool isChange;
            if( !psbt_deserialize(&p, &b) ){
               replyError(  KEYBOX_ERROR_INVALID_PARAMETER, "you must provide valid psbt buffer.");
               goto _psbt_err_ret;
            }
            // output message, let user decide.
            tx = psbt_get_unsigned_tx(&p);
            Q_ASSERT(tx);
            for(i=0; i<tx->vout->len; i++){
                isChange = false;
                ovec = (vector*)vector_idx(p.output_data, i);
                for(j=0; j<ovec->len && !isChange; j++){
                    psbt_map_elem * elem = (psbt_map_elem*)vector_idx(ovec, j);
                    if( elem->type.output == PSBT_OUT_BIP32_DERIVATION ){
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

                        }
                        else{
                            if( memcmp(signNode.public_key, (uint8_t*)elem->key.p + 1, 33) == 0 ){
                                isChange = true;
                            }
                        }
                        free(bip32_path);
                    }
                }
                if( isChange )
                    continue;
                btc_tx_out * tx_out = (btc_tx_out*)vector_idx(tx->vout, i);
                btc_tx_get_output_address(address, 
                    tx_out,
                    params);
                if( coin == BtcLikeCoins_BITCOINCASH ){
                    const char * hrp = bitcoinSignReq.testnet ? "bchtest": "bitcoincash";
                    bch_tx_get_output_address(address, tx_out, hrp);
                }
                outinfo.append(address);
                outinfo.append(":");
                outinfo.append(btc_value_pretty(tx_out->value,coin,bitcoinSignReq.testnet));
                outinfo.append("\n");
            }
            if( psbt_get_miner_fee(&p, &miner_fee)){
                outinfo.append("矿工费：");
                outinfo.append(btc_value_pretty(miner_fee,coin,bitcoinSignReq.testnet));
                outinfo.append("\n");
            }
            msgBox.setText(QString::fromUtf8("%1 确认签名:").arg(params->chainname));
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
                        if( !psbt_check_for_sig(&p, (uint32_t)i, &hashType, &errMsg)
                           || !psbt_get_sighash(&p, (uint32_t)i, hashType, hash, &errMsg) ){
                            free(bip32_path);
                            replyError(KEYBOX_ERROR_CLIENT_ISSUE, "psbt can't be signed.");
                            goto _psbt_err_ret;
                        }
                        if( hdnode_sign_digest(&signNode, hash, sig, sig+64, nullptr)==0 ){
                            sig[64] = (uint8_t)hashType;
                            psbt_add_partial_sig(&p, (uint32_t)i, signNode.public_key, sig);
                            someSigned = true;
                        }
                        free(bip32_path);
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
    case MsgTypeEthereumSignRequest:
        CHECK_MWALLET
        decode_status = pb_decode(&stream, EthereumSignRequest_fields, &ethSignReq);
        if( decode_status ){
            // check path first
            struct ethereum_tx tx;
            if( !rlp_parse(ethSignReq.unsignedTx.bytes, ethSignReq.unsignedTx.size, &tx)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid unsignedTx parameter");
                break;
            }
            if( ethereum_tx_has_signature(&tx)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "already signed tx");
                break;
            }
            QString msg = QString::fromUtf8("以太坊交易：\n");
            char temp[100];
            if(! ethereum_tx_get_toaddress(&tx, temp)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid to address");
                break;
            }
            msg.append("to:").append(temp).append("\n");
            if( !ethereum_tx_get_value(&tx, temp)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid value");
                break;
            }
            msg.append("value:").append(temp).append("\n");
            uint32_t nonce;
            if( !ethereum_tx_get_nonce(&tx, &nonce)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid nonce");
                break;
            }
            msg.append(QString("nonce: %1 \n").arg(nonce));

            if( !ethereum_tx_get_gasPrice(&tx, temp)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid gasPrice");
                break;
            }
            msg.append("gas price:").append(temp).append("\n");
            uint64_t limit;
            if( !ethereum_tx_get_gasLimit(&tx, &limit)){
                replyError(KEYBOX_ERROR_CLIENT_ISSUE, "invalid gasLimt");
                break;
            }
            msg.append(QString("gas limit: %1 \n").arg(limit));


            QMessageBox msgBox;
            msgBox.setText("确认ETH签名请求");
            msgBox.setInformativeText(msg);
            msgBox.setStandardButtons(QMessageBox::Ok  | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            int ret = msgBox.exec();
            if(ret == QMessageBox::Cancel ){
                replyError(KEYBOX_ERROR_USER_CANCELLED, "用户拒绝签名");
                break;
            }

            uint8_t hash[32];
            keccak_256(ethSignReq.unsignedTx.bytes, ethSignReq.unsignedTx.size,hash);

            // temp
            // replyError(KEYBOX_ERROR_SERVER_ISSUE, "not yet implemented");
            QByteArray pubkey;
            int32_t errcode;
            QString errMessage;
            EccSignature sig;
            EccSignOptions option = EccSignOptions_init_default;
            option.rfc6979 = true;
            option.graphene_canonize = false;
            mWallet ->eccSign(QString::fromUtf8(ethSignReq.hdPath), QByteArray((char*)hash, 32), option, errcode, errMessage, sig, pubkey);
            if( errcode != 0 ){
                replyError(errcode, errMessage);
                break;
            }
            struct byte_array &r = tx.content[7];
            struct byte_array &s = tx.content[8];
            struct byte_array &v = tx.content[6];
            if( tx.array_len == 6){ // before EIP155
                uint8_t vbyte;
                vbyte = sig.recovery_param + 27;
                v.p = &vbyte;
                v.size = 1;
            }
            else {
                uint32_t chainId;
                uint8_t enc[4];
                uint8_t localS;
                if( !ethereum_tx_get_chainid(&tx, &chainId)){
                    replyError(KEYBOX_ERROR_CLIENT_ISSUE, "cant decode chainId");
                }
                uint32_rlp_encode(chainId*2 + 35 + sig.recovery_param, enc, &localS);
                v.p = enc;
                v.size = localS;
            }

            r.p = ( uint8_t*)sig.R.constData();
            r.size = sig.R.length();
            s.p = (uint8_t*)sig.S.constData();
            s.size = sig.S.length();
            tx.array_len = 9;
            EthereumSignResult result;
            uint32_t outSize;
            if( !rlp_encode(&tx, result.signedTx.bytes, 8192, &outSize)){
                replyError(KEYBOX_ERROR_SERVER_ISSUE, "unexpected encode error.");
                break;
            }
            result.signedTx.size = outSize;
            pb_ostream_t ostream = pb_ostream_from_buffer((uint8_t*)rb.data(), rb.size());
            encode_status = pb_encode(&ostream, EthereumSignResult_fields, &result);
            size = ostream.bytes_written;

            if( encode_status ){
                rb.resize(size);
                mServer -> sendMessage(MsgTypeEthereumSignResult, rb);
            }
            else {
                qFatal("can't encode message, %s", PB_GET_ERROR(&ostream));
            }

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
