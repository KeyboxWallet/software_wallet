
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
QMAKE_CFLAGS += -std=gnu99 \
            -W \
            -Wall \
            -Wextra \
            -Wimplicit-function-declaration \
            -Wredundant-decls \
            -Wstrict-prototypes \
            -Wundef \
            -Wshadow \
            -Wpointer-arith \
            -Wformat \
            -Wreturn-type \
            -Wsign-compare \
            -Wmultichar \
            -Wformat-nonliteral \
            -Winit-self \
            -Wuninitialized \
            -Wformat-security \
            -Werror

macx: QMAKE_CFLAGS -= -Werror

QMAKE_CFLAGS += -DUSE_ETHEREUM=1
QMAKE_CFLAGS += -DUSE_GRAPHENE=1
QMAKE_CFLAGS += -DUSE_KECCAK=1
QMAKE_CFLAGS += -DUSE_MONERO=1
QMAKE_CFLAGS += -DUSE_NEM=1
QMAKE_CFLAGS += -DUSE_CARDANO=1

INCLUDEPATH += $$CRYPTO_LIB_PATH
INCLUDEPATH += $$CRYPTO_LIB_PATH/aes
INCLUDEPATH += $$PSBT_PATH/include
