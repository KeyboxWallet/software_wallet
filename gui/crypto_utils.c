#include "crypto_utils.h"
#include "curves.h"
#include "aes.h"
#include "bip39.h"
#include "pbkdf2.h"
#include "base64.h"
#include <string.h>
#define UNUSED(x) (void)(x)

static bool isValidPart(const char *part){
    if( !part) {
        return false;
    }
    if( strlen(part) == 0){
        return false;
    }
    char c;
    const char *p = part;
    c = *p++;
    if( ! (c >= '0' && c <= '9')){
        return false;
    }
    if( *p == 0 ){
        return true;
    }
    if( c == '0'){
        return *p == '\'' && *(p+1) == 0;
    }
    if (*p == '\'' && *(p+1) == 0) {
        return true;
    }
    uint32_t value = c - '0';
    uint32_t nValue;
    while (p[1] != 0){
        c = p[0];
        nValue = value * 10 + (c-'0');
        if( nValue < value || nValue >= 0x80000000U ){ //too large.
            return false;
        }
        if( !(c >= '0' && c <= '9')){
            return false;
        }
        p++;
    }
    c = p[0];
    return c =='\'' ||  ( c>='0' && c <='9' );
}

bool isValidBip32Path(const char *path)
{
    if( !path ) {
        return false;
    }
    if (path[0] != 'm' && path[1] != '/' ){
        return false;
    }
    char *splitString = strdup(path + 2);
    char * token = strtok(splitString, "/");
    while (token) {
        if( !isValidPart(token)){
            free(splitString);
            return false;
        }
        token = strtok(NULL, "/");
    }
    free(splitString);
    return true;
}


uint16_t getPathDeriveCount(const char *path)
{
    uint16_t count = 0;
    char c;
    while( (c = *path++) != 0){
        if( c == '/' ){
            count ++;
        }
    }
    return count;
}



bool getBip32NodeFromPath(const char seed[64], const char *path, HDNode * node)
{
    if( !path ) {
        return false;
    }
    if (path[0] != 'm' || path[1] != '/' ){
        return false;
    }
    if (!node) {
        return false;
    }
    int r;
    r = hdnode_from_seed((uint8_t*)seed, 64, SECP256K1_NAME, node);
    if( !r) {
        return false;
    }
    char *splitString = strdup(path + 2);
    char * token = strtok(splitString, "/");
    int index;
    bool hardend;
    while (token) {
        if( !isValidPart(token)){
            free(splitString);
            return false;
        }

        int len = strlen(token);
        hardend = false;
        if (token[len-1] == '\''){
            token[len-1] = 0;
            hardend = true;
        }
        index = atoi(token);
        if( hardend ){
            r = hdnode_private_ckd_prime(node, index);
        }
        else {
            r = hdnode_private_ckd(node, index);
        }
        if (!r) {
            free(splitString);
            return false;
        }

        token = strtok(NULL, "/");
    }
    hdnode_fill_public_key(node);
    free(splitString);
    return true;
}

bool bip32PathDerive( const uint32_t *path, size_t pathCount, HDNode * node)
{
    int r;
    if( !path ) {
        return false;
    }
    if (!node) {
        return false;
    }
    uint32_t index;
    for(size_t i=0; i<pathCount; i++) {
        index = path[i];
        if( index & 0x80000000 ){
            r = hdnode_private_ckd_prime(node, index & ~0x80000000);
        }
        else{
            r = hdnode_private_ckd(node, index);
        }

        if(!r){
            return false;
        }
    }
    hdnode_fill_public_key(node);
    return true;
}


int isSigGrapheneCanonical(uint8_t by, uint8_t sig[64])
{
    UNUSED(by);
    return !(sig[0] & 0x80)
                   && !(sig[0] == 0 && !(sig[1] & 0x80))
                   && !(sig[32] & 0x80)
                   && !(sig[32] == 0 && !(sig[33] & 0x80));
}

void bip32getIdentifier(HDNode *node, uint8_t id[32])
{
    hdnode_fill_public_key(node);
    hasher_Raw(node->curve->hasher_pubkey, node->public_key, 33, id);
}

// 密码扩展算法，加解密通用。
static void getSecretAndIv(const char * password, const char * salt,  uint8_t secret[64]){
    PBKDF2_HMAC_SHA512_CTX pctx;
    pbkdf2_hmac_sha512_Init(&pctx, (const uint8_t*)password, strlen(password), (const uint8_t *)salt, strlen(salt), 0);
    pbkdf2_hmac_sha512_Update(&pctx, 2048 );
    pbkdf2_hmac_sha512_Final(&pctx, secret);
}

const char * mnenoic_salt = "Mnemonic Backup";

int encrypt_mnemonic(const char * mnemonic, const char * password, char out[44])
{
  uint8_t entropy[33];
  uint8_t encrypt[32];
  int bitlen = mnemonic_to_entropy(mnemonic, entropy);
  if( bitlen == 0 ){
     return 0;
  }
  // int bytelen = (bitlen & 0x7) == 0 ? bitlen /8  : bitlen / 8 + 1;
  uint8_t secret[64];
  getSecretAndIv(password, mnenoic_salt, secret);
  aes_encrypt_ctx enc_ctx;
  aes_encrypt_key256(secret, &enc_ctx);
  if( aes_cbc_encrypt(entropy, encrypt, 32, secret + 32, &enc_ctx) == 0 ){
     return b64_encode(encrypt, 32, (unsigned char*)out); // the size of out, should be 44
  }
 else {
     return 0;
 }
}

struct name_lang_map_item {
   const char * name;
   enum bip39_lang lang;
};

static struct name_lang_map_item lang_maps[] = {
  {"en", lang_en},
  {"zh_cn", lang_zh_cn},
  {"zh_tw", lang_zh_tw},
  {"jp", lang_jp},
  {"fr", lang_fr},
  {"ko", lang_ko},
  {"es", lang_es},
  {"cs", lang_cs},
  {"it", lang_it}
};

int decrypt_mnemnic(const char * b64_encrypted, const char * password, const char * language, uint8_t word_len, char * mnemonic)
{
    uint8_t bytelen ;
    if( word_len == 12 || word_len == 15 || word_len == 18 || word_len == 21 || word_len == 24 ){
        bytelen = word_len*11 / 8;
    }
    else {
        return 0;
    }
    if( word_len == 24 ){
        bytelen -= 1;
    }
    uint8_t encrypted[33];
    uint8_t decrypted[32];
    if( b64_decode((const uint8_t*)b64_encrypted, 44, encrypted ) != 32 ){
        return 0;
    }
    int lang_ok = 0;
    for( size_t i=0; i<sizeof(lang_maps)/sizeof(lang_maps[0]); i++){
        if( strcmp(language, lang_maps[i].name) == 0 ){
            if( bip39_set_language(lang_maps[i].lang) == 0){
                return 0;
            }
            else{
                lang_ok = 1;
                break;
            }
        }
    }
    if( !lang_ok ){
        return 0;
    }
    uint8_t secret[64];
    getSecretAndIv(password, mnenoic_salt, secret);
    aes_decrypt_ctx dec_ctx;
    aes_decrypt_key256(secret, &dec_ctx);
    if( aes_cbc_decrypt(encrypted, decrypted, 32, secret + 32, &dec_ctx) == 0 ){
        const char * m = mnemonic_from_data(decrypted, bytelen);
        strcpy(mnemonic, m);
        return strlen(m);
    }
    else {
        return 0;
    }

}

