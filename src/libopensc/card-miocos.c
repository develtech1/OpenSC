/*
 * card-miocos.c: Support for PKI cards by Miotec
 *
 * Copyright (C) 2002  Juha Yrj�l� <juha.yrjola@iki.fi>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sc-internal.h"
#include "sc-log.h"
#include <stdlib.h>
#include <string.h>

static struct sc_atr_table miocos_atrs[] = {
	{ (const u8 *) "\x3B\x9D\x94\x40\x23\x00\x68\x10\x11\x4D\x69\x6F\x43\x4F\x53\x00\x90\x00", 18 },
	{ NULL }
};

struct miocos_priv_data {
	int type;
};

#define DRVDATA(card)        ((struct miocos_priv_data *) ((card)->drv_data))

static struct sc_card_operations miocos_ops;
static const struct sc_card_driver miocos_drv = {
	"MioCOS 1.1 cards",
	"miocos",
	&miocos_ops
};

static int miocos_finish(struct sc_card *card)
{
	return 0;
}

static int miocos_match_card(struct sc_card *card)
{
	int i;

	i = _sc_match_atr(card, miocos_atrs, NULL);
	if (i < 0)
		return 0;
	return 1;
}

static int miocos_init(struct sc_card *card)
{
	int i, id;
	struct miocos_priv_data *priv = NULL;

	i = _sc_match_atr(card, miocos_atrs, &id);
	if (i < 0)
		return 0;
	priv = malloc(sizeof(struct miocos_priv_data));
	if (priv == NULL)
		return SC_ERROR_OUT_OF_MEMORY;
	card->drv_data = priv;
	card->cla = 0x00;
	if (1) {
		unsigned long flags;
		
		flags = SC_ALGORITHM_RSA_RAW | SC_ALGORITHM_RSA_PAD_PKCS1;
		flags |= SC_ALGORITHM_RSA_HASH_NONE | SC_ALGORITHM_RSA_HASH_SHA1;

		_sc_card_add_rsa_alg(card, 1024, flags, 0);
	}

	return 0;
}

static const struct sc_card_operations *iso_ops = NULL;

static u8 acl_to_byte(const struct sc_acl_entry *e)
{
	switch (e->method) {
	case SC_AC_NONE:
		return 0x00;
	case SC_AC_CHV:
		switch (e->key_ref) {
		case 1:
			return 0x01;
			break;
		case 2:
			return 0x02;
			break;
		default:
			return 0x00;
		}
		break;
	case SC_AC_TERM:
		return 0x04;
	case SC_AC_NEVER:
		return 0x0F;
	}
	return 0x00;
}

static int encode_file_structure(struct sc_card *card, const struct sc_file *file,
				 u8 *buf, size_t *buflen)
{
	u8 *p = buf;
	const int df_ops[8] = {
		SC_AC_OP_DELETE, SC_AC_OP_CREATE,
		-1, /* CREATE AC */ -1, /* UPDATE AC */ -1, -1, -1, -1
	};
	const int ef_ops[8] = {
		SC_AC_OP_DELETE, -1, SC_AC_OP_READ,
		SC_AC_OP_UPDATE, -1, -1, SC_AC_OP_INVALIDATE,
		SC_AC_OP_REHABILITATE
	};
	const int key_ops[8] = {
		SC_AC_OP_DELETE, -1, -1,
		SC_AC_OP_UPDATE, SC_AC_OP_CRYPTO, -1, SC_AC_OP_INVALIDATE,
		SC_AC_OP_REHABILITATE
	};
        const int *ops;
        int i;

	*p++ = file->id >> 8;
	*p++ = file->id & 0xFF;
	switch (file->type) {
	case SC_FILE_TYPE_DF:
		*p++ = 0x20;
		break;
	case SC_FILE_TYPE_WORKING_EF:
		switch (file->ef_structure) {
		case SC_FILE_EF_TRANSPARENT:
			*p++ = 0x40;
			break;
		case SC_FILE_EF_LINEAR_FIXED:
			*p++ = 0x41;
                        break;
		case SC_FILE_EF_CYCLIC:
			*p++ = 0x43;
			break;
		default:
			error(card->ctx, "Invalid EF structure\n");
			return SC_ERROR_INVALID_ARGUMENTS;
		}
		break;
	case SC_FILE_TYPE_INTERNAL_EF:
		*p++ = 0x44;
		break;
	default:
		error(card->ctx, "Unknown file type\n");
                return SC_ERROR_INVALID_ARGUMENTS;
	}
	if (file->type == SC_FILE_TYPE_DF) {
		*p++ = 0;
		*p++ = 0;
	} else {
		*p++ = file->size >> 8;
		*p++ = file->size & 0xFF;
	}
	switch (file->type) {
	case SC_FILE_TYPE_WORKING_EF:
		ops = ef_ops;
		break;
	case SC_FILE_TYPE_INTERNAL_EF:
		ops = key_ops;
		break;
	case SC_FILE_TYPE_DF:
		ops = df_ops;
		break;
	default:
                return SC_ERROR_INVALID_ARGUMENTS;
	}
	if (file->sec_attr_len == 4) {
		memcpy(p, file->sec_attr, 4);
		p += 4;
	} else for (i = 0; i < 8; i++) {
		u8 nibble;

		if (ops[i] == -1)
			nibble = 0x00;
		else
			nibble = acl_to_byte(sc_file_get_acl_entry(file, ops[i]));
		if ((i & 1) == 0)
			*p = nibble << 4;
		else
			*p++ = nibble & 0x0F;
	}
	if (file->type == SC_FILE_TYPE_WORKING_EF &&
	    file->ef_structure != SC_FILE_EF_TRANSPARENT)
                *p++ = file->record_length;
	else
		*p++ = 0;
	if (file->status & SC_FILE_STATUS_INVALIDATED)
		*p++ = 0;
	else
		*p++ = 0x01;
	if (file->type == SC_FILE_TYPE_DF && file->namelen) {
                assert(file->namelen <= 16);
		memcpy(p, file->name, file->namelen);
		p += file->namelen;
	}
	*buflen = p - buf;

        return 0;
}

