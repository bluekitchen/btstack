#include <stdio.h>
#include <stdint.h>
#include "btstack_util.h"
#include "aes_cmac.h"
#include <errno.h>

typedef uint8_t key_t[16];

#define LOG_KEY(NAME) { printf("%16s: ", #NAME); printf_hexdump(NAME, 16); }
#define PARSE_KEY(NAME) { parse_hex(NAME, NAME##_string); LOG_KEY(NAME); }
#define DEFINE_KEY(NAME, VALUE) key_t NAME; parse_hex(NAME, VALUE); LOG_KEY(NAME);

static int parse_hex(uint8_t * buffer, const char * hex_string){
	int len = 0;
	while (*hex_string){
		if (*hex_string == ' '){
			hex_string++;
			continue;
		}
		int high_nibble = nibble_for_char(*hex_string++);
		int low_nibble = nibble_for_char(*hex_string++);
		*buffer++ = (high_nibble << 4) | low_nibble;
		len++;
	}
	return len;
}

// CCM Encrypt & Decrypt from Zephyr Project

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint64_t u64_t;

static void sys_put_be16(uint16_t value, uint8_t * buffer) {
	big_endian_store_16(buffer, 0, value);
}
static int bt_encrypt_be(const uint8_t * key, const uint8_t * plain, uint8_t * cipher) {
	aes128_calc_cyphertext(key, plain, cipher);
	return 0;
}

