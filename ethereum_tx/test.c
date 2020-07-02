#include "rlp.h"
#include "utils.h"
#include <assert.h>

char * txS[] = {
    "f86b80850861c4680082a4109425f7c8e6624edb5e288a6ba893cd18e1bc98bc7c871c6bf5263400008026a0e376296ec72dfa0d12f0b1131e7492c2d4fe3eaeba6d2520e1b3624488aae628a05a93b63a893969988134324ec4b10d3907b56f8b63938d45ca39f28bf5033e7b",
    "f8a980850ba43b77e882a9009496a62428509002a7ae5f6ad29e4750d852a3f3d780b844a9059cbb00000000000000000000000025f7c8e6624edb5e288a6ba893cd18e1bc98bc7c000000000000000000000000000000000000000000000000016345785d8a000026a046b7b76cd6b5237c31d21fafe68191f0c52c0b5e94768dec9e3a69c1152d2552a027d27665a38fdced7b1ea0737720e8cd7a1dd72fa614c4a5ee25c2fecc1bf305"
};


uint8_t dec_buffer[8000];
uint8_t enc_buffer[8000];


void main()
{
    size_t i;
    int decSize, encSize;
    int ret;
    struct ethereum_tx tx;
    for(i=0; i<sizeof(txS)/sizeof(txS[0]); i++ ){
        char * p = txS[i];
        utils_hex_to_bin(p, dec_buffer, strlen(txS[i]), &decSize);
        ret = rlp_parse(dec_buffer, decSize, &tx);
        assert(ret == 1);
        ret = rlp_encode(&tx, enc_buffer, 8000, &encSize);
        assert(ret == 1);
        assert(decSize == encSize);
        assert(memcmp(dec_buffer,enc_buffer, encSize) == 0);
    }


}