TEMPLATE = lib
CONFIG += object_parallel_to_source staticlib
TARGET = psbt

PSBT_PATH = ../libpsbt_for_signer
CRYPTO_LIB_PATH = ../crypto_lib
include(../common/common.pri)

PSBT_SRCS = $$files($$PSBT_PATH/src/*.c)


SOURCES += $$PSBT_SRCS

