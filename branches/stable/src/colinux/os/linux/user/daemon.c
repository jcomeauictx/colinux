/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include <colinux/os/user/daemon.h>
#include <colinux/os/user/pipe.h>
#include <colinux/os/alloc.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "frame.h"

struct co_daemon_handle {
	int sock;
	co_os_frame_collector_t frame;
};

co_rc_t
co_os_open_daemon_pipe(co_id_t linux_id, co_module_t module_id,
		       co_daemon_handle_t *handle_out)
{
	int sock, ret;
	co_rc_t rc = CO_RC(OK);
	co_daemon_handle_t handle;
	struct sockaddr_un saddr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
		return CO_RC(ERROR);

	rc = co_os_set_blocking(sock, PFALSE);
	if (!CO_OK(rc))
		return rc;

	saddr.sun_family = AF_UNIX;
	snprintf(saddr.sun_path, sizeof(saddr.sun_path), 
		 "%s/colinux_%s.%d", "/tmp", "test", linux_id);

	ret = connect(sock, (struct sockaddr *)&saddr, sizeof (saddr));
	if (ret == -1) {
		close(sock);
		return CO_RC(ERROR);
	}

	handle = co_os_malloc(sizeof(*handle));
	if (handle == NULL) {
		close(sock);
		rc = CO_RC(ERROR); 
		goto out;
	}

	memset(handle, 0, sizeof(*handle));

	handle->sock = sock;
	handle->frame.max_buffer_size = CO_OS_PIPE_SERVER_BUFFER_SIZE;

	rc = co_os_frame_send(handle->sock, (char *)&module_id, sizeof(module_id));
	if (!CO_OK(rc)) {
		close(sock);
		co_os_free(handle);
		goto out;
	}

	*handle_out = handle;
out:
	return rc;
}

co_rc_t
co_os_daemon_get_message(co_daemon_handle_t handle,
			 co_message_t **message_out, 
			 unsigned long timeout)
{
	unsigned long size;
	co_message_t *message = NULL;
	struct pollfd poll_s;
	co_rc_t rc;
	int ret;

	poll_s.fd = handle->sock;
	poll_s.events = POLLIN | POLLHUP | POLLERR;
	poll_s.revents = 0;

	ret = poll(&poll_s, 1, timeout);
	if (ret < 1)
		return CO_RC(OK);

	rc = co_os_frame_recv(&handle->frame, handle->sock, (char **)&message, &size);
	if (!CO_OK(rc))
		return rc;

	*message_out = message;
	return rc;
}

co_rc_t
co_os_daemon_send_message(co_daemon_handle_t handle, co_message_t *message)
{
	return co_os_frame_send(handle->sock, (char *)message, message->size + sizeof(*message));
}

void
co_os_daemon_close(co_daemon_handle_t handle)
{
	close(handle->sock);
	co_os_free(handle);
}
