/*
 * sc.c: General functions
 *
 * Copyright (C) 2001  Juha Yrj�l� <juha.yrjola@iki.fi>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "sc-internal.h"
#include "sc-log.h"
#include "sc-asn1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef VERSION
const char *sc_version = VERSION;
#else
#warning FIXME: version info undefined
const char *sc_version = "(undef)";
#endif

int sc_hex_to_bin(const char *in, u8 *out, size_t *outlen)
{
	int err = 0;
	size_t left, c = 0;

	assert(in != NULL && out != NULL && outlen != NULL);
        left = *outlen;

	while (*in != (char) 0) {
		int byte;

		if (sscanf(in, "%02X", &byte) != 1) {
                        err = SC_ERROR_INVALID_ARGUMENTS;
			break;
		}
		in += 2;
		if (*in == ':')
			in++;
		if (left <= 0) {
                        err = SC_ERROR_BUFFER_TOO_SMALL;
			break;
		}
		*out++ = (u8) byte;
		left--;
		c++;
	}
	*outlen = c;
	return err;
}

struct sc_slot_info * _sc_get_slot_info(struct sc_reader *reader, int slot_id)
{
	assert(reader != NULL);
	if (slot_id > reader->slot_count)
		return NULL;
	return &reader->slot[slot_id];
}

int sc_detect_card_presence(struct sc_reader *reader, int slot_id)
{
	int r;
	struct sc_slot_info *slot = _sc_get_slot_info(reader, slot_id);
	
	if (slot == NULL)
		SC_FUNC_RETURN(reader->ctx, 0, SC_ERROR_SLOT_NOT_FOUND);
	SC_FUNC_CALLED(reader->ctx, 1);
	if (reader->ops->detect_card_presence == NULL)
		SC_FUNC_RETURN(reader->ctx, 0, SC_ERROR_NOT_SUPPORTED);
	
	r = reader->ops->detect_card_presence(reader, slot);
	SC_FUNC_RETURN(reader->ctx, 1, r);
}

#if 0
int sc_wait_for_card(struct sc_context *ctx, int reader, int timeout)
{
	LONG ret;
	SCARD_READERSTATE_A rgReaderStates[SC_MAX_READERS];
	int count = 0, i;

	assert(ctx != NULL);
	SC_FUNC_CALLED(ctx, 1);
	if (reader >= ctx->reader_count)
		SC_FUNC_RETURN(ctx, 1, SC_ERROR_INVALID_ARGUMENTS);

	if (reader < 0) {
		if (ctx->reader_count == 0)
			SC_FUNC_RETURN(ctx, 1, SC_ERROR_NO_READERS_FOUND);
		for (i = 0; i < ctx->reader_count; i++) {
			rgReaderStates[i].szReader = ctx->readers[i];
			rgReaderStates[i].dwCurrentState = SCARD_STATE_UNAWARE;
			rgReaderStates[i].dwEventState = SCARD_STATE_UNAWARE;
		}
		count = ctx->reader_count;
	} else {
		rgReaderStates[0].szReader = ctx->readers[reader];
		rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;
		rgReaderStates[0].dwEventState = SCARD_STATE_UNAWARE;
		count = 1;
	}
	ret = SCardGetStatusChange(ctx->pcsc_ctx, timeout, rgReaderStates, count);
	if (ret != 0) {
		error(ctx, "SCardGetStatusChange failed: %s\n", pcsc_stringify_error(ret));
		SC_FUNC_RETURN(ctx, 1, -1);
	}
	for (i = 0; i < count; i++) {
		if (rgReaderStates[i].dwEventState & SCARD_STATE_CHANGED)
			SC_FUNC_RETURN(ctx, 1, 1);
	}
	SC_FUNC_RETURN(ctx, 1, 0);
}
#endif

void sc_format_path(const char *str, struct sc_path *path)
{
	int len = 0;
	int type = SC_PATH_TYPE_PATH;
	u8 *p = path->value;

	if (*str == 'i' || *str == 'I') {
		type = SC_PATH_TYPE_FILE_ID;
		str++;
	}
	while (*str) {
		int byte;
		
		if (sscanf(str, "%02X", &byte) != 1)
			break;
		*p++ = byte;
		len++;
		str += 2;
	}
	path->len = len;
	path->type = type;
	path->index = 0;
	return;
}

int sc_append_path(struct sc_path *dest, const struct sc_path *src)
{
	assert(dest != NULL && src != NULL);
	if (dest->len + src->len > SC_MAX_PATH_SIZE)
		return SC_ERROR_INVALID_ARGUMENTS;
	memcpy(dest->value + dest->len, src->value, src->len);
	dest->len += src->len;
	return 0;
}

int sc_append_path_id(struct sc_path *dest, const u8 *id, size_t idlen)
{
	if (dest->len + idlen > SC_MAX_PATH_SIZE)
		return SC_ERROR_INVALID_ARGUMENTS;
	memcpy(dest->value + dest->len, id, idlen);
	dest->len += idlen;
	return 0;
}

const char *sc_strerror(int error)
{
	const char *errors[] = {
		"Unknown error",
		"Command too short",
		"Command too long",
		"Not supported",
		"Transmit failed",
		"File not found",
		"Invalid arguments",
		"PKCS#15 compatible SmartCard not found",
		"Required parameter not found on SmartCard",
		"Out of memory",
		"No readers found",
		"Object not valid",
		"Unknown response",
		"PIN code incorrect",
		"Security status not satisfied",
		"Error connecting to Resource Manager",
		"Invalid ASN.1 object",
		"Buffer too small",
		"Card not present",
		"Error with Resource Manager",
		"Card removed",
		"Invalid PIN length",
		"Unknown SmartCard",
		"Unknown reply from SmartCard",
		"Requested object not found",
		"Card reset",
		"Required ASN.1 object not found",
		"Premature end of ASN.1 stream",
		"Too many objects",
		"Card is invalid or cannot be handled",
		"Wrong length",
		"Record not found",
		"Internal error",
		"Invalid CLA byte in APDU",
		"Slot not found",
		"Slot already connected",
		"Authentication method blocked",
	};
	int nr_errors = sizeof(errors) / sizeof(errors[0]);

	error -= SC_ERROR_MIN;
	if (error < 0)
		error = -error;

	if (error >= nr_errors)
		return errors[0];
	return errors[error];
}

int sc_file_add_acl_entry(struct sc_file *file, unsigned int operation,
                          unsigned int method, unsigned long key_ref)
{
	struct sc_acl_entry *p, *new;

	assert(file != NULL);
	assert(operation < SC_MAX_AC_OPS);

	switch (method) {
	case SC_AC_NEVER:
		sc_file_clear_acl_entries(file, operation);
		file->acl[operation] = (struct sc_acl_entry *) 1;
		return 0;
	case SC_AC_NONE:
		sc_file_clear_acl_entries(file, operation);
		file->acl[operation] = (struct sc_acl_entry *) 2;
		return 0;
	case SC_AC_UNKNOWN:
		sc_file_clear_acl_entries(file, operation);
		file->acl[operation] = (struct sc_acl_entry *) 3;
		return 0;
	default:
		/* NONE and UNKNOWN get zapped when a new AC is added.
		 * If the ACL is NEVER, additional entries will be
		 * dropped silently. */
		if (file->acl[operation] == (struct sc_acl_entry *) 1)
			return 0;
		if (file->acl[operation] == (struct sc_acl_entry *) 2
		 || file->acl[operation] == (struct sc_acl_entry *) 3)
			file->acl[operation] = NULL;
	}
	
	new = malloc(sizeof(struct sc_acl_entry));
	if (new == NULL)
		return SC_ERROR_OUT_OF_MEMORY;
	new->method = method;
	new->key_ref = key_ref;
	new->next = NULL;

	p = file->acl[operation];
	if (p == NULL) {
		file->acl[operation] = new;
		return 0;
	}
	while (p->next != NULL)
		p = p->next;
	p->next = new;

	return 0;
}

