/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#ifndef __COLINUX_DEBUG_H__
#define __COLINUX_DEBUG_H__

#ifdef COLINUX_DEBUG

/*-------------------------------- Debug Mode --------------------------------*/

#include <stdarg.h>

extern void co_debug_system(const char *fmt, ...);

extern char _colinux_module[0x30];

#define COLINUX_DEFINE_MODULE(str) \
char _colinux_module[] = str;

#define CO_DEBUG_LIST \
	X(misc,           15, 10) \
	X(network,        15, 10) \
	X(messages,       15, 10) \
	X(prints,         15, 10) \
	X(blockdev,       15, 10) \
	X(allocations,    15, 10) \

typedef struct {
#define X(facility, static_level, default_dynamic_level) int facility##_level;
	CO_DEBUG_LIST
#undef X
} co_debug_levels_t;

typedef enum {
#define X(facility, static_level, default_dynamic_level) CO_DEBUG_FACILITY_##facility,
	CO_DEBUG_LIST
#undef X
} co_debug_facility_t;

extern void co_debug_(const char *module, co_debug_facility_t facility, int level, 
		      const char *filename, int line, const char *func,
		      const char *fmt, ...);

extern co_debug_levels_t co_global_debug_levels;

#define X(facility, static_level, default_dynamic_level)	        \
static inline int co_debug_log_##facility##_level(int requested_level)  \
{									\
	if (requested_level > static_level)				\
		return 0;						\
	return (requested_level <= co_global_debug_levels.facility##_level);      \
}
CO_DEBUG_LIST;
#undef X

#define co_debug_lvl(facility, __level__, fmt, ...) do { \
        if (co_debug_log_##facility##_level(__level__))  \
		co_debug_(_colinux_module, CO_DEBUG_FACILITY_##facility, __level__, __FILE__, \
                          __LINE__, __FUNCTION__, fmt, ## __VA_ARGS__);	\
} while (0);

#else

/*------------------------------ Production Mode -----------------------------*/

#define co_debug_lvl(facility, level, fmt, ...)    do {} while(0)
#define co_debug_system(fmt, ...)                  do {} while(0)

/*----------------------------------------------------------------------------*/

#endif

typedef enum {
	CO_DEBUG_TYPE_TLV,
	CO_DEBUG_TYPE_TIMESTAMP,
	CO_DEBUG_TYPE_STRING,
	CO_DEBUG_TYPE_MODULE,
	CO_DEBUG_TYPE_LOCAL_INDEX,
	CO_DEBUG_TYPE_FACILITY,
	CO_DEBUG_TYPE_LEVEL,
	CO_DEBUG_TYPE_LINE,
	CO_DEBUG_TYPE_FUNC,
	CO_DEBUG_TYPE_FILE,
} co_debug_type_t;

typedef struct {
	unsigned long low;
	unsigned long high;
} co_debug_timestamp_t;

typedef struct {
	char type;
	unsigned char length;
	char value[];
} co_debug_tlv_t;

extern void co_debug_buf(const char *buf, long size);

#define co_debug_ulong(name)     co_debug("%s: 0x%x\n", #name, name)
#define co_debug(fmt, ...)       co_debug_lvl(misc, 10, fmt, ## __VA_ARGS__)

#ifndef COLINUX_TRACE
#undef CO_TRACE_STOP
#define CO_TRACE_STOP
#undef CO_TRACE_CONTINUE
#define CO_TRACE_CONTINUE
#endif

#endif
