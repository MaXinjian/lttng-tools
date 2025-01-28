/*
 * event-field-value.c
 *
 * Linux Trace Toolkit Control Library
 *
 * SPDX-FileCopyrightText: 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#define _LGPL_SOURCE
#include <common/error.hpp>
#include <common/macros.hpp>

#include <lttng/event-field-value-internal.hpp>

#include <stdbool.h>
#include <stddef.h>

static struct lttng_event_field_value *
create_empty_field_val(enum lttng_event_field_value_type type, size_t size)
{
	struct lttng_event_field_value *field_val;

	field_val = zmalloc<lttng_event_field_value>(size);
	if (!field_val) {
		goto end;
	}

	field_val->type = type;

end:
	return field_val;
}

struct lttng_event_field_value *lttng_event_field_value_uint_create(uint64_t val)
{
	struct lttng_event_field_value_uint *field_val;

	field_val = lttng::utils::container_of(
		create_empty_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_INT,
				       sizeof(*field_val)),
		&lttng_event_field_value_uint::parent);
	if (!field_val) {
		goto error;
	}

	field_val->val = val;
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return &field_val->parent;
}

struct lttng_event_field_value *lttng_event_field_value_int_create(int64_t val)
{
	struct lttng_event_field_value_int *field_val;

	field_val = lttng::utils::container_of(
		create_empty_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_INT, sizeof(*field_val)),
		&lttng_event_field_value_int::parent);
	if (!field_val) {
		goto error;
	}

	field_val->val = val;
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return &field_val->parent;
}

static struct lttng_event_field_value_enum *
create_enum_field_val(enum lttng_event_field_value_type type, size_t size)
{
	struct lttng_event_field_value_enum *field_val;

	field_val = lttng::utils::container_of(create_empty_field_val(type, size),
					       &lttng_event_field_value_enum::parent);
	if (!field_val) {
		goto error;
	}

	lttng_dynamic_pointer_array_init(&field_val->labels, free);
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return field_val;
}

struct lttng_event_field_value *lttng_event_field_value_enum_uint_create(uint64_t val)
{
	struct lttng_event_field_value_enum_uint *field_val;

	field_val = lttng::utils::container_of(
		create_enum_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_ENUM,
				      sizeof(*field_val)),
		&lttng_event_field_value_enum_uint::parent);
	if (!field_val) {
		goto error;
	}

	field_val->val = val;
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent.parent);

end:
	return &field_val->parent.parent;
}

struct lttng_event_field_value *lttng_event_field_value_enum_int_create(int64_t val)
{
	struct lttng_event_field_value_enum_int *field_val;

	field_val = lttng::utils::container_of(
		create_enum_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_ENUM, sizeof(*field_val)),
		&lttng_event_field_value_enum_int::parent);
	if (!field_val) {
		goto error;
	}

	field_val->val = val;
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent.parent);

end:
	return &field_val->parent.parent;
}

struct lttng_event_field_value *lttng_event_field_value_real_create(double val)
{
	struct lttng_event_field_value_real *field_val = lttng::utils::container_of(
		create_empty_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_REAL, sizeof(*field_val)),
		&lttng_event_field_value_real::parent);

	if (!field_val) {
		goto error;
	}

	field_val->val = val;
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return &field_val->parent;
}

struct lttng_event_field_value *lttng_event_field_value_string_create_with_size(const char *val,
										size_t size)
{
	struct lttng_event_field_value_string *field_val = lttng::utils::container_of(
		create_empty_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_STRING, sizeof(*field_val)),
		&lttng_event_field_value_string::parent);

	if (!field_val) {
		goto error;
	}

	if (size) {
		LTTNG_ASSERT(val);
		field_val->val = strndup(val, size);
	} else {
		/*
		 * User code do not expect a NULL string pointer. Populate with
		 * an empty string when length is 0.
		 */
		field_val->val = strdup("");
	}
	if (!field_val->val) {
		goto error;
	}

	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return &field_val->parent;
}

struct lttng_event_field_value *lttng_event_field_value_string_create(const char *val)
{
	LTTNG_ASSERT(val);
	return lttng_event_field_value_string_create_with_size(val, strlen(val));
}

static void destroy_field_val(void *field_val)
{
	lttng_event_field_value_destroy((lttng_event_field_value *) field_val);
}

struct lttng_event_field_value *lttng_event_field_value_array_create()
{
	struct lttng_event_field_value_array *field_val = lttng::utils::container_of(
		create_empty_field_val(LTTNG_EVENT_FIELD_VALUE_TYPE_ARRAY, sizeof(*field_val)),
		&lttng_event_field_value_array::parent);

