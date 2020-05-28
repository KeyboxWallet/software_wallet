TEMPLATE = lib
CONFIG += object_parallel_to_source staticlib
TARGET = crypto_lib

CRYPTO_LIB_PATH = ../../crypto_lib
include(../../common/common.pri)

CRYPTO_SRCS = $$files($$CRYPTO_LIB_PATH/*.c)
AES_SRCS = $$files($$CRYPTO_LIB_PATH/aes/*.c)
AES_SRCS -= $$CRYPTO_LIB_PATH/aes/aestst.c
CHACHA20POLY1305_SRCS = $$files($$CRYPTO_LIB_PATH/chacha20poly1305/*.c)
MONERO_SRCS = $$files($$CRYPTO_LIB_PATH/monero/*.c)

CRYPTO_SRCS += $$CHACHA20POLY1305_SRCS
CRYPTO_SRCS += $$AES_SRCS
CRYPTO_SRCS += $$MONERO_SRCS

CRYPTO_HDRS = $$files(./$$CRYPTO_LIB_PATH/*.h, true)

SOURCES += $$CRYPTO_SRCS

# there is a bug in qmake, so need to include this file in different project files.
SOURCES += $$CRYPTO_LIB_PATH/ed25519-donna/ed25519.c

