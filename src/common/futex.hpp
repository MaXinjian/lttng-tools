/*
 * SPDX-FileCopyrightText: 2011 EfficiOS Inc.
 * SPDX-FileCopyrightText: 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef _LTT_FUTEX_H
#define _LTT_FUTEX_H

#include <cstdint>

void futex_wait_update(int32_t *futex, int active);
void futex_nto1_prepare(int32_t *futex);
void futex_nto1_wait(int32_t *futex);
void futex_nto1_wake(int32_t *futex);

#endif /* _LTT_FUTEX_H */
