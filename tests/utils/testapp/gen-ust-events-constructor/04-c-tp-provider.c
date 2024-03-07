/*
 * Copyright (C) 2024 Kienan Stewart <kstewart@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

static void fct_constructor4(void);
static void fct_destructor4(void);

void test_constructor4_same_unit_before(void) __attribute__((constructor));
void test_constructor4_same_unit_before(void)
{
	fct_constructor4();
}

void test_destructor4_same_unit_before(void) __attribute__((destructor));
void test_destructor4_same_unit_before(void)
{
	fct_destructor4();
}

#define TRACEPOINT_CREATE_PROBES
#include "tp.h"

static void fct_constructor4(void)
{
	tracepoint(tp, constructor_c_same_unit_before_provider);
}

static void fct_destructor4(void)
{
	tracepoint(tp, destructor_c_same_unit_before_provider);
}

void test_constructor4_same_unit_after(void) __attribute__((constructor));
void test_constructor4_same_unit_after(void)
{
	tracepoint(tp, constructor_c_same_unit_after_provider);
}

void test_destructor4_same_unit_after(void) __attribute__((destructor));
void test_destructor4_same_unit_after(void)
{
	tracepoint(tp, destructor_c_same_unit_after_provider);
}
