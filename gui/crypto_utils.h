#ifndef _KEYBOX_CRYPTO_UTILS_INCLUDE_
#define _KEYBOX_CRYPTO_UTILS_INCLUDE_

#ifdef __cplusplus
extern "C" {
#endif

#include "bip39.h"
#include "bip32.h"


bool isValidBip32Path(const char *path);
bool getBip32NodeFromPath(const char seed[64], const char *path, HDNode * node);
int isSigGrapheneCanonical(uint8_t by, uint8_t sig[64]);
uint16_t getPathDeriveCount(const char *path);
void bip32getIdentifier(HDNode *node, uint8_t id[32]);



#ifdef __cplusplus
}
#endif

#endif