/*
 * SPDX-FileCopyrightText: 2021 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "ust-sigbus.hpp"

#include <lttng/lttng-export.h>
#include <lttng/ust-ctl.h>
#include <lttng/ust-sigbus.h>

LTTNG_EXPORT DEFINE_LTTNG_UST_SIGBUS_STATE();

void lttng_ust_handle_sigbus(void *address)
{
	lttng_ust_ctl_sigbus_handle(address);
}
