#include "crypto_utils.h"
#include "curves.h"
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

