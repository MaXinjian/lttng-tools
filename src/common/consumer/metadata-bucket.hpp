/*
 * SPDX-FileCopyrightText: 2020 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef METADATA_BUCKET_H
#define METADATA_BUCKET_H

#include <common/consumer/consumer.hpp>

struct metadata_bucket;

using metadata_bucket_flush_cb = ssize_t (*)(const struct stream_subbuffer *, void *);

enum metadata_bucket_status {
	METADATA_BUCKET_STATUS_OK,
	METADATA_BUCKET_STATUS_ERROR,
};

struct metadata_bucket *metadata_bucket_create(metadata_bucket_flush_cb flush, void *data);

void metadata_bucket_destroy(struct metadata_bucket *bucket);

enum metadata_bucket_status metadata_bucket_fill(struct metadata_bucket *bucket,
						 const struct stream_subbuffer *buffer);

void metadata_bucket_reset(struct metadata_bucket *bucket);

#endif /* METADATA_BUCKET_H */
