#ifndef _SOFTWARE_WALLET_RLP_ENCODED_
#define _SOFTWARE_WALLET_RLP_ENCODED_

#include "ethereum_tx.h"


int rlp_parse(const uint8_t * buf, uint32_t buf_size, struct ethereum_tx * out_tx);

int rlp_encode(const struct ethereum_tx * in_tx, uint8_t * buf, uint32_t in_size, uint32_t * out_size);

void uint32_rlp_encode(uint32_t v, uint8_t buffer[4], uint8_t *outSize);

#endif // _SOFTWARE_WALLET_RLP_ENCODED_
