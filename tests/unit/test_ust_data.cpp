/*
 * SPDX-FileCopyrightText: 2011 EfficiOS Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include <common/compat/errno.hpp>
#include <common/defaults.hpp>

#include <lttng/lttng.h>
#include <lttng/ust-sigbus.h>

#include <bin/lttng-sessiond/lttng-ust-abi.hpp>
#include <bin/lttng-sessiond/notification-thread.hpp>
#include <bin/lttng-sessiond/trace-ust.hpp>
#include <bin/lttng-sessiond/ust-app.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tap/tap.h>
#include <time.h>
#include <unistd.h>
#include <urcu.h>

/* This path will NEVER be created in this test */
#define PATH1 "/tmp/.test-junk-lttng"

#define RANDOM_STRING_LEN 11

/* Number of TAP tests in this file */
#define NUM_TESTS 16

LTTNG_EXPORT DEFINE_LTTNG_UST_SIGBUS_STATE();

static const char alphanum[] = "0123456789"
			       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			       "abcdefghijklmnopqrstuvwxyz";
static char random_string[RANDOM_STRING_LEN];

/*
 * Return random string of 10 characters.
 * Not thread-safe.
 */
static char *get_random_string()
{
	int i;

	for (i = 0; i < RANDOM_STRING_LEN - 1; i++) {
		random_string[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	random_string[RANDOM_STRING_LEN - 1] = '\0';

	return random_string;
}

static void test_create_one_ust_session()
{
	struct ltt_ust_session *usess = trace_ust_create_session(42);

	ok(usess != nullptr, "Create UST session");

	if (!usess) {
		skip(1, "UST session is null");
		return;
	}

	ok(usess->id == 42 && !usess->active && usess->domain_global.channels != nullptr &&
		   usess->uid == 0 && usess->gid == 0,
	   "Validate UST session");

	trace_ust_destroy_session(usess);
	trace_ust_free_session(usess);
}

static void test_create_ust_channel()
{
	struct ltt_ust_channel *uchan;
	struct lttng_channel attr;
	struct lttng_channel_extended extended;

	memset(&attr, 0, sizeof(attr));
	memset(&extended, 0, sizeof(extended));
	attr.attr.extended.ptr = &extended;

	ok(lttng_strncpy(attr.name, "channel0", sizeof(attr.name)) == 0,
	   "Validate channel name length");
	uchan = trace_ust_create_channel(&attr, LTTNG_DOMAIN_UST);
	ok(uchan != nullptr, "Create UST channel");

	if (!uchan) {
		skip(1, "UST channel is null");
		return;
	}

	ok(!uchan->enabled && strncmp(uchan->name, "channel0", 8) == 0 &&
		   uchan->name[LTTNG_UST_ABI_SYM_NAME_LEN - 1] == '\0' && uchan->ctx != nullptr &&
		   uchan->events != nullptr && uchan->attr.overwrite == attr.attr.overwrite,
	   "Validate UST channel");

	trace_ust_destroy_channel(uchan);
}

static void test_create_ust_event()
{
	struct ltt_ust_event *event;
	struct lttng_event ev;
	enum lttng_error_code ret;

	memset(&ev, 0, sizeof(ev));
	ok(lttng_strncpy(ev.name, get_random_string(), LTTNG_SYMBOL_NAME_LEN) == 0,
	   "Validate string length");
	ev.type = LTTNG_EVENT_TRACEPOINT;
	ev.loglevel_type = LTTNG_EVENT_LOGLEVEL_ALL;

	ret = trace_ust_create_event(&ev, nullptr, nullptr, nullptr, false, &event);

	ok(ret == LTTNG_OK, "Create UST event");

	if (!event) {
		skip(1, "UST event is null");
		return;
	}

	ok(!event->enabled && event->attr.instrumentation == LTTNG_UST_ABI_TRACEPOINT &&
		   strcmp(event->attr.name, ev.name) == 0 &&
		   event->attr.name[LTTNG_UST_ABI_SYM_NAME_LEN - 1] == '\0',
	   "Validate UST event");

	trace_ust_destroy_event(event);
}

static void test_create_ust_event_exclusion()
{
	int copy_ret;
	enum lttng_error_code ret;
	struct ltt_ust_event *event;
	struct lttng_event ev;
	char *name;
	char *random_name;
	struct lttng_event_exclusion *exclusion = nullptr;
	struct lttng_event_exclusion *exclusion_copy = nullptr;
	const int exclusion_count = 2;

	memset(&ev, 0, sizeof(ev));

	/* make a wildcarded event name */
	name = get_random_string();
	name[strlen(name) - 1] = '*';
	ok(lttng_strncpy(ev.name, name, LTTNG_SYMBOL_NAME_LEN) == 0, "Validate string length");

	ev.type = LTTNG_EVENT_TRACEPOINT;
	ev.loglevel_type = LTTNG_EVENT_LOGLEVEL_ALL;

	/* set up an exclusion set */
	exclusion = zmalloc<lttng_event_exclusion>(sizeof(*exclusion) +
						   LTTNG_SYMBOL_NAME_LEN * exclusion_count);
	ok(exclusion != nullptr, "Create UST exclusion");
	if (!exclusion) {
		skip(4, "zmalloc failed");
		goto end;
	}

	exclusion->count = exclusion_count;
	random_name = get_random_string();
	strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 0),
		random_name,
		LTTNG_SYMBOL_NAME_LEN - 1);
	strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 1),
		random_name,
		LTTNG_SYMBOL_NAME_LEN - 1);

	ret = trace_ust_create_event(&ev, nullptr, nullptr, exclusion, false, &event);
	exclusion = nullptr;

	ok(ret != LTTNG_OK, "Create UST event with identical exclusion names fails");

	exclusion = zmalloc<lttng_event_exclusion>(sizeof(*exclusion) +
						   LTTNG_SYMBOL_NAME_LEN * exclusion_count);
	ok(exclusion != nullptr, "Create UST exclusion");
	if (!exclusion) {
		skip(2, "zmalloc failed");
		goto end;
	}

	exclusion_copy = zmalloc<lttng_event_exclusion>(sizeof(*exclusion) +
							LTTNG_SYMBOL_NAME_LEN * exclusion_count);
	if (!exclusion_copy) {
		skip(2, "zmalloc failed");
		goto end;
	}

	/*
	 * We are giving ownership of the exclusion struct to the
	 * trace_ust_create_event() function. Make a copy of the exclusion struct
	 * so we can compare it later.
	 */

	exclusion->count = exclusion_count;
	copy_ret = lttng_strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 0),
				 get_random_string(),
				 sizeof(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 0)));
	LTTNG_ASSERT(copy_ret == 0);
	copy_ret = lttng_strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 1),
				 get_random_string(),
				 sizeof(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 1)));
	LTTNG_ASSERT(copy_ret == 0);

	exclusion_copy->count = exclusion_count;
	copy_ret = lttng_strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion_copy, 0),
				 LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 0),
				 sizeof(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion_copy, 0)));
	LTTNG_ASSERT(copy_ret == 0);
	copy_ret = lttng_strncpy(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion_copy, 1),
				 LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion, 1),
				 sizeof(LTTNG_EVENT_EXCLUSION_NAME_AT(exclusion_copy, 1)));
	LTTNG_ASSERT(copy_ret == 0);

	ret = trace_ust_create_event(&ev, nullptr, nullptr, exclusion, false, &event);
	exclusion = nullptr;
	ok(ret == LTTNG_OK, "Create UST event with different exclusion names");

	if (!event) {
		skip(1, "UST event with exclusion is null");
		goto end;
	}

	ok(!event->enabled && event->attr.instrumentation == LTTNG_UST_ABI_TRACEPOINT &&
		   strcmp(event->attr.name, ev.name) == 0 && event->exclusion != nullptr &&
		   event->exclusion->count == exclusion_count &&
		   !memcmp(event->exclusion->names,
			   exclusion_copy->names,
			   LTTNG_SYMBOL_NAME_LEN * exclusion_count) &&
		   event->attr.name[LTTNG_UST_ABI_SYM_NAME_LEN - 1] == '\0',
	   "Validate UST event and exclusion");

	trace_ust_destroy_event(event);
end:
	free(exclusion);
	free(exclusion_copy);
	return;
}

static void test_create_ust_context()
{
	struct lttng_event_context ectx;
	struct ltt_ust_context *uctx;

	ectx.ctx = LTTNG_EVENT_CONTEXT_VTID;

	uctx = trace_ust_create_context(&ectx);
	ok(uctx != nullptr, "Create UST context");

	if (uctx) {
		ok((int) uctx->ctx.ctx == LTTNG_UST_ABI_CONTEXT_VTID, "Validate UST context");
	} else {
		skip(1, "Skipping UST context validation as creation failed");
	}
	free(uctx);
}

int main()
{
	plan_tests(NUM_TESTS);

	diag("UST data structures unit test");

	rcu_register_thread();

	test_create_one_ust_session();
	test_create_ust_channel();
	test_create_ust_event();
	test_create_ust_context();
	test_create_ust_event_exclusion();

	rcu_unregister_thread();

	return exit_status();
}