	if (!field_val) {
		goto error;
	}

	lttng_dynamic_pointer_array_init(&field_val->elems, destroy_field_val);
	goto end;

error:
	lttng_event_field_value_destroy(&field_val->parent);

end:
	return &field_val->parent;
}

void lttng_event_field_value_destroy(struct lttng_event_field_value *field_val)
{
	if (!field_val) {
		goto end;
	}

	switch (field_val->type) {
	case LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_ENUM:
	case LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_ENUM:
	{
		struct lttng_event_field_value_enum *enum_field_val = lttng::utils::container_of(
			field_val, &lttng_event_field_value_enum::parent);

		lttng_dynamic_pointer_array_reset(&enum_field_val->labels);
		break;
	}
	case LTTNG_EVENT_FIELD_VALUE_TYPE_STRING:
	{
		struct lttng_event_field_value_string *str_field_val = lttng::utils::container_of(
			field_val, &lttng_event_field_value_string::parent);

		free(str_field_val->val);
		break;
	}
	case LTTNG_EVENT_FIELD_VALUE_TYPE_ARRAY:
	{
		struct lttng_event_field_value_array *array_field_expr = lttng::utils::container_of(
			field_val, &lttng_event_field_value_array::parent);

		lttng_dynamic_pointer_array_reset(&array_field_expr->elems);
		break;
	}
	default:
		break;
	}

	free(field_val);

end:
	return;
}

int lttng_event_field_value_enum_append_label_with_size(struct lttng_event_field_value *field_val,
							const char *label,
							size_t size)
{
	int ret;
	char *new_label;

	LTTNG_ASSERT(field_val);
	LTTNG_ASSERT(label);
	new_label = strndup(label, size);
	if (!new_label) {
		ret = -1;
		goto end;
	}

	ret = lttng_dynamic_pointer_array_add_pointer(
		&lttng::utils::container_of(field_val, &lttng_event_field_value_enum::parent)
			 ->labels,
		new_label);
	if (ret == 0) {
		new_label = nullptr;
	}

end:
	free(new_label);
	return ret;
}

int lttng_event_field_value_enum_append_label(struct lttng_event_field_value *field_val,
					      const char *label)
{
	LTTNG_ASSERT(label);
	return lttng_event_field_value_enum_append_label_with_size(field_val, label, strlen(label));
}

int lttng_event_field_value_array_append(struct lttng_event_field_value *array_field_val,
					 struct lttng_event_field_value *field_val)
{
	LTTNG_ASSERT(array_field_val);
	LTTNG_ASSERT(field_val);
	return lttng_dynamic_pointer_array_add_pointer(
		&lttng::utils::container_of(array_field_val, &lttng_event_field_value_array::parent)
			 ->elems,
		field_val);
}

int lttng_event_field_value_array_append_unavailable(struct lttng_event_field_value *array_field_val)
{
	LTTNG_ASSERT(array_field_val);
	return lttng_dynamic_pointer_array_add_pointer(
		&lttng::utils::container_of(array_field_val, &lttng_event_field_value_array::parent)
			 ->elems,
		nullptr);
}

enum lttng_event_field_value_type
lttng_event_field_value_get_type(const struct lttng_event_field_value *field_val)
{
	enum lttng_event_field_value_type type;

	if (!field_val) {
		type = LTTNG_EVENT_FIELD_VALUE_TYPE_INVALID;
		goto end;
	}

	type = field_val->type;

end:
	return type;
}

