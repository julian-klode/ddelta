CFLAGS += -Wall -Wextra -O2 -g


all: ddelta_generate ddelta_apply

ddelta_generate: LDFLAGS=-ldivsufsort

ddelta_generate: ddelta_generate.c
ddelta_apply: ddelta_apply.c
dpkg-delta: CFLAGS+=-DJKPATCH_NO_MAIN
dpkg-delta: dpkg-delta.c ddelta_apply.c
