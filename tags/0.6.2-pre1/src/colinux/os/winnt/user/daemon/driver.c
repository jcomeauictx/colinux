/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * Service support by Jaroslaw Kowalski <jaak@zd.com.pl>, 2004 (c)
 * Driver service separation by Daniel R. Slater <dan_slater@yahoo.com>, 2004 (c)
 * Driver installation fixes by George Boutwell <gboutwel@praize.com>, 2004 (c)
 * 
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include <windows.h>
#include <shlwapi.h>

#include <colinux/common/common.h>
#include <colinux/common/ioctl.h>
#include <colinux/user/daemon.h>
#include <colinux/user/manager.h>
#include <colinux/user/debug.h>
#include <colinux/os/user/misc.h>
#include <colinux/os/winnt/kernel/driver.h>
#include <colinux/os/winnt/user/misc.h>
#include <colinux/os/current/os.h>

#include "main.h"
#include "cmdline.h"
#include "misc.h"
#include "driver.h"
#include "service.h"

co_rc_t co_win32_manager_is_installed(bool_t *installed)
{
	co_rc_t rc;

	rc = co_os_manager_is_installed(installed);
	if (!CO_OK(rc)) {
		if (CO_RC_GET_CODE(rc) == CO_RC_ACCESS_DENIED)
			co_terminal_print("access defined, not enough privileges\n");
		else
			co_terminal_print("error, unable to determine if driver is installed (rc %x)\n", rc);
	}

	return rc;
}

/*
 * This function makes sure the driver is installed and initializes it.
 */
co_rc_t co_winnt_install_driver(void) 
{
	co_rc_t rc = CO_RC_OK;
	bool_t installed = PFALSE;
	co_manager_handle_t handle;

	rc = co_win32_manager_is_installed(&installed);
	if (!CO_OK(rc)) {
		return rc;
	}

	if (installed) {
		co_terminal_print("driver already installed\n");
		return CO_RC(OK);
	}
	
	rc = co_winnt_driver_install_lowlevel();
	if (!CO_OK(rc)) {
		co_terminal_print("cannot install\n");
		return CO_RC(ERROR);
	}
	
	handle = co_os_manager_open();
	if (handle == NULL) {
		co_terminal_print("error opening kernel driver\n");
		return CO_RC(ERROR);
	}
	
	return CO_RC(OK);
}

co_rc_t co_winnt_remove_driver(void) 
{
	co_rc_t rc = CO_RC_OK;
	bool_t installed = PFALSE;
	co_manager_handle_t handle;
	co_manager_ioctl_status_t status = {0, };

	rc = co_win32_manager_is_installed(&installed);
	if (!CO_OK(rc))
		return rc;

	if (!installed) {
		co_terminal_print("driver not installed\n");
		return CO_RC(OK);
	}

	handle = co_os_manager_open();
	if (!handle) {
		co_terminal_print("couldn't get driver handle, removing anyway\n");
		co_winnt_driver_remove_lowlevel();
		return CO_RC(ERROR);
	}		
	rc = co_manager_status(handle, &status);
	if (!CO_OK(rc)) {
		if (CO_RC_GET_CODE(rc) == CO_RC_VERSION_MISMATCHED) {
			co_ntevent_print("driver version is %d while expected version %d\n", 
					 status.periphery_api_version, CO_LINUX_PERIPHERY_API_VERSION);
		} else {
			co_ntevent_print("detected a old buggy driver version\n");
		}

		co_os_manager_close(handle);
		co_winnt_driver_remove_lowlevel();
		return CO_RC(ERROR);
	}		

	if (status.monitors_count != 0) {
		co_terminal_print("monitors are running, cannot remove driver\n");
		co_os_manager_close(handle);
		return CO_RC(ERROR);
	}

	co_os_manager_close(handle);
	co_winnt_driver_remove_lowlevel();
	return CO_RC(OK);
}	

void co_winnt_status_driver(void) 
{
	co_rc_t rc = CO_RC_OK;
	bool_t installed = PFALSE;
	co_manager_handle_t handle;
	co_manager_ioctl_status_t status = {0, };

	co_terminal_print("checking if the driver is installed\n");

	rc = co_win32_manager_is_installed(&installed);
	if (!CO_OK(rc))
		return;

	if (!installed) {
		co_terminal_print("driver not installed\n");
		return;
	}

	handle = co_os_manager_open();
	if (!handle) {
		co_terminal_print("couldn't get driver handle\n");
		return;
	}		
	
	rc = co_manager_status(handle, &status);
	if (!CO_OK(rc)) {
		co_terminal_print("couldn't get driver status (rc %x)\n", rc);
		co_os_manager_close(handle);
		return;
	}		

	co_terminal_print("current number of monitors: %x\n", status.monitors_count);
	co_os_manager_close(handle);
}

co_rc_t co_winnt_install_driver_lowlevel(IN SC_HANDLE SchSCManager, IN LPCTSTR  DriverName, IN LPCTSTR ServiceExe)
{ 
	SC_HANDLE  schService; 
 
	schService = CreateService(SchSCManager,         
				   DriverName,           
				   DriverName,          
				   SERVICE_ALL_ACCESS,
				   SERVICE_KERNEL_DRIVER, 
				   SERVICE_AUTO_START,
				   SERVICE_ERROR_NORMAL, 
				   ServiceExe,           
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL); 
 
	if (schService == NULL) {
		co_terminal_print_last_error("daemon");
		return CO_RC(ERROR_INSTALLING_DRIVER); 
	}

	/* Possible error: ERROR_SERVICE_EXISTS */
 
	CloseServiceHandle(schService);

	return CO_RC(OK); 
} 
 
