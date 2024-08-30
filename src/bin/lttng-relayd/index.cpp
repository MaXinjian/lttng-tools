/*
 * Copyright (C) 2013 Julien Desfossez <jdesfossez@efficios.com>
 * Copyright (C) 2013 David Goulet <dgoulet@efficios.com>
 * Copyright (C) 2015 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE

#include "connection.hpp"
#include "index.hpp"
#include "lttng-relayd.hpp"
#include "stream.hpp"

#include <common/common.hpp>
#include <common/compat/endian.hpp>
#include <common/urcu.hpp>
#include <common/utils.hpp>

/*
 * Allocate a new relay index object. Pass the stream in which it is
 * contained as parameter. The sequence number will be used as the hash
 * table key.
 *
 * Called with stream mutex held.
 * Return allocated object or else NULL on error.
 */
static struct relay_index *relay_index_create(struct relay_stream *stream, uint64_t net_seq_num)
{
	struct relay_index *index;

	DBG2("Creating relay index for stream id %" PRIu64 " and seqnum %" PRIu64,
	     stream->stream_handle,
	     net_seq_num);

	index = zmalloc<relay_index>();
	if (!index) {
		PERROR("Relay index zmalloc");
		goto end;
	}
	if (!stream_get(stream)) {
		ERR("Cannot get stream");
		free(index);
		index = nullptr;
		goto end;
	}
	index->stream = stream;

	lttng_ht_node_init_u64(&index->index_n, net_seq_num);
	pthread_mutex_init(&index->lock, nullptr);
	urcu_ref_init(&index->ref);

end:
	return index;
}

/*
 * Add unique relay index to the given hash table. In case of a collision, the
 * already existing object is put in the given _index variable.
 *
 * RCU read side lock MUST be acquired.
 */
static struct relay_index *relay_index_add_unique(struct relay_stream *stream,
						  struct relay_index *index)
{
	struct cds_lfht_node *node_ptr;
	struct relay_index *_index;

	ASSERT_RCU_READ_LOCKED();

	DBG2("Adding relay index with stream id %" PRIu64 " and seqnum %" PRIu64,
	     stream->stream_handle,
	     index->index_n.key);

	node_ptr = cds_lfht_add_unique(stream->indexes_ht->ht,
				       stream->indexes_ht->hash_fct(&index->index_n.key,
								    lttng_ht_seed),
				       stream->indexes_ht->match_fct,
				       &index->index_n.key,
				       &index->index_n.node);
	if (node_ptr != &index->index_n.node) {
		_index = lttng_ht_node_container_of(node_ptr, &relay_index::index_n);
	} else {
		_index = nullptr;
	}
	return _index;
}

/*
 * Should be called with RCU read-side lock held.
 */
static bool relay_index_get(struct relay_index *index)
{
	ASSERT_RCU_READ_LOCKED();

	DBG2("index get for stream id %" PRIu64 " and seqnum %" PRIu64 " refcount %d",
	     index->stream->stream_handle,
	     index->index_n.key,
	     (int) index->ref.refcount);

	return urcu_ref_get_unless_zero(&index->ref);
}

/*
 * Get a relayd index in within the given stream, or create it if not
 * present.
 *
 * Called with stream mutex held.
 * Return index object or else NULL on error.
 */
struct relay_index *relay_index_get_by_id_or_create(struct relay_stream *stream,
						    uint64_t net_seq_num)
{
	struct lttng_ht_node_u64 *node;
	struct lttng_ht_iter iter;
	struct relay_index *index = nullptr;

	DBG3("Finding index for stream id %" PRIu64 " and seq_num %" PRIu64,
	     stream->stream_handle,
	     net_seq_num);

	const lttng::urcu::read_lock_guard read_lock;
	lttng_ht_lookup(stream->indexes_ht, &net_seq_num, &iter);
	node = lttng_ht_iter_get_node<lttng_ht_node_u64>(&iter);
	if (node) {
		index = lttng::utils::container_of(node, &relay_index::index_n);
	} else {
		struct relay_index *oldindex;

		index = relay_index_create(stream, net_seq_num);
		if (!index) {
			ERR("Cannot create index for stream id %" PRIu64 " and seq_num %" PRIu64,
			    stream->stream_handle,
			    net_seq_num);
			goto end;
		}
		oldindex = relay_index_add_unique(stream, index);
		if (oldindex) {
			/* Added concurrently, keep old. */
			relay_index_put(index);
			index = oldindex;
			if (!relay_index_get(index)) {
				index = nullptr;
			}
		} else {
			stream->indexes_in_flight++;
			index->in_hash_table = true;
		}
	}
end:
	DBG2("Index %sfound or created in HT for stream ID %" PRIu64 " and seqnum %" PRIu64,
	     (index == NULL) ? "NOT " : "",
	     stream->stream_handle,
	     net_seq_num);
	return index;
}