const struct sc_acl_entry * sc_file_get_acl_entry(const struct sc_file *file,
						  unsigned int operation)
{
	struct sc_acl_entry *p;
	static const struct sc_acl_entry e_never = {
		SC_AC_NEVER, SC_AC_KEY_REF_NONE, NULL
	};
	static const struct sc_acl_entry e_none = {
		SC_AC_NONE, SC_AC_KEY_REF_NONE, NULL
	};
	static const struct sc_acl_entry e_unknown = {
		SC_AC_UNKNOWN, SC_AC_KEY_REF_NONE, NULL
	};

	assert(file != NULL);
	assert(operation < SC_MAX_AC_OPS);

	p = file->acl[operation];
	if (p == (struct sc_acl_entry *) 1)
		return &e_never;
	if (p == (struct sc_acl_entry *) 2)
		return &e_none;
	if (p == (struct sc_acl_entry *) 3)
		return &e_unknown;

	return file->acl[operation];
}

void sc_file_clear_acl_entries(struct sc_file *file, unsigned int operation)
{
	struct sc_acl_entry *e;
	
	assert(file != NULL);
	assert(operation < SC_MAX_AC_OPS);

	e = file->acl[operation];
	if (e == (struct sc_acl_entry *) 1 || 
	    e == (struct sc_acl_entry *) 2 ||
	    e == (struct sc_acl_entry *) 3) {
		file->acl[operation] = NULL;
		return;
	}

	while (e != NULL) {
		struct sc_acl_entry *tmp = e->next;
		free(e);
		e = tmp;
	}
	file->acl[operation] = NULL;
}

struct sc_file * sc_file_new()
{
	struct sc_file *file = malloc(sizeof(struct sc_file));
	
	if (file == NULL)
		return NULL;
	memset(file, 0, sizeof(struct sc_file));
	file->magic = SC_FILE_MAGIC;
	return file;
}

