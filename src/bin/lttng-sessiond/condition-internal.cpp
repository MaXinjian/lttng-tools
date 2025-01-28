/*
 * SPDX-FileCopyrightText: 2020 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include "condition-internal.hpp"

#include <common/hashtable/hashtable.hpp>
#include <common/hashtable/utils.hpp>

#include <lttng/condition/buffer-usage-internal.hpp>
#include <lttng/condition/condition-internal.hpp>
#include <lttng/condition/condition.h>
#include <lttng/condition/event-rule-matches-internal.hpp>
#include <lttng/condition/event-rule-matches.h>
#include <lttng/condition/session-consumed-size-internal.hpp>
#include <lttng/condition/session-rotation-internal.hpp>
#include <lttng/event-rule/event-rule-internal.hpp>

static unsigned long lttng_condition_buffer_usage_hash(const struct lttng_condition *_condition)
{
	unsigned long hash;
	unsigned long condition_type;
	struct lttng_condition_buffer_usage *condition;

	condition = lttng::utils::container_of(_condition, &lttng_condition_buffer_usage::parent);

	condition_type = (unsigned long) condition->parent.type;
	hash = hash_key_ulong((void *) condition_type, lttng_ht_seed);
	if (condition->session_name) {
		hash ^= hash_key_str(condition->session_name, lttng_ht_seed);
	}
	if (condition->channel_name) {
		hash ^= hash_key_str(condition->channel_name, lttng_ht_seed);
	}
	if (condition->domain.set) {
		hash ^= hash_key_ulong((void *) condition->domain.type, lttng_ht_seed);
	}
	if (condition->threshold_ratio.set) {
		hash ^= hash_key_u64(&condition->threshold_ratio.value, lttng_ht_seed);
	} else if (condition->threshold_bytes.set) {
		uint64_t val;

		val = condition->threshold_bytes.value;
		hash ^= hash_key_u64(&val, lttng_ht_seed);
	}
	return hash;
}

static unsigned long
lttng_condition_session_consumed_size_hash(const struct lttng_condition *_condition)
{
	unsigned long hash;
	const unsigned long condition_type =
		(unsigned long) LTTNG_CONDITION_TYPE_SESSION_CONSUMED_SIZE;
	struct lttng_condition_session_consumed_size *condition;
	uint64_t val;

	condition = lttng::utils::container_of(_condition,
					       &lttng_condition_session_consumed_size::parent);

	hash = hash_key_ulong((void *) condition_type, lttng_ht_seed);
	if (condition->session_name) {
		hash ^= hash_key_str(condition->session_name, lttng_ht_seed);
	}
	val = condition->consumed_threshold_bytes.value;
	hash ^= hash_key_u64(&val, lttng_ht_seed);
	return hash;
}

static unsigned long lttng_condition_session_rotation_hash(const struct lttng_condition *_condition)
{
	unsigned long hash, condition_type;
	struct lttng_condition_session_rotation *condition;

	condition =
		lttng::utils::container_of(_condition, &lttng_condition_session_rotation::parent);
	condition_type = (unsigned long) condition->parent.type;
	hash = hash_key_ulong((void *) condition_type, lttng_ht_seed);
	LTTNG_ASSERT(condition->session_name);
	hash ^= hash_key_str(condition->session_name, lttng_ht_seed);
	return hash;
}

static unsigned long
lttng_condition_event_rule_matches_hash(const struct lttng_condition *condition)
{
	unsigned long hash, condition_type;
	enum lttng_condition_status condition_status;
	const struct lttng_event_rule *event_rule;

	condition_type = (unsigned long) condition->type;
	condition_status = lttng_condition_event_rule_matches_get_rule(condition, &event_rule);
	LTTNG_ASSERT(condition_status == LTTNG_CONDITION_STATUS_OK);

	hash = hash_key_ulong((void *) condition_type, lttng_ht_seed);
	return hash ^ lttng_event_rule_hash(event_rule);
}

/*
 * The lttng_condition hashing code is kept in this file (rather than
 * condition.c) since it makes use of GPLv2 code (hashtable utils), which we
 * don't want to link in liblttng-ctl.
 */
unsigned long lttng_condition_hash(const struct lttng_condition *condition)
{
	switch (condition->type) {
	case LTTNG_CONDITION_TYPE_BUFFER_USAGE_LOW:
	case LTTNG_CONDITION_TYPE_BUFFER_USAGE_HIGH:
		return lttng_condition_buffer_usage_hash(condition);
	case LTTNG_CONDITION_TYPE_SESSION_CONSUMED_SIZE:
		return lttng_condition_session_consumed_size_hash(condition);
	case LTTNG_CONDITION_TYPE_SESSION_ROTATION_ONGOING:
	case LTTNG_CONDITION_TYPE_SESSION_ROTATION_COMPLETED:
		return lttng_condition_session_rotation_hash(condition);
	case LTTNG_CONDITION_TYPE_EVENT_RULE_MATCHES:
		return lttng_condition_event_rule_matches_hash(condition);
	default:
		abort();
	}
}

struct lttng_condition *lttng_condition_copy(const struct lttng_condition *condition)
{
	int ret;
	struct lttng_payload copy_buffer;
	struct lttng_condition *copy = nullptr;

	lttng_payload_init(&copy_buffer);

	ret = lttng_condition_serialize(condition, &copy_buffer);
	if (ret < 0) {
		goto end;
	}

	{
		struct lttng_payload_view view =
			lttng_payload_view_from_payload(&copy_buffer, 0, -1);

		ret = lttng_condition_create_from_payload(&view, &copy);
		if (ret < 0) {
			copy = nullptr;
			goto end;
		}
	}

end:
	lttng_payload_reset(&copy_buffer);
	return copy;
}