enum lttng_event_field_value_status
lttng_event_field_value_unsigned_int_get_value(const struct lttng_event_field_value *field_val,
					       uint64_t *val)
{
	enum lttng_event_field_value_status status;

	if (!field_val || !val) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	switch (field_val->type) {
	case LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_INT:
		*val = lttng::utils::container_of(field_val, &lttng_event_field_value_uint::parent)
			       ->val;
		break;
	case LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_ENUM:
	{
		const struct lttng_event_field_value_enum *field_val_enum =
			lttng::utils::container_of(field_val,
						   &lttng_event_field_value_enum::parent);
		const struct lttng_event_field_value_enum_uint *field_val_enum_uint =
			lttng::utils::container_of(field_val_enum,
						   &lttng_event_field_value_enum_uint::parent);
		*val = field_val_enum_uint->val;
		break;
	}
	default:
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

enum lttng_event_field_value_status
lttng_event_field_value_signed_int_get_value(const struct lttng_event_field_value *field_val,
					     int64_t *val)
{
	enum lttng_event_field_value_status status;

	if (!field_val || !val) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	switch (field_val->type) {
	case LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_INT:
		*val = lttng::utils::container_of(field_val, &lttng_event_field_value_int::parent)
			       ->val;
		break;
	case LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_ENUM:
	{
		const struct lttng_event_field_value_enum *field_val_enum =
			lttng::utils::container_of(field_val,
						   &lttng_event_field_value_enum::parent);
		const struct lttng_event_field_value_enum_int *field_val_enum_uint =
			lttng::utils::container_of(field_val_enum,
						   &lttng_event_field_value_enum_int::parent);
		*val = field_val_enum_uint->val;
		break;
	}
	default:
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

enum lttng_event_field_value_status
lttng_event_field_value_real_get_value(const struct lttng_event_field_value *field_val, double *val)
{
	enum lttng_event_field_value_status status;

	if (!field_val || field_val->type != LTTNG_EVENT_FIELD_VALUE_TYPE_REAL || !val) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	*val = lttng::utils::container_of(field_val, &lttng_event_field_value_real::parent)->val;
	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

static bool is_enum_field_val(const struct lttng_event_field_value *field_val)
{
	return field_val->type == LTTNG_EVENT_FIELD_VALUE_TYPE_UNSIGNED_ENUM ||
		field_val->type == LTTNG_EVENT_FIELD_VALUE_TYPE_SIGNED_ENUM;
}

enum lttng_event_field_value_status
lttng_event_field_value_enum_get_label_count(const struct lttng_event_field_value *field_val,
					     unsigned int *count)
{
	enum lttng_event_field_value_status status;

	if (!field_val || !is_enum_field_val(field_val) || !count) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	*count = (unsigned int) lttng_dynamic_pointer_array_get_count(
		&lttng::utils::container_of(field_val, &lttng_event_field_value_enum::parent)
			 ->labels);
	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

const char *
lttng_event_field_value_enum_get_label_at_index(const struct lttng_event_field_value *field_val,
						unsigned int index)
{
	const char *ret;
	const struct lttng_event_field_value_enum *enum_field_val;

	if (!field_val || !is_enum_field_val(field_val)) {
		ret = nullptr;
		goto end;
	}

	enum_field_val =
		lttng::utils::container_of(field_val, &lttng_event_field_value_enum::parent);

	if (index >= lttng_dynamic_pointer_array_get_count(&enum_field_val->labels)) {
		ret = nullptr;
		goto end;
	}

	ret = (const char *) lttng_dynamic_pointer_array_get_pointer(&enum_field_val->labels,
								     index);

end:
	return ret;
}

enum lttng_event_field_value_status
lttng_event_field_value_string_get_value(const struct lttng_event_field_value *field_val,
					 const char **value)
{
	enum lttng_event_field_value_status status;

	if (!field_val || field_val->type != LTTNG_EVENT_FIELD_VALUE_TYPE_STRING) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	*value =
		lttng::utils::container_of(field_val, &lttng_event_field_value_string::parent)->val;
	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

enum lttng_event_field_value_status
lttng_event_field_value_array_get_length(const struct lttng_event_field_value *field_val,
					 unsigned int *length)
{
	enum lttng_event_field_value_status status;

	if (!field_val || field_val->type != LTTNG_EVENT_FIELD_VALUE_TYPE_ARRAY || !length) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	*length = (unsigned int) lttng_dynamic_pointer_array_get_count(
		&lttng::utils::container_of(field_val, &lttng_event_field_value_array::parent)
			 ->elems);
	status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;

end:
	return status;
}

enum lttng_event_field_value_status lttng_event_field_value_array_get_element_at_index(
	const struct lttng_event_field_value *field_val,
	unsigned int index,
	const struct lttng_event_field_value **elem_field_val)
{
	enum lttng_event_field_value_status status;
	const struct lttng_event_field_value_array *array_field_val;

	if (!field_val || field_val->type != LTTNG_EVENT_FIELD_VALUE_TYPE_ARRAY ||
	    !elem_field_val) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	array_field_val =
		lttng::utils::container_of(field_val, &lttng_event_field_value_array::parent);

	if (index >= lttng_dynamic_pointer_array_get_count(&array_field_val->elems)) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_INVALID;
		goto end;
	}

	*elem_field_val = (lttng_event_field_value *) lttng_dynamic_pointer_array_get_pointer(
		&array_field_val->elems, index);
	if (*elem_field_val) {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_OK;
	} else {
		status = LTTNG_EVENT_FIELD_VALUE_STATUS_UNAVAILABLE;
	}

end:
	return status;
}
