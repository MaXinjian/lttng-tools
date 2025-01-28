/*
 * SPDX-FileCopyrightText: 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#define _LGPL_SOURCE
#include "readwrite.hpp"

#include <common/compat/errno.hpp>

#include <limits.h>
#include <unistd.h>

/*
 * lttng_read and lttng_write take care of EINTR and partial read/write.
 * Upon success, they return the "count" received as parameter.
 * They can return a negative value if an error occurs.
 * If a value lower than the requested "count" is returned, it means an
 * error occurred.
 * The error can be checked by querying errno.
 */
ssize_t lttng_read(int fd, void *buf, size_t count)
{
	size_t i = 0;
	ssize_t ret;

	LTTNG_ASSERT(buf);

	/*
	 * Deny a read count that can be bigger then the returned value max size.
	 * This makes the function to never return an overflow value.
	 */
	if (count > SSIZE_MAX) {
		return -EINVAL;
	}

	do {
		ret = read(fd, (char *) buf + i, count - i);
		if (ret < 0) {
			if (errno == EINTR) {
				continue; /* retry operation */
			} else {
				goto error;
			}
		}
		i += ret;
		LTTNG_ASSERT(i <= count);
	} while (count - i > 0 && ret > 0);
	return i;

error:
	if (i == 0) {
		return -1;
	} else {
		return i;
	}
}

ssize_t lttng_write(int fd, const void *buf, size_t count)
{
	size_t i = 0;
	ssize_t ret;

	LTTNG_ASSERT(buf);

	/*
	 * Deny a write count that can be bigger then the returned value max size.
	 * This makes the function to never return an overflow value.
	 */
	if (count > SSIZE_MAX) {
		return -EINVAL;
	}

	do {
		ret = write(fd, (char *) buf + i, count - i);
		if (ret < 0) {
			if (errno == EINTR) {
				continue; /* retry operation */
			} else {
				goto error;
			}
		}
		i += ret;
		LTTNG_ASSERT(i <= count);
	} while (count - i > 0 && ret > 0);
	return i;

error:
	if (i == 0) {
		return -1;
	} else {
		return i;
	}
}
