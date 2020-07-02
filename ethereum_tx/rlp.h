#ifndef _SOFTWARE_WALLET_RLP_ENCODED_
#define _SOFTWARE_WALLET_RLP_ENCODED_

#include <stdint.h>
struct byte_array
{
    uint8_t * p;
    uint32_t size;
};

struct ethereum_tx
{
    uint8_t array_len; // 6 or 9
    struct byte_array content[9];
/*
    byte_array nonce;
    byte_array gasPrice;
    byte_array gas;
    byte_array to;
    byte_array value;
    byte_array data;
    byte_array chainid_or_v; 
    byte_array zero_or_r;
    byte_array zero_or_s;*/
};



int rlp_parse(const uint8_t * buf, uint32_t buf_size, struct ethereum_tx * out_tx);

int rlp_encode(const struct ethereum_tx * in_tx, uint8_t * buf, uint32_t in_size, uint32_t * out_size);


#endif // _SOFTWARE_WALLET_RLP_ENCODED_