static int bt_mesh_ccm_decrypt(const u8_t key[16], u8_t nonce[13],
			       const u8_t *enc_msg, size_t msg_len,
			       const u8_t *aad, size_t aad_len,
			       u8_t *out_msg, size_t mic_size)
{
	u8_t msg[16], pmsg[16], cmic[16], cmsg[16], Xn[16], mic[16];
	u16_t last_blk, blk_cnt;
	size_t i, j;
	int err;

	if (msg_len < 1 || aad_len >= 0xff00) {
		return -EINVAL;
	}

	/* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
	pmsg[0] = 0x01;
	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(0x0000, pmsg + 14);

	err = bt_encrypt_be(key, pmsg, cmic);
	if (err) {
		return err;
	}

	/* X_0 = e(AppKey, 0x09 || nonce || length) */
	if (mic_size == sizeof(u64_t)) {
		pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
	} else {
		pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
	}

	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(msg_len, pmsg + 14);

	err = bt_encrypt_be(key, pmsg, Xn);
	if (err) {
		return err;
	}

	/* If AAD is being used to authenticate, include it here */
	if (aad_len) {
		sys_put_be16(aad_len, pmsg);

		for (i = 0; i < sizeof(u16_t); i++) {
			pmsg[i] = Xn[i] ^ pmsg[i];
		}

		j = 0;
		aad_len += sizeof(u16_t);
		while (aad_len > 16) {
			do {
				pmsg[i] = Xn[i] ^ aad[j];
				i++, j++;
			} while (i < 16);

			aad_len -= 16;
			i = 0;

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}
		}

		for (i = 0; i < aad_len; i++, j++) {
			pmsg[i] = Xn[i] ^ aad[j];
		}

		for (i = aad_len; i < 16; i++) {
			pmsg[i] = Xn[i];
		}

		err = bt_encrypt_be(key, pmsg, Xn);
		if (err) {
			return err;
		}
	}

	last_blk = msg_len % 16;
	blk_cnt = (msg_len + 15) / 16;
	if (!last_blk) {
		last_blk = 16;
	}

	for (j = 0; j < blk_cnt; j++) {
		if (j + 1 == blk_cnt) {
			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < last_blk; i++) {
				msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
			}

			memcpy(out_msg + (j * 16), msg, last_blk);

			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < last_blk; i++) {
				pmsg[i] = Xn[i] ^ msg[i];
			}

			for (i = last_blk; i < 16; i++) {
				pmsg[i] = Xn[i] ^ 0x00;
			}

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

			/* MIC = C_mic ^ X_1 */
			for (i = 0; i < sizeof(mic); i++) {
				mic[i] = cmic[i] ^ Xn[i];
			}
		} else {
			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < 16; i++) {
				msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
			}

			memcpy(out_msg + (j * 16), msg, 16);

			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < 16; i++) {
				pmsg[i] = Xn[i] ^ msg[i];
			}

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}
		}
	}

	if (memcmp(mic, enc_msg + msg_len, mic_size)) {
		return -EBADMSG;
	}

	return 0;
}
static int bt_mesh_ccm_encrypt(const u8_t key[16], u8_t nonce[13],
			       const u8_t *msg, size_t msg_len,
			       const u8_t *aad, size_t aad_len,
			       u8_t *out_msg, size_t mic_size)
{
	u8_t pmsg[16], cmic[16], cmsg[16], mic[16], Xn[16];
	u16_t blk_cnt, last_blk;
	size_t i, j;
	int err;

	// BT_DBG("key %s", bt_hex(key, 16));
	// BT_DBG("nonce %s", bt_hex(nonce, 13));
	// BT_DBG("msg (len %zu) %s", msg_len, bt_hex(msg, msg_len));
	// BT_DBG("aad_len %zu mic_size %zu", aad_len, mic_size);

	/* Unsupported AAD size */
	if (aad_len >= 0xff00) {
		return -EINVAL;
	}

	/* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
	pmsg[0] = 0x01;
	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(0x0000, pmsg + 14);

	err = bt_encrypt_be(key, pmsg, cmic);
	if (err) {
		return err;
	}

	/* X_0 = e(AppKey, 0x09 || nonce || length) */
	if (mic_size == sizeof(u64_t)) {
		pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
	} else {
		pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
	}

	memcpy(pmsg + 1, nonce, 13);
	sys_put_be16(msg_len, pmsg + 14);

	err = bt_encrypt_be(key, pmsg, Xn);
	if (err) {
		return err;
	}

	/* If AAD is being used to authenticate, include it here */
	if (aad_len) {
		sys_put_be16(aad_len, pmsg);

		for (i = 0; i < sizeof(u16_t); i++) {
			pmsg[i] = Xn[i] ^ pmsg[i];
		}

		j = 0;
		aad_len += sizeof(u16_t);
		while (aad_len > 16) {
			do {
				pmsg[i] = Xn[i] ^ aad[j];
				i++, j++;
			} while (i < 16);

			aad_len -= 16;
			i = 0;

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}
		}

		for (i = 0; i < aad_len; i++, j++) {
			pmsg[i] = Xn[i] ^ aad[j];
		}

		for (i = aad_len; i < 16; i++) {
			pmsg[i] = Xn[i];
		}

		err = bt_encrypt_be(key, pmsg, Xn);
		if (err) {
			return err;
		}
	}

	last_blk = msg_len % 16;
	blk_cnt = (msg_len + 15) / 16;
	if (!last_blk) {
		last_blk = 16;
	}

	for (j = 0; j < blk_cnt; j++) {
		if (j + 1 == blk_cnt) {
			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < last_blk; i++) {
				pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
			}
			for (i = last_blk; i < 16; i++) {
				pmsg[i] = Xn[i] ^ 0x00;
			}

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

			/* MIC = C_mic ^ X_1 */
			for (i = 0; i < sizeof(mic); i++) {
				mic[i] = cmic[i] ^ Xn[i];
			}

			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_1 */
			for (i = 0; i < last_blk; i++) {
				out_msg[(j * 16) + i] =
					msg[(j * 16) + i] ^ cmsg[i];
			}
		} else {
			/* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
			for (i = 0; i < 16; i++) {
				pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
			}

			err = bt_encrypt_be(key, pmsg, Xn);
			if (err) {
				return err;
			}

			/* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
			pmsg[0] = 0x01;
			memcpy(pmsg + 1, nonce, 13);
			sys_put_be16(j + 1, pmsg + 14);

			err = bt_encrypt_be(key, pmsg, cmsg);
			if (err) {
				return err;
			}

			/* Encrypted = Payload[0-15] ^ C_N */
			for (i = 0; i < 16; i++) {
				out_msg[(j * 16) + i] =
					msg[(j * 16) + i] ^ cmsg[i];
			}

		}
	}

	memcpy(out_msg + msg_len, mic, mic_size);

	return 0;
}

static void message_24(void){
	DEFINE_KEY(encryption_key, "0953fa93e7caac9638f58820220a398e");

	uint8_t network_nonce[13];
	parse_hex(network_nonce, "000307080d1234000012345677");
	printf("%16s: ", "network_nonce"); printf_hexdump(network_nonce, 13);

	uint8_t plaintext[18];
	parse_hex(plaintext, "9736e6a03401de1547118463123e5f6a17b9");
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, sizeof(plaintext));

	uint8_t ciphertext[18+4];
	bt_mesh_ccm_encrypt(encryption_key, network_nonce, plaintext, sizeof(plaintext), NULL, 0, ciphertext, 4);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 18);
	printf("%16s: ", "NetMIC");     printf_hexdump(&ciphertext[18], 4);
}

int main(void){
	message_24();
	return 0;
}
