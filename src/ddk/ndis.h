#ifndef __COLINUX_COMPAT_DDK_NDIS_H__
#define __COLINUX_COMPAT_DDK_NDIS_H__

#if defined(__has_include)
#if __has_include(<ddk/ndis.h>)
#include_next <ddk/ndis.h>
#else
#include "ndis_fixed.h"
#endif
#else
#include_next <ddk/ndis.h>
#endif

#endif
