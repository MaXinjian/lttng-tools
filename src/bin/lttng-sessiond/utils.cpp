/*
 * SPDX-FileCopyrightText: 2011 EfficiOS Inc.
 * SPDX-FileCopyrightText: 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE
#include "lttng-sessiond.hpp"
#include "snapshot.hpp"
#include "utils.hpp"

#include <common/compat/getenv.hpp>
#include <common/error.hpp>

#include <stdlib.h>
#include <unistd.h>

/*
 * Write to writable pipe used to notify a thread.
 */
int notify_thread_pipe(int wpipe)
{
	ssize_t ret;

	/* Ignore if the pipe is invalid. */
	if (wpipe < 0) {
		return 0;
	}

	ret = lttng_write(wpipe, "!", 1);
	if (ret < 1) {
		ret = -1;
		PERROR("Failed to write to thread pipe");
	}

	return (int) ret;
}

int loglevels_match(int a_loglevel_type,
		    int a_loglevel_value,
		    int b_loglevel_type,
		    int b_loglevel_value,
		    int loglevel_all_type)
{
	int match = 1;

	if (a_loglevel_type == b_loglevel_type) {
		/* Same loglevel type. */
		if (b_loglevel_type != loglevel_all_type) {
			/*
			 * Loglevel value must also match since the loglevel
			 * type is not all.
			 */
			if (a_loglevel_value != b_loglevel_value) {
				match = 0;
			}
		}
	} else {
		/* Loglevel type is different: no match. */
		match = 0;
	}

	return match;
}

const char *session_get_base_path(const ltt_session::locked_ref& session)
{
	return consumer_output_get_base_path(session->consumer);
}

const char *consumer_output_get_base_path(const struct consumer_output *output)
{
	return output->type == CONSUMER_DST_LOCAL ? output->dst.session_root_path :
						    output->dst.net.base_dir;
}
