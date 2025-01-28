/*
 * SPDX-FileCopyrightText: 2013 David Goulet <dgoulet@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#ifndef LTTNG_CTL_HELPER_H
#define LTTNG_CTL_HELPER_H

#include <common/sessiond-comm/sessiond-comm.hpp>

#include <lttng/lttng.h>

#include <stdio.h>

/* Copy helper functions. */
void lttng_ctl_copy_lttng_domain(struct lttng_domain *dst, struct lttng_domain *src);

/*
 * Sends the lttcomm message to the session daemon and fills buf if the
 * returned data is not NULL.
 *
 * Return the size of the received data on success or else a negative lttng
 * error code. If buf is NULL, 0 is returned on success.
 */
int lttng_ctl_ask_sessiond_fds_varlen(struct lttcomm_session_msg *lsm,
				      const int *fds,
				      size_t nb_fd,
				      const void *vardata,
				      size_t vardata_len,
				      void **user_payload_buf,
				      void **user_cmd_header_buf,
				      size_t *user_cmd_header_len);

/*
 * Sends the lttcomm message to the session daemon and fills the reply payload.
 *
 * Return the size of the received data on success or else a negative lttng
 * error code.
 */
int lttng_ctl_ask_sessiond_payload(struct lttng_payload_view *message, struct lttng_payload *reply);

/*
 * Calls lttng_ctl_ask_sessiond_fds_varlen() with no expected command header.
 */
static inline int lttng_ctl_ask_sessiond_varlen_no_cmd_header(struct lttcomm_session_msg *lsm,
							      const void *vardata,
							      size_t vardata_len,
							      void **user_payload_buf)
{
	return lttng_ctl_ask_sessiond_fds_varlen(
		lsm, nullptr, 0, vardata, vardata_len, user_payload_buf, nullptr, nullptr);
}

/*
 * Calls lttng_ctl_ask_sessiond_fds_varlen() with fds and no expected command header.
 */
static inline int lttng_ctl_ask_sessiond_fds_no_cmd_header(struct lttcomm_session_msg *lsm,
							   const int *fds,
							   size_t nb_fd,
							   void **buf __attribute__((unused)))
{
	return lttng_ctl_ask_sessiond_fds_varlen(
		lsm, fds, nb_fd, nullptr, 0, nullptr, nullptr, nullptr);
}
/*
 * Use this if no variable length data needs to be sent.
 */
static inline int lttng_ctl_ask_sessiond(struct lttcomm_session_msg *lsm, void **buf)
{
	return lttng_ctl_ask_sessiond_varlen_no_cmd_header(lsm, nullptr, 0, buf);
}

int lttng_check_tracing_group();

int connect_sessiond();

#endif /* LTTNG_CTL_HELPER_H */
