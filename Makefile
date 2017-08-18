CFLAGS += -Wall -Wextra -O2 -g


all: ddelta ddelta_apply

ddelta: LDFLAGS=-ldivsufsort

ddelta: ddelta_generate.c
ddelta_apply: ddelta_apply.c
