.phony: all

all: gen_key

VPATH += ../../library
CFLAGS = -I../../include

# TODO: get rid of AES, SHA, MD5, PK, ENTROPY, ASN1, PEM

MBED_TLS = \
	aes.c \
	aesni.c \
	asn1parse.c \
	asn1write.c \
	base64.c \
	bignum.c \
	ctr_drbg.c \
	ecp.c \
	ecp_curves.c \
	entropy.c \
	entropy_poll.c \
	error.c \
	md.c \
	md5.c \
	md_wrap.c \
	oid.c \
	pem.c \
	pk.c \
	pk_wrap.c \
	pkwrite.c \
	sha256.c \
	sha512.c \
	timing.c \

gen_key: gen_key.c $(MBED_TLS)
	gcc $(CFLAGS) $^ -o gen_key

key: gen_key
	./gen_key type=ec ec_curve=secp256r1

clean:
	rm -f gen_key