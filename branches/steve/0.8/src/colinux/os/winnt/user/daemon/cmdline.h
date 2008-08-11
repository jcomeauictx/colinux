/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#ifndef __CO_OS_USER_WINNT_DAEMON_CMDLINE_H__
#define __CO_OS_USER_WINNT_DAEMON_CMDLINE_H__

#include <colinux/common/common.h>
#include <colinux/user/cmdline.h>

typedef struct co_winnt_parameters {
	bool_t install_service;
	bool_t remove_service;
	bool_t run_service;
	bool_t install_driver;
	bool_t status_driver;
	bool_t show_status;
	bool_t remove_driver;
	char service_name[128];
} co_winnt_parameters_t;

extern void co_winnt_daemon_syntax(void);
extern co_rc_t co_winnt_daemon_parse_args(co_command_line_params_t cmdline, co_winnt_parameters_t *winnt_parameters);

#endif
