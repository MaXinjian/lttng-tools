/*
 * SPDX-FileCopyrightText: 2023 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "obj.h"
#include "tp.h"

void test_constructor1() __attribute__((constructor));
void test_constructor1()
{
	tracepoint(tp, constructor_c_across_units_before_define);
}

void test_destructor1() __attribute__((destructor));
void test_destructor1()
{
	tracepoint(tp, destructor_c_across_units_before_define);
}

Obj g_obj_across_units_before_define("global - across units before define");
