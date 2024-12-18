/*
 * Copyright (C) 2012 David Goulet <dgoulet@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE
#include "inet6.hpp"

#include <common/common.hpp>
#include <common/compat/errno.hpp>
#include <common/compat/time.hpp>
#include <common/time.hpp>

#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define RECONNECT_DELAY 200 /* ms */

/*
 * INET protocol operations.
 */
static const struct lttcomm_proto_ops inet6_ops = {
	.bind = lttcomm_bind_inet6_sock,
	.close = lttcomm_close_inet6_sock,
	.connect = lttcomm_connect_inet6_sock,
	.accept = lttcomm_accept_inet6_sock,
	.listen = lttcomm_listen_inet6_sock,
	.recvmsg = lttcomm_recvmsg_inet6_sock,
	.sendmsg = lttcomm_sendmsg_inet6_sock,
};

/*
 * Creates an PF_INET socket.
 */
int lttcomm_create_inet6_sock(struct lttcomm_sock *sock, int type, int proto)
{
	int val = 1, ret;
	unsigned long timeout;

	/* Create server socket */
	if ((sock->fd = socket(PF_INET6, type, proto)) < 0) {
		PERROR("socket inet6");
		goto error;
	}

	sock->ops = &inet6_ops;

	/*
	 * Set socket option to reuse the address.
	 */
	ret = setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
	if (ret < 0) {
		PERROR("setsockopt inet6");
		goto error;
	}
	timeout = lttcomm_get_network_timeout();
	if (timeout) {
		ret = lttcomm_setsockopt_rcv_timeout(sock->fd, timeout);
		if (ret) {
			goto error;
		}
		ret = lttcomm_setsockopt_snd_timeout(sock->fd, timeout);
		if (ret) {
			goto error;
		}
	}

	return 0;

error:
	return -1;
}

/*
 * Bind socket and return.
 */
