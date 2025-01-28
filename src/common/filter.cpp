/*
 * SPDX-FileCopyrightText: 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include "filter.hpp"

#include <stddef.h>

struct bytecode_symbol_iterator {
	/* No ownership of bytecode is taken. */
	char *bytecode;
	size_t offset, len;
};

struct bytecode_symbol_iterator *bytecode_symbol_iterator_create(struct lttng_bytecode *bytecode)
{
	struct bytecode_symbol_iterator *it = nullptr;

	if (!bytecode) {
		goto end;
	}

	it = zmalloc<bytecode_symbol_iterator>();
	if (!it) {
		goto end;
	}

	it->bytecode = bytecode->data;
	it->offset = bytecode->reloc_table_offset;
	it->len = bytecode->len;
end:
	return it;
}

int bytecode_symbol_iterator_next(struct bytecode_symbol_iterator *it)
{
	int ret;
	size_t len;

	if (!it || it->offset >= it->len) {
		ret = -1;
		goto end;
	}

	len = strlen(it->bytecode + it->offset + sizeof(uint16_t)) + 1;
	it->offset += len + sizeof(uint16_t);
	ret = it->offset >= it->len ? -1 : 0;
end:
	return ret;
}

int bytecode_symbol_iterator_get_type(struct bytecode_symbol_iterator *it)
{
	int ret;

	if (!it) {
		ret = -1;
		goto end;
	}

	ret = *((uint16_t *) (it->bytecode + it->offset));
end:
	return ret;
}

const char *bytecode_symbol_iterator_get_name(struct bytecode_symbol_iterator *it)
{
	const char *ret = nullptr;

	if (!it) {
		goto end;
	}

	ret = it->bytecode + it->offset + sizeof(uint16_t);
end:
	return ret;
}

void bytecode_symbol_iterator_destroy(struct bytecode_symbol_iterator *it)
{
	free(it);
}