int relay_index_set_file(struct relay_index *index,
			 struct lttng_index_file *index_file,
			 uint64_t data_offset)
{
	int ret = 0;

	pthread_mutex_lock(&index->lock);
	if (index->index_file) {
		ret = -1;
		goto end;
	}
	lttng_index_file_get(index_file);
	index->index_file = index_file;
	index->index_data.offset = data_offset;
end:
	pthread_mutex_unlock(&index->lock);
	return ret;
}

int relay_index_set_data(struct relay_index *index, const struct ctf_packet_index *data)
{
	int ret = 0;

	pthread_mutex_lock(&index->lock);
	if (index->has_index_data) {
		ret = -1;
		goto end;
	}
	/* Set everything except data_offset. */
	index->index_data.packet_size = data->packet_size;
	index->index_data.content_size = data->content_size;
	index->index_data.timestamp_begin = data->timestamp_begin;
	index->index_data.timestamp_end = data->timestamp_end;
	index->index_data.events_discarded = data->events_discarded;
	index->index_data.stream_id = data->stream_id;
	index->has_index_data = true;
end:
	pthread_mutex_unlock(&index->lock);
	return ret;
}

static void index_destroy(struct relay_index *index)
{
	free(index);
}

static void index_destroy_rcu(struct rcu_head *rcu_head)
{
	struct relay_index *index = lttng::utils::container_of(rcu_head, &relay_index::rcu_node);

	index_destroy(index);
}

/* Stream lock must be held by the caller. */
static void index_release(struct urcu_ref *ref)
{
	struct relay_index *index = lttng::utils::container_of(ref, &relay_index::ref);
	struct relay_stream *stream = index->stream;
	int ret;
	struct lttng_ht_iter iter;

	if (index->index_file) {
		lttng_index_file_put(index->index_file);
		index->index_file = nullptr;
	}
	if (index->in_hash_table) {
		/* Delete index from hash table. */
		iter.iter.node = &index->index_n.node;
		ret = lttng_ht_del(stream->indexes_ht, &iter);
		LTTNG_ASSERT(!ret);
		stream->indexes_in_flight--;
	}

	stream_put(index->stream);
	index->stream = nullptr;

	call_rcu(&index->rcu_node, index_destroy_rcu);
}

/*
 * Called with stream mutex held.
 *
 * Stream lock must be held by the caller.
 */
void relay_index_put(struct relay_index *index)
{
	DBG2("index put for stream id %" PRIu64 " and seqnum %" PRIu64 " refcount %d",
	     index->stream->stream_handle,
	     index->index_n.key,
	     (int) index->ref.refcount);
	/*
	 * Ensure existence of index->lock for index unlock.
	 */
	const lttng::urcu::read_lock_guard read_lock;
	/*
	 * Index lock ensures that concurrent test and update of stream
	 * ref is atomic.
	 */
	LTTNG_ASSERT(index->ref.refcount != 0);
	urcu_ref_put(&index->ref, index_release);
}

/*
 * Try to flush index to disk. Releases self-reference to index once
 * flush succeeds.
 *
 * Stream lock must be held by the caller.
 * Return 0 on successful flush, a negative value on error, or positive
 * value if no flush was performed.
 */
int relay_index_try_flush(struct relay_index *index)
{
	int ret = 1;
	bool flushed = false;

	pthread_mutex_lock(&index->lock);
	if (index->flushed) {
		goto skip;
	}
	/* Check if we are ready to flush. */
	if (!index->has_index_data || !index->index_file) {
		goto skip;
	}

	DBG2("Writing index for stream ID %" PRIu64 " and seq num %" PRIu64,
	     index->stream->stream_handle,
	     index->index_n.key);
	flushed = true;
	index->flushed = true;
	ret = lttng_index_file_write(index->index_file, &index->index_data);
skip:
	pthread_mutex_unlock(&index->lock);

	if (flushed) {
		/* Put self-ref from index now that it has been flushed. */
		relay_index_put(index);
	}
	return ret;
}