void sc_file_free(struct sc_file *file)
{
	int i;
	assert(sc_file_valid(file));
	file->magic = 0;
	for (i = 0; i < SC_MAX_AC_OPS; i++)
		sc_file_clear_acl_entries(file, i);
	if (file->sec_attr)
		free(file->sec_attr);
	if (file->prop_attr)
		free(file->prop_attr);
	free(file);
}

void sc_file_dup(struct sc_file **dest, const struct sc_file *src)
{
	struct sc_file *newf;
	const struct sc_acl_entry *e;
	int op;
	
	assert(sc_file_valid(src));
	*dest = NULL;
	newf = sc_file_new();
	if (newf == NULL)
		return;
	*dest = newf;
	
	*newf = *src;
	for (op = 0; op < SC_MAX_AC_OPS; op++) {
		newf->acl[op] = NULL;
		e = sc_file_get_acl_entry(src, op);
		if (e != NULL)
			sc_file_add_acl_entry(newf, op, e->method, e->key_ref);
	}
}

int sc_file_set_sec_attr(struct sc_file *file, const u8 *sec_attr,
			 size_t sec_attr_len)
{
	assert(sc_file_valid(file));

	if (sec_attr == NULL) {
		if (file->sec_attr != NULL)
			free(file->sec_attr);
		file->sec_attr = NULL;
		file->sec_attr_len = 0;
		return 0;
	 }
	file->sec_attr = realloc(file->sec_attr, sec_attr_len);
	if (file->sec_attr == NULL) {
		file->sec_attr_len = 0;
		return SC_ERROR_OUT_OF_MEMORY;
	}
	memcpy(file->sec_attr, sec_attr, sec_attr_len);
	file->sec_attr_len = sec_attr_len;

	return 0;
}                         

int sc_file_set_prop_attr(struct sc_file *file, const u8 *prop_attr,
			 size_t prop_attr_len)
{
	assert(sc_file_valid(file));

	if (prop_attr == NULL) {
		if (file->prop_attr != NULL)
			free(file->prop_attr);
		file->prop_attr = NULL;
		file->prop_attr_len = 0;
		return 0;
	 }
	file->prop_attr = realloc(file->prop_attr, prop_attr_len);
	if (file->prop_attr == NULL) {
		file->prop_attr_len = 0;
		return SC_ERROR_OUT_OF_MEMORY;
	}
	memcpy(file->prop_attr, prop_attr, prop_attr_len);
	file->prop_attr_len = prop_attr_len;

	return 0;
}                         

inline int sc_file_valid(const struct sc_file *file) {
#ifndef NDEBUG
	assert(file != NULL);
#endif
	return file->magic == SC_FILE_MAGIC;
}

int _sc_parse_atr(struct sc_context *ctx, struct sc_slot_info *slot)
{
	u8 *p = slot->atr;
	int atr_len = (int) slot->atr_len;
	int n_hist, x;
	int tx[4];
	int i, FI, DI;
	const int Fi_table[] = {
		372, 372, 558, 744, 1116, 1488, 1860, -1,
		-1, 512, 768, 1024, 1536, 2048, -1, -1 };
	const int f_table[] = {
		40, 50, 60, 80, 120, 160, 200, -1,
		-1, 50, 75, 100, 150, 200, -1, -1 };
	const int Di_table[] = {
		-1, 1, 2, 4, 8, 16, 32, -1,
		12, 20, -1, -1, -1, -1, -1, -1 };
	
	if (p[0] != 0x3B && p[0] != 0x3F) {
		error(ctx, "invalid sync byte in ATR: 0x%02X\n", p[0]);
		return SC_ERROR_INTERNAL;
	}
	n_hist = p[1] & 0x0F;
	x = p[1] >> 4;
	p += 2;
	atr_len -= 2;
	for (i = 0; i < 4 && atr_len > 0; i++) {
                if (x & (1 << i)) {
                        tx[i] = *p;
                        p++;
                        atr_len--;
                } else
                        tx[i] = -1;
        }
	if (tx[0] >= 0) {
		slot->atr_info.FI = FI = tx[0] >> 4;
		slot->atr_info.DI = DI = tx[0] & 0x0F;
		slot->atr_info.Fi = Fi_table[FI];
		slot->atr_info.f = f_table[FI];
		slot->atr_info.Di = Di_table[DI];
	} else {
		slot->atr_info.Fi = -1;
		slot->atr_info.f = -1;
		slot->atr_info.Di = -1;
	}
	if (tx[2] >= 0)
		slot->atr_info.N = tx[3];
	else
		slot->atr_info.N = -1;
	while (tx[3] > 0 && tx[3] & 0xF0 && atr_len > 0) {
		x = tx[3] >> 4;
		for (i = 0; i < 4 && atr_len > 0; i++) {
	                if (x & (1 << i)) {
	                        tx[i] = *p;
	                        p++;
	                        atr_len--;
	                } else
	                        tx[i] = -1;
        	}
	}
	if (atr_len <= 0)
		return 0;
	if (n_hist > atr_len)
		n_hist = atr_len;
	slot->atr_info.hist_bytes_len = n_hist;
	slot->atr_info.hist_bytes = p;
	
	return 0;
}
