#ifndef _ETHEREUM_TX_INCLUDE_
#define _ETHEREUM_TX_INCLUDE_
#include <stdint.h>
#include "bignum.h"

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
static inline int ba_parse_uint32(const struct  byte_array* ba, uint32_t *out)
{
    if( ba->size == 0){
        *out = 0;
        return 1;
    }
    if( ba->size > 4){
        return 0;
    }
    uint32_t o = 0;
    uint8_t *p = ba->p;
    for( uint8_t i=0; i<ba->size; i++){
        o = o<<8;
        o |= *p++;
    }
    *out = o;
    return 1;
}

static inline int ba_parse_uint64(const struct  byte_array* ba, uint64_t *out)
{
    if( ba->size == 0){
        *out = 0;
        return 1;
    }
    if( ba->size > 8){
        return 0;
    }
    uint64_t o = 0;
    uint8_t *p = ba->p;
    for( uint8_t i=0; i<ba->size; i++){
        o = o<<8;
        o |= *p++;
    }
    *out = o;
    return 1;
}

static inline int ba_parse_bignum(const struct byte_array*ba, bignum256 * out)
{
    bignum256 o, t1, t2;
    bn_zero(&o);
    if( ba->size > 32){
        return 0;
    }
    for( uint8_t i=0; i<ba->size; i++){
        for(uint8_t j=0; j<8; j++)
            bn_lshift(&o);
        bn_zero(&t1);
        bn_addi(&t1, *(ba->p+i));
        bn_xor(&t2, &o, &t1);
        bn_copy(&t2, &o);
    }
    bn_copy(&o, out);
    return 1;  
}



static inline int ethereum_tx_get_nonce(const struct ethereum_tx * tx, uint32_t * nonce)
{
    if( tx->array_len >= 1){
        return ba_parse_uint32(&tx->content[0], nonce);
    }
    else {
        return 0;
    }
}

static inline int ethereum_tx_get_gasPrice(const struct ethereum_tx * tx, uint64_t * price)
{
    if( tx->array_len >= 2){
        return ba_parse_uint64(&tx->content[1], price);
    }
    else {
        return 0;
    }
}

static inline int ethereum_tx_get_gasLimit(const struct ethereum_tx * tx, uint64_t * limit)
{
    if( tx->array_len >= 3){
        return ba_parse_uint64(&tx->content[2], limit);
    }
    else {
        return 0;
    }
}

static void bin_to_hex(unsigned char* bin_in, size_t inlen, char* hex_out)
{
    static char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < inlen; i++) {
        hex_out[i * 2] = digits[(bin_in[i] >> 4) & 0xF];
        hex_out[i * 2 + 1] = digits[bin_in[i] & 0xF];
    }
    hex_out[inlen * 2] = '\0';
}

static inline int ethereum_tx_get_toaddress(const struct ethereum_tx *tx, char hex_address[43])
{
    if( tx->array_len >= 4){
        if(tx->content[3].size != 20){
            return 0;
        }
        hex_address[0] = '0';
        hex_address[1] = 'x';
        bin_to_hex(tx->content[3].p, tx->content[3].size, hex_address+2);
        return 1;
    }
    else 
        return 0;
}


static inline int ethereum_tx_get_value(const struct ethereum_tx *tx, char value[90])
{
    bignum256 v;
    bignum256 t1, t2;
    bn_read_uint32(10000000U, &t1);
    bn_read_uint64(1000000000000000UL, &t2);
    if( tx->array_len >= 5){
        if( ba_parse_bignum(&tx->content[4], &v) == 0 ){
            return 0;
        }
        if( bn_is_less(&v, &t1 )){
            return bn_format(&v, "", " WEI", 0, 0, false, value, 90);
        }
        if( bn_is_less(&v, &t2)) {
            return bn_format(&v, "", " GWEI", 9, 0, false, value, 90 );
        }
        return bn_format(&v,"", " ETH", 18, 0, false, value, 90 );
    }
    return 0;
}

static inline bool ethereum_tx_has_signature(const struct ethereum_tx *tx)
{
    if(tx->array_len == 9 ){
        if( tx->content[6].size == 1 
        && tx->content[7].size > 20
        && tx->content[8].size > 20
        )
        return true;
    }
    return false;
}

static inline int ethereum_tx_get_chainid(const struct ethereum_tx *tx, uint32_t *chainId )
{
    if(tx->array_len >= 7){
        return ba_parse_uint32(&tx->content[6], chainId);
    }
    return 0;
}

#endif // 