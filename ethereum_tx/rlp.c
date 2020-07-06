#include "rlp.h"
#include <string.h>

enum RLP_TYPE {
    type_buffer,
    type_list
};

struct rlp_parsed_item {
    enum RLP_TYPE type;
    struct byte_array data;
};

static void skip_byte_array(struct byte_array *inout, uint32_t size)
{
    inout->p += size;
    inout->size -= size;
}

static int rlp_parse_item(struct byte_array *inout, struct rlp_parsed_item * out )
{
    if( inout->size < 1){
        return 0;
    }
    uint8_t first = *inout->p;
    if( first < 128 ){
        out->type = type_buffer;
        out->data.p = inout->p;
        out->data.size = 1;
        skip_byte_array(inout, 1);
    }
    else if( first < 128+56 ){
        out->type = type_buffer;
        skip_byte_array(inout, 1);
        out->data.p = inout->p;
        out->data.size = first - 128;
        skip_byte_array(inout, out->data.size);
    }
    else if( first < 192) {
        out->type = type_buffer;
        uint8_t lenlen = first - 183;
        skip_byte_array(inout, 1);
        if(lenlen > 4){
            return 0; // not supported
        }
        uint32_t len=0;
        while(lenlen >0){
            len = len << 8;
            len += *inout->p;
            lenlen --;
            skip_byte_array(inout, 1);
        }
        out->data.p = inout->p;
        out->data.size = len;
        skip_byte_array(inout, len);
    }
    else if( first < 192+56 ){
        out->type = type_list;
        skip_byte_array(inout, 1);
        out->data.p = inout->p;
        out->data.size = first - 192;
        skip_byte_array(inout, out->data.size);
    }
    else {
        out->type = type_list;
        uint8_t lenlen = first - (192+55);
        skip_byte_array(inout, 1);
        if(lenlen > 4){
            return 0; // not supported
        }
        uint32_t len=0;
        while(lenlen >0){
            len = len << 8;
            len += *inout->p;
            lenlen --;
            skip_byte_array(inout, 1);
        }
        out->data.p = inout->p;
        out->data.size = len;
        skip_byte_array(inout, len);
    }
    return 1;
}


int rlp_parse(const uint8_t * buf, uint32_t buf_size, struct ethereum_tx * out_tx)
{
    if( !buf || buf_size < 6 || !out_tx){
        return 0;
    }
    struct byte_array ba;
    struct rlp_parsed_item item;
    ba.p = (uint8_t*)buf;
    ba.size = buf_size;
    if( rlp_parse_item(&ba, &item) == 0 ){
        return 0;
    }
    if( item.type != type_list || ba.size != 0){
        return 0;
    }
    ba = item.data;
    uint8_t i;
    for( i=0; i<9 && ba.size > 0; i++){
        if( rlp_parse_item(&ba, &item) == 0){
            return 0;
        }
        if( item.type != type_buffer ){
            return 0;
        }
        out_tx->content[i] = item.data;
    }
    //
    //out_tx->nonce = item.data;
    out_tx->array_len = i;
    int ret = ( i==6 || i==9 ) && ba.size == 0;
    return ret;
}

#define CHECK_SIZE(n) \
if( ba->size < (n)){ \
    return 0; \
}

static int rlp_encode_uint(struct byte_array * ba, uint32_t v)
{
    if( v == 0 ){
        CHECK_SIZE(1);
        * ba->p = 128;
        skip_byte_array(ba, 1);
    }
    else if( v < 0x100 ){
        CHECK_SIZE(1);
        * ba->p = v;
        skip_byte_array(ba, 1);
    }
    else if( v < 0x10000){
        CHECK_SIZE(2);
        ba->p[0] = (v & 0xFF00) >> 8;
        ba->p[1] = v & 0xFF;
        skip_byte_array(ba, 2);
    }
    else if( v < 0x1000000){
        CHECK_SIZE(3);
        ba->p[0] = (v >> 16) & 0xFF;
        ba->p[1] = (v>>8) & 0xFF;
        ba->p[2] = v & 0xFF;
        skip_byte_array(ba, 3);
    }
    else {
        CHECK_SIZE(4);
        ba->p[0] = (v>>24) & 0xFF;
        ba->p[1] = (v>>16) & 0xFF;
        ba->p[2] = (v>>8) & 0xFF;
        ba->p[3] = v & 0xFF;
        skip_byte_array(ba, 4);
    }
    return 1;
}

