/*
 * filter-visitor-ir-validate-globbing.c
 *
 * LTTng filter IR validate globbing
 *
 * SPDX-FileCopyrightText: 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include "filter-ast.hpp"
#include "filter-ir.hpp"
#include "filter-parser.hpp"

#include <common/compat/errno.hpp>
#include <common/macros.hpp>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int validate_globbing(struct ir_op *node)
{
	int ret;

	switch (node->op) {
	case IR_OP_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown op type\n", __func__);
		return -EINVAL;

	case IR_OP_ROOT:
		return validate_globbing(node->u.root.child);
	case IR_OP_LOAD:
		return 0;
	case IR_OP_UNARY:
		return validate_globbing(node->u.unary.child);
	case IR_OP_BINARY:
	{
		struct ir_op *left = node->u.binary.left;
		struct ir_op *right = node->u.binary.right;

		if (left->op == IR_OP_LOAD && right->op == IR_OP_LOAD &&
		    left->data_type == IR_DATA_STRING && right->data_type == IR_DATA_STRING) {
			/* Test 1. */
			if (left->u.load.u.string.type == IR_LOAD_STRING_TYPE_GLOB_STAR &&
			    right->u.load.u.string.type != IR_LOAD_STRING_TYPE_PLAIN) {
				fprintf(stderr, "[error] Cannot compare two globbing patterns\n");
				return -1;
			}

			if (right->u.load.u.string.type == IR_LOAD_STRING_TYPE_GLOB_STAR &&
			    left->u.load.u.string.type != IR_LOAD_STRING_TYPE_PLAIN) {
				fprintf(stderr, "[error] Cannot compare two globbing patterns\n");
				return -1;
			}
		}

		if ((left->op == IR_OP_LOAD && left->data_type == IR_DATA_STRING) ||
		    (right->op == IR_OP_LOAD && right->data_type == IR_DATA_STRING)) {
			if ((left->op == IR_OP_LOAD &&
			     left->u.load.u.string.type == IR_LOAD_STRING_TYPE_GLOB_STAR) ||
			    (right->op == IR_OP_LOAD &&
			     right->u.load.u.string.type == IR_LOAD_STRING_TYPE_GLOB_STAR)) {
				/* Test 2. */
				if (node->u.binary.type != AST_OP_EQ &&
				    node->u.binary.type != AST_OP_NE) {
					fprintf(stderr,
						"[error] Only the `==` and `!=` operators are allowed with a globbing pattern\n");
					return -1;
				}
			}
		}

		ret = validate_globbing(left);
		if (ret) {
			return ret;
		}

		return validate_globbing(right);
	}
	case IR_OP_LOGICAL:
		ret = validate_globbing(node->u.logical.left);
		if (ret)
			return ret;
		return validate_globbing(node->u.logical.right);
	}
}

/*
 * This function recursively validates that:
 *
 * 1. When there's a binary operation between two literal strings,
 *    if one of them has the IR_LOAD_STRING_TYPE_GLOB_STAR type,
 *    the other one has the IR_LOAD_STRING_TYPE_PLAIN type.
 *
 *    In other words, you cannot compare two globbing patterns, except
 *    for two globbing patterns with only a star at the end for backward
 *    compatibility reasons.
 *
 * 2. When there's a binary operation between two literal strings, if
 *    one of them is a (full) star globbing pattern, the binary
 *    operation is either == or !=.
 */
int filter_visitor_ir_validate_globbing(struct filter_parser_ctx *ctx)
{
	return validate_globbing(ctx->ir_root);
}
