
CRYPTO_LIB_PATH = ../crypto_lib
AES_LIB_PATH = $$CRYPTO_LIB_PATH/aes

INCLUDEPATH += $$CRYPTO_LIB_PATH
INCLUDEPATH += $$CRYPTO_LIB_PATH/aes


SOURCES += $$CRYPTO_LIB_PATH/bip39.c \
           $$CRYPTO_LIB_PATH/hmac.c \
           $$CRYPTO_LIB_PATH/memzero.c \
           $$CRYPTO_LIB_PATH/pbkdf2.c \
           $$CRYPTO_LIB_PATH/rand.c \
           $$CRYPTO_LIB_PATH/sha2.c \
           $$CRYPTO_LIB_PATH/ecdsa.c \
           $$CRYPTO_LIB_PATH/bip32.c \
           $$CRYPTO_LIB_PATH/bignum.c \
           $$CRYPTO_LIB_PATH/curves.c \
           $$CRYPTO_LIB_PATH/base58.c \
           $$CRYPTO_LIB_PATH/address.c \
           $$CRYPTO_LIB_PATH/ripemd160.c \
           $$CRYPTO_LIB_PATH/secp256k1.c \
           $$CRYPTO_LIB_PATH/nist256p1.c \
           $$CRYPTO_LIB_PATH/hasher.c \
           $$CRYPTO_LIB_PATH/blake256.c \
           $$CRYPTO_LIB_PATH/blake2b.c \
           $$CRYPTO_LIB_PATH/blake2s.c \
           $$CRYPTO_LIB_PATH/groestl.c \
           $$AES_LIB_PATH/aes_modes.c \
           $$AES_LIB_PATH/aescrypt.c \
           $$AES_LIB_PATH/aestab.c \
           $$AES_LIB_PATH/aeskey.c \

linux {
 QMAKE_CFLAGS += -DOS_LINUX
}
macx {
 QMAKE_CFLAGS += -DOS_APPLE
}
win32 {
 QMAKE_CFLAGS += -DOS_WIN
}

QMAKE_CFLAGS += -DRAND_PLATFORM_INDEPENDENT
SOURCES += safe_rand.c \
           crypto_utils.c

HEADERS += crypto_utils.h

