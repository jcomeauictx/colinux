/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2004 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#include <windows.h>

#include <colinux/common/common.h>
#include <colinux/user/daemon.h>
#include <colinux/os/user/misc.h>

#include "cmdline.h"

void co_winnt_daemon_syntax(void)
{
	co_daemon_print_header();
	co_terminal_print("\n");
	co_terminal_print("The following options are specific to Windows NT/XP/2000:\n");
	co_terminal_print("\n");
	co_terminal_print("      --install-service [name]     Install colinux-daemon.exe as an NT service\n");
	co_terminal_print("                                   (default service name: Cooperative Linux)\n");
	co_terminal_print("      --remove-service [name]      Remove colinux service\n");
	co_terminal_print("                                   (default service name: Cooperative Linux)\n");
	co_terminal_print("      --install-driver             Install the colinux-driver (linux.sys)\n");
	co_terminal_print("      --remove-driver              Uninstall (remove) the colinux-driver (linux.sys)\n");
	co_terminal_print("      --status-driver              Show status about the installed/running\n");
	co_terminal_print("                                   driver\n");
}

co_rc_t co_winnt_daemon_parse_args(co_command_line_params_t cmdline, co_winnt_parameters_t *winnt_parameters)
{
	co_rc_t rc;

	/* Default settings */
	co_snprintf(winnt_parameters->service_name, sizeof(winnt_parameters->service_name), "Cooperative Linux");

	winnt_parameters->install_service = PFALSE;
	winnt_parameters->run_service = PFALSE;
	winnt_parameters->remove_service = PFALSE;
	winnt_parameters->install_driver = PFALSE;
	winnt_parameters->remove_driver = PFALSE;
	winnt_parameters->status_driver = PFALSE;

	rc = co_cmdline_params_one_optional_arugment_parameter(
		cmdline, "--install-service", 
		&winnt_parameters->install_service,
		winnt_parameters->service_name,
		sizeof(winnt_parameters->service_name));

	if (!CO_OK(rc)) 
		return rc;

	rc = co_cmdline_params_one_optional_arugment_parameter(
		cmdline, "--remove-service", 
		&winnt_parameters->remove_service,
		winnt_parameters->service_name,
		sizeof(winnt_parameters->service_name));

	if (!CO_OK(rc)) 
		return rc;

	rc = co_cmdline_params_one_optional_arugment_parameter(
		cmdline, "--run-service", 
		&winnt_parameters->run_service,
		winnt_parameters->service_name,  /* service name is obsolate at running time */
		sizeof(winnt_parameters->service_name));

	if (!CO_OK(rc)) 
		return rc;

	rc = co_cmdline_params_argumentless_parameter(
		cmdline,
		"--install-driver", 
		&winnt_parameters->install_driver);

	if (!CO_OK(rc)) 
		return rc;

	rc = co_cmdline_params_argumentless_parameter(
		cmdline,
		"--remove-driver", 
		&winnt_parameters->remove_driver);

	if (!CO_OK(rc)) 
		return rc;

	rc = co_cmdline_params_argumentless_parameter(
		cmdline,
		"--status-driver", 
		&winnt_parameters->status_driver);

	if (!CO_OK(rc)) 
		return rc;

	return CO_RC(OK);
}