co_rc_t co_winnt_remove_driver_lowlevel(IN SC_HANDLE  SchSCManager, IN LPCTSTR DriverName) 
{ 
	SC_HANDLE  schService; 
	co_rc_t   rc;

	schService = OpenService (SchSCManager, 
				  DriverName, 
				  SERVICE_ALL_ACCESS); 
 
	if (schService == NULL)
		return CO_RC(ERROR); 
 
	if (DeleteService(schService))
		rc = CO_RC(OK);
	else
		rc = CO_RC(ERROR_REMOVING_DRIVER);
 
	CloseServiceHandle(schService); 

	return rc; 
} 
 
co_rc_t co_winnt_start_driver_lowlevel(IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName) 
{ 
	SC_HANDLE  schService; 
	co_rc_t   ret;
	DWORD      err; 
    
	schService = OpenService(SchSCManager, DriverName, SERVICE_ALL_ACCESS); 
 
	if (schService == NULL) { 
		return CO_RC(ERROR_ACCESSING_DRIVER); 
	} 
 
	if (StartService(schService, 0, NULL))
		ret = CO_RC(OK);
	else
		ret = CO_RC(ERROR_STARTING_DRIVER);

	if (!ret) {
		err = GetLastError(); 

#if (0) 
		if (err == ERROR_SERVICE_ALREADY_RUNNING) 
			co_winnt_debug("failure: StartService, ERROR_SERVICE_ALREADY_RUNNING\n"); 
		else 
			co_winnt_debug("failure: StartService (0x%02x)\n", err);
#endif
	} 
 
	CloseServiceHandle(schService); 

	return ret; 
} 
 
co_rc_t co_winnt_stop_driver_lowlevel(IN SC_HANDLE  SchSCManager, IN LPCTSTR DriverName)
{ 
	SC_HANDLE       schService; 
	SERVICE_STATUS  serviceStatus; 
	co_rc_t        rc;

	schService = OpenService(SchSCManager, DriverName, SERVICE_ALL_ACCESS); 
 
	if (schService == NULL)
		return CO_RC(ERROR_ACCESSING_DRIVER); 

	if (ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus))
		rc = CO_RC(OK);
	else {
		rc = CO_RC(ERROR_STOPPING_DRIVER);
	}

	CloseServiceHandle(schService); 

	return rc; 
} 

co_rc_t co_winnt_unload_driver_lowlevel_by_name(char *name) 
{ 
	SC_HANDLE   schSCManager; 
	co_rc_t rc;

	co_debug_end();

	schSCManager = OpenSCManager (NULL,                 // machine (NULL == local) 
				      NULL,                 // database (NULL == default) 
				      SC_MANAGER_ALL_ACCESS // access required 
		); 
 
	co_debug("stopping driver service\n");
	rc = co_winnt_stop_driver_lowlevel(schSCManager, name);  

	co_debug("removing driver service\n");
	rc = co_winnt_remove_driver_lowlevel(schSCManager, name);
	
	CloseServiceHandle(schSCManager); 

	/*
	 * Apparently this givens the service manager an opportunity to
	 * remove the service before we reinstall it.
	 */
	Sleep(100);
			
	return rc;
} 

co_rc_t co_winnt_load_driver_lowlevel_by_name(char *name, char *path) 
{ 
	SC_HANDLE   schSCManager; 
	char fullpath[0x100] = {0,};
	char driverfullpath[0x100] = {0,};
	co_rc_t rc;

	GetModuleFileName(co_current_win32_instance, fullpath, sizeof(fullpath));
	PathRemoveFileSpec(fullpath);
	PathCombine(driverfullpath, fullpath, path);

	co_terminal_print("loading %s\n", driverfullpath);

	schSCManager = OpenSCManager(NULL,                 // machine (NULL == local) 
				     NULL,                 // database (NULL == default) 
				     SC_MANAGER_ALL_ACCESS /* access required */ );

	rc = co_winnt_install_driver_lowlevel(schSCManager, name, driverfullpath);
	if (!CO_OK(rc)) {
		CloseServiceHandle(schSCManager);    
		return rc;
	}

	rc = co_winnt_start_driver_lowlevel(schSCManager, name); 
	if (!CO_OK(rc)) {
		co_winnt_remove_driver_lowlevel(schSCManager, name); 
		CloseServiceHandle(schSCManager);    
		return rc;
	}

#if (0)
	rc = co_os_check_device(name); 
	if (!CO_OK(rc)) {
		co_winnt_stop_driver_lowlevel(schSCManager, name);  
		co_winnt_remove_driver_lowlevel(schSCManager, name); 
		CloseServiceHandle(schSCManager);    
		return rc;
	}
#endif

	CloseServiceHandle(schSCManager);    

	return CO_RC(OK);
} 

co_rc_t co_winnt_driver_remove_lowlevel(void)
{
	return co_winnt_unload_driver_lowlevel_by_name(CO_DRIVER_NAME);
}

co_rc_t co_winnt_driver_install_lowlevel(void)
{
	return co_winnt_load_driver_lowlevel_by_name(CO_DRIVER_NAME, COLINUX_DRIVER_FILE);
}
