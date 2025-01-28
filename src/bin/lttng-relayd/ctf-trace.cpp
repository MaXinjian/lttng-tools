/*
 * SPDX-FileCopyrightText: 2013 Julien Desfossez <jdesfossez@efficios.com>
 * SPDX-FileCopyrightText: 2013 David Goulet <dgoulet@efficios.com>
 * SPDX-FileCopyrightText: 2015 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE

#include "ctf-trace.hpp"
#include "lttng-relayd.hpp"
#include "stream.hpp"

#include <common/common.hpp>
#include <common/urcu.hpp>
#include <common/utils.hpp>

#include <urcu/rculist.h>

static uint64_t last_relay_ctf_trace_id;
static pthread_mutex_t last_relay_ctf_trace_id_lock = PTHREAD_MUTEX_INITIALIZER;

static void rcu_destroy_ctf_trace(struct rcu_head *rcu_head)
{
	struct ctf_trace *trace = lttng::utils::container_of(rcu_head, &ctf_trace::rcu_node);

	free(trace);
}

/*
 * Destroy a ctf trace and all stream contained in it.
 *
 * MUST be called with the RCU read side lock.
 */
static void ctf_trace_destroy(struct ctf_trace *trace)
{
	/*
	 * Getting to this point, every stream referenced by that trace
	 * have put back their ref since the've been closed by the
	 * control side.
	 */
	LTTNG_ASSERT(cds_list_empty(&trace->stream_list));
	ASSERT_RCU_READ_LOCKED();

	session_put(trace->session);
	trace->session = nullptr;
	free(trace->path);
	trace->path = nullptr;
	call_rcu(&trace->rcu_node, rcu_destroy_ctf_trace);
}

static void ctf_trace_release(struct urcu_ref *ref)
{
	struct ctf_trace *trace = lttng::utils::container_of(ref, &ctf_trace::ref);
	int ret;
	struct lttng_ht_iter iter;

	iter.iter.node = &trace->node.node;
	ret = lttng_ht_del(trace->session->ctf_traces_ht, &iter);
	LTTNG_ASSERT(!ret);
	ctf_trace_destroy(trace);
}

/*
 * The caller must either:
 * - hold the RCU read side lock, or
 * - guarantee the existence of the object by already holding a reference to
 *   the object.
 */
bool ctf_trace_get(struct ctf_trace *trace)
{
	const bool ref = urcu_ref_get_unless_zero(&trace->ref);

	if (!ref) {
		/*
		 * The ref count is already zero. It means the object is being
		 * torn down concurently.
		 * This is only acceptable if we hold the RCU read-side lock,
		 * else it's a logic error.
		 */
		ASSERT_RCU_READ_LOCKED();
	}

	return ref;
}

/*
 * Create and return an allocated ctf_trace. NULL on error.
 * There is no "open" and "close" for a ctf_trace, but rather just a
 * create and refcounting. Whenever all the streams belonging to a trace
 * put their reference, its refcount drops to 0.
 */
static struct ctf_trace *ctf_trace_create(struct relay_session *session, const char *subpath)
{
	struct ctf_trace *trace;

	trace = zmalloc<ctf_trace>();
	if (!trace) {
		PERROR("Failed to allocate ctf_trace");
		goto end;
	}
	urcu_ref_init(&trace->ref);

	if (!session_get(session)) {
		ERR("Failed to acquire session reference");
		goto error;
	}
	trace->session = session;
	trace->path = strdup(subpath);
	if (!trace->path) {
		goto error;
	}

	CDS_INIT_LIST_HEAD(&trace->stream_list);

	pthread_mutex_lock(&last_relay_ctf_trace_id_lock);
	trace->id = ++last_relay_ctf_trace_id;
	pthread_mutex_unlock(&last_relay_ctf_trace_id_lock);

	lttng_ht_node_init_str(&trace->node, trace->path);
	trace->session = session;
	pthread_mutex_init(&trace->lock, nullptr);
	pthread_mutex_init(&trace->stream_list_lock, nullptr);
	lttng_ht_add_str(session->ctf_traces_ht, &trace->node);

	DBG("Created ctf_trace %" PRIu64 " of session \"%s\" from host \"%s\" with path: %s",
	    trace->id,
	    session->session_name,
	    session->hostname,
	    subpath);

end:
	return trace;
error:
	ctf_trace_put(trace);
	return nullptr;
}

/*
 * Return a ctf_trace if found by id in the given hash table else NULL.
 * Hold a reference on the ctf_trace, and must be paired with
 * ctf_trace_put().
 */
struct ctf_trace *ctf_trace_get_by_path_or_create(struct relay_session *session,
						  const char *subpath)
{
	struct lttng_ht_node_str *node;
	struct lttng_ht_iter iter;
	struct ctf_trace *trace = nullptr;

	const lttng::urcu::read_lock_guard read_lock;
	lttng_ht_lookup(session->ctf_traces_ht, subpath, &iter);
	node = lttng_ht_iter_get_node<lttng_ht_node_str>(&iter);
	if (!node) {
		DBG("CTF Trace path %s not found", subpath);
		goto end;
	}
	trace = lttng::utils::container_of(node, &ctf_trace::node);
	if (!ctf_trace_get(trace)) {
		trace = nullptr;
	}
end:
	if (!trace) {
		/* Try to create */
		trace = ctf_trace_create(session, subpath);
	}
	return trace;
}

void ctf_trace_put(struct ctf_trace *trace)
{
	const lttng::urcu::read_lock_guard read_lock;
	urcu_ref_put(&trace->ref, ctf_trace_release);
}

int ctf_trace_close(struct ctf_trace *trace)
{
	for (auto *stream :
	     lttng::urcu::rcu_list_iteration_adapter<relay_stream, &relay_stream::stream_node>(
		     trace->stream_list)) {
		/*
		 * Close stream since the connection owning the trace is being
		 * torn down.
		 */
		try_stream_close(stream);
	}

	/*
	 * Since all references to the trace are held by its streams, we
	 * don't need to do any self-ref put.
	 */
	return 0;
}

struct relay_viewer_stream *ctf_trace_get_viewer_metadata_stream(struct ctf_trace *trace)
{
	struct relay_viewer_stream *vstream;

	const lttng::urcu::read_lock_guard read_lock;
	vstream = rcu_dereference(trace->viewer_metadata_stream);
	if (!vstream) {
		goto end;
	}
	if (!viewer_stream_get(vstream)) {
		vstream = nullptr;
	}
end:
	return vstream;
}
