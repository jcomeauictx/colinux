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
#include "misc.h"

/*
 * Currently there's a bug on SMP that prevents coLinux from
 * running stable unless it is pinned to the first processor.
 */
void co_winnt_affinity_workaround(void)
{
	char *env;
	HANDLE hProcess = GetCurrentProcess();

	// Prevent coLinux from using CPU0
	env = getenv("COLINUX_NO_CPU0_WORKAROUND");
	if (env && strcmp(env, "Y") == 0) {
		DWORD ProcessMask, SystemMask, NewMask;

		if (GetProcessAffinityMask (hProcess, &ProcessMask, &SystemMask)) {
			NewMask = SystemMask & ~1;
			co_debug("AffinityMasks Process %lx, System %lx, New %lx", ProcessMask, SystemMask, NewMask);
			if (NewMask != ProcessMask) {
				if (!SetProcessAffinityMask(hProcess, NewMask)) {
					co_debug("SetProcessAffinityMask failed with 0x%lx", GetLastError());
				}
			}
		} else
			co_debug("GetProcessAffinityMask failed with 0x%lx", GetLastError());

		return;
	}

	env = getenv("COLINUX_NO_SMP_WORKAROUND");
	if (env && strcmp(env, "Y") == 0)
		return;

	SetProcessAffinityMask(hProcess, 1);
}