static int miocos_create_file(struct sc_card *card, struct sc_file *file)
{
	struct sc_apdu apdu;
	u8 sbuf[32];
        size_t buflen;
	int r;

	r = encode_file_structure(card, file, sbuf, &buflen);
	if (r)
		return r;
	sc_format_apdu(card, &apdu, SC_APDU_CASE_3_SHORT, 0xE0, 0x00, 0x00);
	apdu.data = sbuf;
	apdu.datalen = buflen;
	apdu.lc = buflen;

	r = sc_transmit_apdu(card, &apdu);
        SC_TEST_RET(card->ctx, r, "APDU transmit failed");
        r = sc_check_sw(card, apdu.sw1, apdu.sw2);
        SC_TEST_RET(card->ctx, r, "Card returned error");

	return 0;
}

static int miocos_set_security_env(struct sc_card *card,
				  const struct sc_security_env *env,
				  int se_num)
{
	if (env->flags & SC_SEC_ENV_ALG_PRESENT) {
		struct sc_security_env tmp;

		tmp = *env;
		tmp.flags &= ~SC_SEC_ENV_ALG_PRESENT;
		tmp.flags |= SC_SEC_ENV_ALG_REF_PRESENT;
		if (tmp.algorithm != SC_ALGORITHM_RSA) {
			error(card->ctx, "Only RSA algorithm supported.\n");
			return SC_ERROR_NOT_SUPPORTED;
		}
		tmp.algorithm_ref = 0x00;
		/* potential FIXME: return an error, if an unsupported
		 * pad or hash was requested, although this shouldn't happen.
		 */
		if (env->algorithm_flags & SC_ALGORITHM_RSA_PAD_PKCS1)
			tmp.algorithm_ref = 0x02;
		if (tmp.algorithm_flags & SC_ALGORITHM_RSA_HASH_SHA1)
			tmp.algorithm_ref |= 0x10;
		return iso_ops->set_security_env(card, &tmp, se_num);
	}
        return iso_ops->set_security_env(card, env, se_num);
}

static void add_acl_entry(struct sc_file *file, int op, u8 byte)
{
	unsigned int method, key_ref = SC_AC_KEY_REF_NONE;

	switch (byte >> 4) {
	case 0:
		method = SC_AC_NONE;
		break;
	case 1:
		method = SC_AC_CHV;
		key_ref = 1;
		break;
	case 2:
		method = SC_AC_CHV;
		key_ref = 2;
		break;
	case 4:
		method = SC_AC_TERM;
		break;
	case 15:
		method = SC_AC_NEVER;
		break;
	default:
		method = SC_AC_UNKNOWN;
		break;
	}
	sc_file_add_acl_entry(file, op, method, key_ref);
}

