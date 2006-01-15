/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 * Alejandro R. Sedeno <asedeno@mit.edu>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include <windows.h>
#include <stdio.h>

#include <colinux/common/common.h>
#include <colinux/user/debug.h>
#include <colinux/os/user/misc.h>
#include <colinux/user/cmdline.h>
#include <colinux/common/common.h>
#include "../daemon.h"

COLINUX_DEFINE_MODULE("colinux-serial-daemon");

/*******************************************************************************
 * Type Declarations
 */

typedef struct co_win32_overlapped {
	HANDLE write_handle;
	HANDLE read_handle;
	HANDLE read_event;
	HANDLE write_event;
	OVERLAPPED read_overlapped;
	OVERLAPPED write_overlapped;
	char buffer[0x10000];
	unsigned long size;
} co_win32_overlapped_t;

typedef struct start_parameters {
	bool_t show_help;
	int index;
	int instance;
} start_parameters_t;

/*******************************************************************************
 * Globals
 */
start_parameters_t *daemon_parameters;

co_win32_overlapped_t stdio_overlapped, daemon_overlapped;

co_rc_t co_win32_overlapped_write_async(co_win32_overlapped_t *overlapped,
					void *buffer, unsigned long size)
{
	unsigned long write_size;
	BOOL result;
	DWORD error;

	result = WriteFile(overlapped->write_handle, buffer, size,
			   &write_size, &overlapped->write_overlapped);
	
	if (!result) { 
		switch (error = GetLastError())
		{ 
		case ERROR_IO_PENDING: 
			WaitForSingleObject(overlapped->write_event, INFINITE);
			break;
		default:
			return CO_RC(ERROR);
		} 
	}

	return CO_RC(OK);
}

co_rc_t co_win32_overlapped_read_received(co_win32_overlapped_t *overlapped)
{
	if (overlapped == &daemon_overlapped) {
		/* Received packet from daemon */
		co_message_t *message;

		message = (co_message_t *)overlapped->buffer;

		co_win32_overlapped_write_async(&stdio_overlapped, message->data, message->size);

	} else {
		/* Received packet from standard input */
		struct {
			co_message_t message;
			co_linux_message_t linux;
			char data[overlapped->size];
		} message;
		
		message.message.from = CO_MODULE_SERIAL0 + daemon_parameters->index;
		message.message.to = CO_MODULE_LINUX;
		message.message.priority = CO_PRIORITY_DISCARDABLE;
		message.message.type = CO_MESSAGE_TYPE_OTHER;
		message.message.size = sizeof(message) - sizeof(message.message);
		message.linux.device = CO_DEVICE_SERIAL;
		message.linux.unit = daemon_parameters->index;
		message.linux.size = overlapped->size;
		memcpy(message.data, overlapped->buffer, overlapped->size);

		co_win32_overlapped_write_async(&daemon_overlapped, &message, sizeof(message));
	}

	return CO_RC(OK);
}

co_rc_t co_win32_overlapped_read_async(co_win32_overlapped_t *overlapped)
{
	BOOL result;
	DWORD error;

	while (TRUE) {
		result = ReadFile(overlapped->read_handle,
				  &overlapped->buffer,
				  sizeof(overlapped->buffer),
				  &overlapped->size,
				  &overlapped->read_overlapped);

		if (!result) { 
			error = GetLastError();
			switch (error)
			{ 
			case ERROR_IO_PENDING: 
				return CO_RC(OK);
			default:
				co_debug("Error: %x\n", error);
				return CO_RC(ERROR);
			}
		} else {
			co_win32_overlapped_read_received(overlapped);
		}
	}

	return CO_RC(OK);
}

co_rc_t co_win32_overlapped_read_completed(co_win32_overlapped_t *overlapped)
{
	BOOL result;

	result = GetOverlappedResult(
		overlapped->read_handle,
		&overlapped->read_overlapped,
		&overlapped->size,
		FALSE);

	if (result) {
		co_win32_overlapped_read_received(overlapped);
		co_win32_overlapped_read_async(overlapped);
	} else {
		if (GetLastError() == ERROR_BROKEN_PIPE) {
			co_debug("Pipe broken, exiting\n");
			return CO_RC(ERROR);
		}

		co_debug("GetOverlappedResult error %d\n", GetLastError());
	}
	 
	return CO_RC(OK);
}

void co_win32_overlapped_init(co_win32_overlapped_t *overlapped, HANDLE read_handle, HANDLE write_handle)
{
	overlapped->read_handle = read_handle;
	overlapped->write_handle = write_handle;
	overlapped->read_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	overlapped->write_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	overlapped->read_overlapped.Offset = 0; 
	overlapped->read_overlapped.OffsetHigh = 0; 
	overlapped->read_overlapped.hEvent = overlapped->read_event; 

	overlapped->write_overlapped.Offset = 0; 
	overlapped->write_overlapped.OffsetHigh = 0; 
	overlapped->write_overlapped.hEvent = overlapped->write_event; 
}