static int rlp_encode_item(struct byte_array *ba, const struct rlp_parsed_item *item)
{

    if(item->type == type_buffer){
        if( item->data.size == 1 && (*item->data.p < 128)){
            CHECK_SIZE(1);
            * ba->p = * item->data.p;
            skip_byte_array(ba, 1);
        }
        else if( item->data.size < 56){
            CHECK_SIZE(1);
            * ba->p = item->data.size + 128;
            skip_byte_array(ba, 1);
            CHECK_SIZE(item->data.size);
            memmove(ba->p, item->data.p, item->data.size);
            skip_byte_array(ba, item->data.size);
        }
        else{
            CHECK_SIZE(1);
            if( item->data.size < 0x100 ){
                * ba->p = 184;
            }
            else if( item->data.size < 0x10000 ){
                * ba->p = 185;
            }
            else if( item->data.size < 0x1000000){
                * ba->p = 186;
            }
            else {
                * ba->p = 187;
            }
            skip_byte_array(ba, 1);
            if( rlp_encode_uint(ba, item->data.size) == 0){
                return 0;
            }
            CHECK_SIZE(item->data.size);
            memmove(ba->p, item->data.p, item->data.size);
            skip_byte_array(ba, item->data.size);
        }
    }
    else if(item->type == type_list){
        if( item->data.size < 56){
            CHECK_SIZE(1);
            * ba->p = item->data.size + 192;
            skip_byte_array(ba, 1);
            CHECK_SIZE(item->data.size);
            memcpy(ba->p, item->data.p, item->data.size);
            skip_byte_array(ba, item->data.size);
        }
        else{
            CHECK_SIZE(1);
            if( item->data.size < 0x100 ){
                * ba->p = 192+56;
            }
            else if( item->data.size < 0x10000 ){
                * ba->p = 192+57;
            }
            else if( item->data.size < 0x1000000){
                * ba->p = 192+58;
            }
            else {
                * ba->p = 192+59;
            }
            skip_byte_array(ba, 1);
            if( rlp_encode_uint(ba, item->data.size) == 0){
                return 0;
            }
            CHECK_SIZE(item->data.size);
            memcpy(ba->p, item->data.p, item->data.size);
            skip_byte_array(ba, item->data.size);
        }
    }
    return 1;
}


int rlp_encode(const struct ethereum_tx * in_tx, uint8_t * buf, uint32_t in_size, uint32_t * out_size)
{
    if( in_tx->array_len != 6 && in_tx->array_len != 9){
        return 0;
    }
    uint8_t i;
    struct byte_array ba;
    if( in_size < 10){
        return 0;
    }
    ba.p = buf + 10;
    ba.size = in_size - 10;
    struct rlp_parsed_item item;
    for( i=0; i<in_tx->array_len; i++){
        item.type = type_buffer;
        item.data = in_tx->content[i];
        if( rlp_encode_item(&ba, &item) == 0){
            return 0;
        }
    }
    uint32_t size  = ba.p - buf - 10;
    item.type = type_list;
    item.data.p = buf + 10;
    item.data.size = size;
    ba.p = buf;
    ba.size = in_size;
    if( rlp_encode_item(&ba, &item ) == 0){
        return 0;
    }
    *out_size = ba.p - buf;
    return 1;
}

void uint32_rlp_encode(uint32_t v, uint8_t buffer[4], uint8_t *outSize)
{
    struct byte_array ba;
    ba.p = buffer;
    ba.size = 4;
    (void)rlp_encode_uint(&ba, v);
    *outSize = 4-ba.size;
}
