/*
 * SPDX-FileCopyrightText: 2011 EfficiOS Inc.
 * SPDX-FileCopyrightText: 2014 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * SPDX-FileCopyrightText: 2020 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include "error.hpp"
#include "macros.hpp"
#include "spawn-viewer.hpp"

#include <common/compat/errno.hpp>

#include <lttng/constant.h>

#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Type is also use as the index in the viewers array. So please, make sure
 * your enum value is in the right order in the array below.
 */
enum viewer_type {
	VIEWER_BABELTRACE = 0,
	VIEWER_BABELTRACE2 = 1,
	VIEWER_USER_DEFINED = 2,
};

namespace {
const char *babeltrace_bin = CONFIG_BABELTRACE_BIN;
const char *babeltrace2_bin = CONFIG_BABELTRACE2_BIN;

/*
 * This is needed for each viewer since we are using execvp().
 */
const char *babeltrace_opts[] = { "babeltrace" };
const char *babeltrace2_opts[] = { "babeltrace2" };

const struct viewer {
	const char *exec_name;
	enum viewer_type type;
} viewers[] = {
	{ "babeltrace", VIEWER_BABELTRACE },
	{ "babeltrace2", VIEWER_BABELTRACE2 },
	{ nullptr, VIEWER_USER_DEFINED },
};
} /* namespace */

static const struct viewer *parse_viewer_option(const char *opt_viewer)
{
	if (opt_viewer == nullptr) {
		/* Default is babeltrace2 */
		return &(viewers[VIEWER_BABELTRACE2]);
	}

	return &(viewers[VIEWER_USER_DEFINED]);
}

/*
 * Alloc an array of string pointer from a simple string having all options
 * seperated by spaces. Also adds the trace path to the arguments.
 *
 * The returning pointer is ready to be passed to execvp().
 */
static char **alloc_argv_from_user_opts(char *opts, const char *trace_path)
{
	int i = 0, ignore_space = 0;
	unsigned int num_opts = 1;
	char **argv, *token = opts, *saveptr = nullptr;

	/* Count number of arguments. */
	do {
		if (*token == ' ') {
			/* Use to ignore consecutive spaces */
			if (!ignore_space) {
				num_opts++;
			}
			ignore_space = 1;
		} else {
			ignore_space = 0;
		}
		token++;
	} while (*token != '\0');

	/* Add two here for the NULL terminating element and trace path */
	argv = calloc<char *>(num_opts + 2);
	if (argv == nullptr) {
		goto error;
	}

	token = strtok_r(opts, " ", &saveptr);
	while (token != nullptr) {
		argv[i] = strdup(token);
		if (argv[i] == nullptr) {
			goto error;
		}
		token = strtok_r(nullptr, " ", &saveptr);
		i++;
	}

	argv[num_opts] = (char *) trace_path;
	argv[num_opts + 1] = nullptr;

	return argv;

error:
	if (argv) {
		for (i = 0; i < num_opts + 2; i++) {
			free(argv[i]);
		}
		free(argv);
	}

	return nullptr;
}

/*
 * Alloc an array of string pointer from an array of strings. It also adds
 * the trace path to the argv.
 *
 * The returning pointer is ready to be passed to execvp().
 */
static char **alloc_argv_from_local_opts(const char **opts,
					 size_t opts_len,
					 const char *trace_path,
					 bool opt_live_mode)
{
	char **argv;
	size_t mem_len;

	/* Add one for the NULL terminating element. */
	mem_len = opts_len + 1;
	if (opt_live_mode) {
		/* Add 3 option for the live mode being "-i lttng-live URL". */
		mem_len += 3;
	} else {
		/* Add option for the trace path. */
		mem_len += 1;
	}

	argv = calloc<char *>(mem_len);
	if (argv == nullptr) {
		goto error;
	}

	memcpy(argv, opts, sizeof(char *) * opts_len);

	if (opt_live_mode) {
		argv[opts_len] = (char *) "-i";
		argv[opts_len + 1] = (char *) "lttng-live";
		argv[opts_len + 2] = (char *) trace_path;
		argv[opts_len + 3] = nullptr;
	} else {
		argv[opts_len] = (char *) trace_path;
		argv[opts_len + 1] = nullptr;
	}

error:
	return argv;
}

/*
 * Spawn viewer with the trace directory path.
 */
int spawn_viewer(const char *trace_path, char *opt_viewer, bool opt_live_mode)
{
	int ret = 0;
	struct stat status;
	const char *viewer_bin = nullptr;
	const struct viewer *viewer;
	char **argv = nullptr;

	/* Check for --viewer option. */
	viewer = parse_viewer_option(opt_viewer);
	if (viewer == nullptr) {
		ret = -1;
		goto error;
	}

retry_viewer:
	switch (viewer->type) {
	case VIEWER_BABELTRACE2:
		if (stat(babeltrace2_bin, &status) == 0) {
			viewer_bin = babeltrace2_bin;
		} else {
			viewer_bin = viewer->exec_name;
		}
		argv = alloc_argv_from_local_opts(
			babeltrace2_opts, ARRAY_SIZE(babeltrace2_opts), trace_path, opt_live_mode);
		break;
	case VIEWER_BABELTRACE:
		if (stat(babeltrace_bin, &status) == 0) {
			viewer_bin = babeltrace_bin;
		} else {
			viewer_bin = viewer->exec_name;
		}
		argv = alloc_argv_from_local_opts(
			babeltrace_opts, ARRAY_SIZE(babeltrace_opts), trace_path, opt_live_mode);
		break;
	case VIEWER_USER_DEFINED:
		argv = alloc_argv_from_user_opts(opt_viewer, trace_path);
		if (argv) {
			viewer_bin = argv[0];
		}
		break;
	default:
		abort();
	}

	if (argv == nullptr || !viewer_bin) {
		ret = -1;
		goto error;
	}

	DBG("Using %s viewer", viewer_bin);

	ret = execvp(viewer_bin, argv);
	if (ret) {
		if (errno == ENOENT && viewer->exec_name) {
			if (viewer->type == VIEWER_BABELTRACE2) {
				/* Fallback to legacy babeltrace. */
				DBG("Default viewer \"%s\" not installed on the system, falling back to \"%s\"",
				    viewers[VIEWER_BABELTRACE2].exec_name,
				    viewers[VIEWER_BABELTRACE].exec_name);
				viewer = &viewers[VIEWER_BABELTRACE];
				free(argv);
				argv = nullptr;
				goto retry_viewer;
			} else {
				ERR("Default viewer \"%s\" (and fallback \"%s\") not found on the system",
				    viewers[VIEWER_BABELTRACE2].exec_name,
				    viewers[VIEWER_BABELTRACE].exec_name);
			}
		} else {
			PERROR("Failed to launch \"%s\" viewer", viewer_bin);
		}
		ret = -1;
		goto error;
	}

	/*
	 * This function should never return if successfull because `execvp(3)`
	 * onle returns if an error has occurred.
	 */
	LTTNG_ASSERT(ret != 0);
error:
	free(argv);
	return ret;
}
