TEMPLATE = lib
CONFIG += object_parallel_to_source
CONFIG += staticlib

CRYPTO_LIB_PATH = ../../crypto_lib
include(../../common/common.pri)

ED25519_SRCS = $$files($$CRYPTO_LIB_PATH/ed25519-donna/*.c)

# message($$ED25519_SRCS)

CRYPTO_SRCS += $$ED25519_SRCS


SOURCES += $$CRYPTO_SRCS
