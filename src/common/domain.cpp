/*
 * Copyright (C) 2020 Simon Marchi <simon.marchi@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include "common/macros.hpp"
#include "lttng/domain-internal.hpp"

const char *lttng_domain_type_str(enum lttng_domain_type domain_type)
{
	switch (domain_type) {
	case LTTNG_DOMAIN_NONE:
		return "none";
	case LTTNG_DOMAIN_KERNEL:
		return "kernel";
	case LTTNG_DOMAIN_UST:
		return "user space";
	case LTTNG_DOMAIN_JUL:
		return "java.util.logging (JUL)";
	case LTTNG_DOMAIN_LOG4J:
		return "log4j";
	case LTTNG_DOMAIN_LOG4J2:
		return "log4j2";
	case LTTNG_DOMAIN_PYTHON:
		return "Python logging";
	default:
		return "???";
	}
}