/*
 * Close every relay index within a given stream, without flushing
 * them.
 */
void relay_index_close_all(struct relay_stream *stream)
{
	for (auto *index :
	     lttng::urcu::lfht_iteration_adapter<relay_index,
						 decltype(relay_index::index_n),
						 &relay_index::index_n>(*stream->indexes_ht->ht)) {
		/* Put self-ref from index. */
		relay_index_put(index);
	}
}

void relay_index_close_partial_fd(struct relay_stream *stream)
{
	for (auto *index :
	     lttng::urcu::lfht_iteration_adapter<relay_index,
						 decltype(relay_index::index_n),
						 &relay_index::index_n>(*stream->indexes_ht->ht)) {
		if (!index->index_file) {
			continue;
		}
		/*
		 * Partial index has its index_file: we have only
		 * received its info from the data socket.
		 * Put self-ref from index.
		 */
		relay_index_put(index);
	}
}

uint64_t relay_index_find_last(struct relay_stream *stream)
{
	uint64_t net_seq_num = -1ULL;

	for (auto *index :
	     lttng::urcu::lfht_iteration_adapter<relay_index,
						 decltype(relay_index::index_n),
						 &relay_index::index_n>(*stream->indexes_ht->ht)) {
		if (net_seq_num == -1ULL || index->index_n.key > net_seq_num) {
			net_seq_num = index->index_n.key;
		}
	}

	return net_seq_num;
}

/*
 * Update the index file of an already existing relay_index.
 * Offsets by 'removed_data_count' the offset field of an index.
 */
static int relay_index_switch_file(struct relay_index *index,
				   struct lttng_index_file *new_index_file,
				   uint64_t removed_data_count)
{
	int ret = 0;
	uint64_t offset;

	pthread_mutex_lock(&index->lock);
	if (!index->index_file) {
		ERR("No index_file");
		ret = 0;
		goto end;
	}

	lttng_index_file_put(index->index_file);
	lttng_index_file_get(new_index_file);
	index->index_file = new_index_file;
	offset = be64toh(index->index_data.offset);
	index->index_data.offset = htobe64(offset - removed_data_count);

end:
	pthread_mutex_unlock(&index->lock);
	return ret;
}

/*
 * Switch the index file of all pending indexes for a stream and update the
 * data offset by substracting the last safe position.
 * Stream lock must be held.
 */
int relay_index_switch_all_files(struct relay_stream *stream)
{
	for (auto *index :
	     lttng::urcu::lfht_iteration_adapter<relay_index,
						 decltype(relay_index::index_n),
						 &relay_index::index_n>(*stream->indexes_ht->ht)) {
		const auto ret = relay_index_switch_file(
			index, stream->index_file, stream->pos_after_last_complete_data_index);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

/*
 * Set index data from the control port to a given index object.
 */
int relay_index_set_control_data(struct relay_index *index,
				 const struct lttcomm_relayd_index *data,
				 unsigned int minor_version)
{
	/* The index on disk is encoded in big endian. */
	ctf_packet_index index_data{};

	index_data.packet_size = htobe64(data->packet_size);
	index_data.content_size = htobe64(data->content_size);
	index_data.timestamp_begin = htobe64(data->timestamp_begin);
	index_data.timestamp_end = htobe64(data->timestamp_end);
	index_data.events_discarded = htobe64(data->events_discarded);
	index_data.stream_id = htobe64(data->stream_id);

	if (minor_version >= 8) {
		index->index_data.stream_instance_id = htobe64(data->stream_instance_id);
		index->index_data.packet_seq_num = htobe64(data->packet_seq_num);
	} else {
		const uint64_t unset_value = -1ULL;

		index->index_data.stream_instance_id = htobe64(unset_value);
		index->index_data.packet_seq_num = htobe64(unset_value);
	}

	return relay_index_set_data(index, &index_data);
}
