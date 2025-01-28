/*
 * SPDX-FileCopyrightText: 2023 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "obj.h"
#include "tp.h"

void test_constructor5() __attribute__((constructor));
void test_constructor5()
{
	tracepoint(tp, constructor_c_across_units_after_provider);
}

void test_destructor5() __attribute__((destructor));
void test_destructor5()
{
	tracepoint(tp, destructor_c_across_units_after_provider);
}

Obj g_obj_across_units_after_provider("global - across units after provider");