static void parse_sec_attr(struct sc_file *file, const u8 *buf, size_t len)
{
	int i;
	const int df_ops[8] = {
		SC_AC_OP_DELETE, SC_AC_OP_CREATE,
		-1, /* CREATE AC */ -1, /* UPDATE AC */ -1, -1, -1, -1
	};
	const int ef_ops[8] = {
		SC_AC_OP_DELETE, -1, SC_AC_OP_READ,
		SC_AC_OP_UPDATE, -1, -1, SC_AC_OP_INVALIDATE,
		SC_AC_OP_REHABILITATE
	};
	const int key_ops[8] = {
		SC_AC_OP_DELETE, -1, -1,
		SC_AC_OP_UPDATE, SC_AC_OP_CRYPTO, -1, SC_AC_OP_INVALIDATE,
		SC_AC_OP_REHABILITATE
	};
        const int *ops;

	if (len < 4)
                return;
	switch (file->type) {
	case SC_FILE_TYPE_WORKING_EF:
		ops = ef_ops;
		break;
	case SC_FILE_TYPE_INTERNAL_EF:
		ops = key_ops;
		break;
	case SC_FILE_TYPE_DF:
		ops = df_ops;
		break;
	default:
                return;
	}
	for (i = 0; i < 8; i++) {
		if (ops[i] == -1)
			continue;
		if ((i & 1) == 0)
			add_acl_entry(file, ops[i], buf[i / 2] >> 4);
		else
			add_acl_entry(file, ops[i], buf[i / 2] & 0x0F);
	}
}

static int miocos_select_file(struct sc_card *card,
			       const struct sc_path *in_path,
			       struct sc_file **file)
{
	int r;

	r = iso_ops->select_file(card, in_path, file);
	if (r)
		return r;
	if (file != NULL)
		parse_sec_attr(*file, (*file)->sec_attr, (*file)->sec_attr_len);
	return 0;
}

static int miocos_list_files(struct sc_card *card, u8 *buf, size_t buflen)
{
	struct sc_apdu apdu;
	int r;

	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0xCA, 0x01, 0);
	apdu.resp = buf;
	apdu.resplen = buflen;
	apdu.le = buflen > 256 ? 256 : buflen;
	r = sc_transmit_apdu(card, &apdu);
	SC_TEST_RET(card->ctx, r, "APDU transmit failed");
	if (apdu.resplen == 0)
		return sc_check_sw(card, apdu.sw1, apdu.sw2);
	return apdu.resplen;
}

static int miocos_delete_file(struct sc_card *card, const struct sc_path *path)
{
	int r;
	struct sc_apdu apdu;

	SC_FUNC_CALLED(card->ctx, 1);
	if (path->type != SC_PATH_TYPE_FILE_ID && path->len != 2) {
		error(card->ctx, "File type has to be SC_PATH_TYPE_FILE_ID\n");
		SC_FUNC_RETURN(card->ctx, 1, SC_ERROR_INVALID_ARGUMENTS);
	}
	r = sc_select_file(card, path, NULL);
	SC_TEST_RET(card->ctx, r, "Unable to select file to be deleted");
	
	sc_format_apdu(card, &apdu, SC_APDU_CASE_1, 0xE4, 0x00, 0x00);
	
	r = sc_transmit_apdu(card, &apdu);
	SC_TEST_RET(card->ctx, r, "APDU transmit failed");
	return sc_check_sw(card, apdu.sw1, apdu.sw2);
}

static const struct sc_card_driver * sc_get_driver(void)
{
	const struct sc_card_driver *iso_drv = sc_get_iso7816_driver();

	miocos_ops = *iso_drv->ops;
	miocos_ops.match_card = miocos_match_card;
	miocos_ops.init = miocos_init;
        miocos_ops.finish = miocos_finish;
	if (iso_ops == NULL)
                iso_ops = iso_drv->ops;
	miocos_ops.create_file = miocos_create_file;
	miocos_ops.set_security_env = miocos_set_security_env;
	miocos_ops.select_file = miocos_select_file;
	miocos_ops.list_files = miocos_list_files;
	miocos_ops.delete_file = miocos_delete_file;
	
        return &miocos_drv;
}

#if 1
const struct sc_card_driver * sc_get_miocos_driver(void)
{
	return sc_get_driver();
}
#endif