int wait_loop(HANDLE daemon_handle)
{
	HANDLE wait_list[2];
	ULONG status;
	co_rc_t rc;

	co_win32_overlapped_init(&daemon_overlapped, daemon_handle, daemon_handle);
	co_win32_overlapped_init(&stdio_overlapped, GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE));

	wait_list[0] = stdio_overlapped.read_event;
	wait_list[1] = daemon_overlapped.read_event;

	co_win32_overlapped_read_async(&stdio_overlapped);
	co_win32_overlapped_read_async(&daemon_overlapped); 

	while (1) {
		status = WaitForMultipleObjects(2, wait_list, FALSE, INFINITE); 
			
		switch (status) {
		case WAIT_OBJECT_0: {/* stdio */
			rc = co_win32_overlapped_read_completed(&stdio_overlapped);
			if (!CO_OK(rc))
				return 0;
			break;
		}
		case WAIT_OBJECT_0+1: {/* daemon */
			rc = co_win32_overlapped_read_completed(&daemon_overlapped);
			if (!CO_OK(rc))
				return 0;

			break;
		}
		default:
			break;
		}
	}

	return 0;
}

/********************************************************************************
 * parameters
 */

static void syntax(void)
{
	co_terminal_print("Cooperative Linux Virtual Serial Daemon\n");
	co_terminal_print("Dan Aloni, 2004 (c)\n");
	co_terminal_print("\n");
	co_terminal_print("Syntax:\n");
	co_terminal_print("     colinux-serial-daemon [-i colinux instance number] [-u unit]\n");
}

static co_rc_t 
handle_paramters(start_parameters_t *start_parameters, int argc, char *argv[])
{
	bool_t instance_specified, unit_specified;
	co_command_line_params_t cmdline;
	co_rc_t rc;

	/* Default settings */
	start_parameters->index = 0;
	start_parameters->instance = 0;
	start_parameters->show_help = PFALSE;

	rc = co_cmdline_params_alloc(&argv[1], argc-1, &cmdline);
	if (!CO_OK(rc)) {
		co_terminal_print("coserial: error parsing arguments\n");
		goto out_clean;
	}

	rc = co_cmdline_params_one_arugment_int_parameter(cmdline, "-i", 
							  &instance_specified, &start_parameters->instance);

	if (!CO_OK(rc)) {
		syntax();
		goto out;
	}

	rc = co_cmdline_params_one_arugment_int_parameter(cmdline, "-u", 
							  &unit_specified, &start_parameters->index);

	if (!CO_OK(rc)) {
		syntax();
		goto out;
	}

	rc = co_cmdline_params_check_for_no_unparsed_parameters(cmdline, PTRUE);
	if (!CO_OK(rc)) {
		syntax();
		goto out;
	}

	if ((start_parameters->index < 0) || (start_parameters->index >= CO_MODULE_MAX_SERIAL)) 
	{
		co_terminal_print("Invalid index: %d\n", start_parameters->index);
		return CO_RC(ERROR);
	}

	if (start_parameters->instance == -1) {
		co_terminal_print("coLinux instance not specificed\n");
		return CO_RC(ERROR);
	}

out:
	co_cmdline_params_free(cmdline);

out_clean:
	return rc;
}

static void terminal_print_hook_func(char *str)
{
	struct {
		co_message_t message;
		char data[strlen(str)+1];
	} message;
	
	message.message.from = CO_MODULE_SERIAL0 + daemon_parameters->index;
	message.message.to = CO_MODULE_CONSOLE;
	message.message.priority = CO_PRIORITY_DISCARDABLE;
	message.message.type = CO_MESSAGE_TYPE_STRING;
	message.message.size = strlen(str)+1;
	memcpy(message.data, str, strlen(str)+1);

	if (daemon_overlapped.write_handle != NULL)
		co_win32_overlapped_write_async(&daemon_overlapped, &message, sizeof(message));
}

int main(int argc, char *argv[])
{	
	co_rc_t rc;
	HANDLE daemon_handle = 0;
	int exit_code = 0;
	co_daemon_handle_t daemon_handle_;
	start_parameters_t start_parameters;

	co_debug_start();

	co_set_terminal_print_hook(terminal_print_hook_func);

	rc = handle_paramters(&start_parameters, argc, argv);
	if (!CO_OK(rc)) {
		exit_code = -1;
		goto out;
	}

	daemon_parameters = &start_parameters;

	rc = co_os_daemon_pipe_open(daemon_parameters->instance, 
				    CO_MODULE_SERIAL0 + daemon_parameters->index, &daemon_handle_);
	if (!CO_OK(rc)) {
		co_terminal_print("Error opening a pipe to the daemon\n");
		goto out;
	}

	daemon_handle = daemon_handle_->handle;
	exit_code = wait_loop(daemon_handle);
	co_os_daemon_pipe_close(daemon_handle_);

out:
	co_debug_end();
	return exit_code;
}