int lttcomm_bind_inet6_sock(struct lttcomm_sock *sock)
{
	struct sockaddr_in6 sockaddr = sock->sockaddr.addr.sin6;
	int ret;

	ret = bind(sock->fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
	if (ret) {
		return ret;
	}

	if (sockaddr.sin6_port == 0) {
		socklen_t socklen = sizeof(sock->sockaddr.addr.sin);
		ret = getsockname(sock->fd, (struct sockaddr *) &sock->sockaddr.addr.sin, &socklen);
	}

	return ret;
}

static int connect_no_timeout(struct lttcomm_sock *sock)
{
	struct sockaddr_in6 sockaddr = sock->sockaddr.addr.sin6;
	return connect(sock->fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
}

static int connect_with_timeout(struct lttcomm_sock *sock)
{
	const unsigned long timeout = lttcomm_get_network_timeout();
	int ret, flags, connect_ret;
	struct timespec orig_time, cur_time;
	unsigned long diff_ms;
	struct sockaddr_in6 sockaddr;

	ret = fcntl(sock->fd, F_GETFL, 0);
	if (ret == -1) {
		PERROR("fcntl");
		return -1;
	}
	flags = ret;

	/* Set socket to nonblock */
	ret = fcntl(sock->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		PERROR("fcntl");
		return -1;
	}

	ret = lttng_clock_gettime(CLOCK_MONOTONIC, &orig_time);
	if (ret == -1) {
		PERROR("clock_gettime");
		return -1;
	}

	sockaddr = sock->sockaddr.addr.sin6;
	connect_ret = connect(sock->fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
	if (connect_ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINPROGRESS) {
		goto error;
	} else if (!connect_ret) {
		/* Connect succeeded */
		goto success;
	}

	DBG("Asynchronous connect for sock %d, performing polling with"
	    " timeout: %lums",
	    sock->fd,
	    timeout);

	/*
	 * Perform poll loop following EINPROGRESS recommendation from
	 * connect(2) man page.
	 */
	do {
		struct pollfd fds;

		fds.fd = sock->fd;
		fds.events = POLLOUT;
		fds.revents = 0;
		ret = poll(&fds, 1, RECONNECT_DELAY);
		if (ret < 0) {
			goto error;
		} else if (ret > 0) {
			int optval;
			socklen_t optval_len = sizeof(optval);

			if (!(fds.revents & POLLOUT)) {
				/* Either hup or error */
				errno = EPIPE;
				goto error;
			}
			/* got something */
			ret = getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &optval, &optval_len);
			if (ret) {
				PERROR("getsockopt");
				goto error;
			}
			if (!optval) {
				connect_ret = 0;
				goto success;
			} else {
				/* Get actual connect() errno from opt_val */
				errno = optval;
				goto error;
			}
		}
		/* ret == 0: timeout */
		ret = lttng_clock_gettime(CLOCK_MONOTONIC, &cur_time);
		if (ret == -1) {
			PERROR("clock_gettime");
			connect_ret = ret;
			goto error;
		}
		if (timespec_to_ms(timespec_abs_diff(cur_time, orig_time), &diff_ms) < 0) {
			ERR("timespec_to_ms input overflows milliseconds output");
			connect_ret = -1;
			goto error;
		}
	} while (diff_ms < timeout);

	/* Timeout */
	errno = ETIMEDOUT;
	connect_ret = -1;

success:
	/* Restore initial flags */
	ret = fcntl(sock->fd, F_SETFL, flags);
	if (ret == -1) {
		PERROR("fcntl");
		/* Continue anyway */
	}
error:
	return connect_ret;
}

/*
 * Connect PF_INET socket.
 */
int lttcomm_connect_inet6_sock(struct lttcomm_sock *sock)
{
	int ret, closeret;

	if (lttcomm_get_network_timeout()) {
		ret = connect_with_timeout(sock);
	} else {
		ret = connect_no_timeout(sock);
	}
	if (ret < 0) {
		PERROR("connect inet6");
		goto error_connect;
	}

	return ret;

error_connect:
	closeret = close(sock->fd);
	if (closeret) {
		PERROR("close inet6");
	}

	return ret;
}

/*
 * Do an accept(2) on the sock and return the new lttcomm socket. The socket
 * MUST be bind(2) before.
 */
struct lttcomm_sock *lttcomm_accept_inet6_sock(struct lttcomm_sock *sock)
{
	int new_fd;
	socklen_t len;
	struct lttcomm_sock *new_sock;
	struct sockaddr_in6 new_addr = {};

	if (sock->proto == LTTCOMM_SOCK_UDP) {
		/*
		 * accept(2) does not exist for UDP so simply return the passed socket.
		 */
		new_sock = sock;
		goto end;
	}

	new_sock = lttcomm_alloc_sock(sock->proto);
	if (new_sock == nullptr) {
		goto error;
	}

	len = sizeof(new_addr);

	/* Blocking call */
	new_fd = accept(sock->fd, (struct sockaddr *) &new_addr, &len);
	if (new_fd < 0) {
		PERROR("accept inet6");
		goto error;
	}
	new_sock->sockaddr.addr.sin6 = new_addr;
	new_sock->fd = new_fd;
	new_sock->ops = &inet6_ops;

end:
	return new_sock;

error:
	free(new_sock);
	return nullptr;
}

/*
 * Make the socket listen using LTTNG_SESSIOND_COMM_MAX_LISTEN.
 */
int lttcomm_listen_inet6_sock(struct lttcomm_sock *sock, int backlog)
{
	int ret;

	if (sock->proto == LTTCOMM_SOCK_UDP) {
		/* listen(2) does not exist for UDP so simply return success. */
		ret = 0;
		goto end;
	}

	/* Default listen backlog */
	if (backlog <= 0) {
		backlog = LTTNG_SESSIOND_COMM_MAX_LISTEN;
	}

	ret = listen(sock->fd, backlog);
	if (ret < 0) {
		PERROR("listen inet6");
	}

end:
	return ret;
}

/*
 * Receive data of size len in put that data into the buf param. Using recvmsg
 * API.
 *
 * Return the size of received data.
 */
ssize_t lttcomm_recvmsg_inet6_sock(struct lttcomm_sock *sock, void *buf, size_t len, int flags)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t ret = -1;
	size_t len_last;
	struct sockaddr_in6 addr = sock->sockaddr.addr.sin6;

	memset(&msg, 0, sizeof(msg));

	iov[0].iov_base = buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	msg.msg_name = (struct sockaddr *) &addr;
	msg.msg_namelen = sizeof(sock->sockaddr.addr.sin6);

	do {
		len_last = iov[0].iov_len;
		ret = recvmsg(sock->fd, &msg, flags);
		if (ret > 0) {
			if (flags & MSG_DONTWAIT) {
				goto end;
			}
			iov[0].iov_base = ((char *) iov[0].iov_base) + ret;
			iov[0].iov_len -= ret;
			LTTNG_ASSERT(ret <= len_last);
		}
	} while ((ret > 0 && ret < len_last) || (ret < 0 && errno == EINTR));
	if (ret < 0) {
		PERROR("recvmsg inet");
	} else if (ret > 0) {
		ret = len;
	}
	/* Else ret = 0 meaning an orderly shutdown. */
end:
	return ret;
}

/*
 * Send buf data of size len. Using sendmsg API.
 *
 * Return the size of sent data.
 */
ssize_t
lttcomm_sendmsg_inet6_sock(struct lttcomm_sock *sock, const void *buf, size_t len, int flags)
{
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t ret = -1;

	memset(&msg, 0, sizeof(msg));

	iov[0].iov_base = (void *) buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	switch (sock->proto) {
	case LTTCOMM_SOCK_UDP:
	{
		struct sockaddr_in6 addr = sock->sockaddr.addr.sin6;

		msg.msg_name = (struct sockaddr *) &addr;
		msg.msg_namelen = sizeof(sock->sockaddr.addr.sin6);
		break;
	}
	default:
		break;
	}

	do {
		ret = sendmsg(sock->fd, &msg, flags);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		/*
		 * Only warn about EPIPE when quiet mode is deactivated.
		 * We consider EPIPE as expected.
		 */
		if (errno != EPIPE || !lttng_opt_quiet) {
			PERROR("sendmsg inet6");
		}
	}

	return ret;
}

/*
 * Shutdown cleanly and close.
 */
int lttcomm_close_inet6_sock(struct lttcomm_sock *sock)
{
	int ret;

	/* Don't try to close an invalid marked socket */
	if (sock->fd == -1) {
		return 0;
	}

	ret = close(sock->fd);
	if (ret) {
		PERROR("close inet6");
	}

	/* Mark socket */
	sock->fd = -1;

	return ret;
}
