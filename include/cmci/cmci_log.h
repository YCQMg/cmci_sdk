#ifndef CMCI_LOG_H
#define CMCI_LOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Log level
 * ================================================================ */
typedef enum {
	CMCI_LOG_ERROR = 0,
	CMCI_LOG_WARN  = 1,
	CMCI_LOG_INFO  = 2,
	CMCI_LOG_DEBUG = 3,
	CMCI_LOG_TRACE = 4,
	CMCI_LOG_NONE = 5,
} cmci_log_level_t;

/* ================================================================
 *  CAT (module category)
 * ================================================================ */
typedef enum {
	CMCI_LOG_CAT_API        = 0,
	CMCI_LOG_CAT_CORE,
	CMCI_LOG_CAT_EXTENSION,
	CMCI_LOG_CAT_PLATFORM,
	CMCI_LOG_CAT_PROTO,
	CMCI_LOG_CAT_TRANSPORT,
	CMCI_LOG_CAT_COUNT,
} cmci_log_cat_t;

/* ================================================================
 *  subCat per CAT
 * ================================================================ */
/* Max subCat entries across all CATs (must be >= any *_COUNT) */
#define CMCI_LOG_SUBCAT_MAX  8

/* API */
typedef enum {
	CMCI_LOG_SUBCAT_API_API = 0,
	CMCI_LOG_SUBCAT_API_COUNT,
} cmci_log_subcat_api_t;

/* CORE */
typedef enum {
	CMCI_LOG_SUBCAT_CORE_CHANNEL  = 0,
	CMCI_LOG_SUBCAT_CORE_DISPATCH,
	CMCI_LOG_SUBCAT_CORE_CONTEXT,
	CMCI_LOG_SUBCAT_CORE_TIMER,
	CMCI_LOG_SUBCAT_CORE_COUNT,
} cmci_log_subcat_core_t;

/* EXTENSION */
typedef enum {
	CMCI_LOG_SUBCAT_EXTENSION_PTY = 0,
	CMCI_LOG_SUBCAT_EXTENSION_CMD,
	CMCI_LOG_SUBCAT_EXTENSION_COUNT,
} cmci_log_subcat_extension_t;

/* PLATFORM */
typedef enum {
	CMCI_LOG_SUBCAT_PLATFORM_WIN   = 0,
	CMCI_LOG_SUBCAT_PLATFORM_LINUX,
	CMCI_LOG_SUBCAT_PLATFORM_COUNT,
} cmci_log_subcat_platform_t;

/* PROTO */
typedef enum {
	CMCI_LOG_SUBCAT_PROTO_ACK        = 0,
	CMCI_LOG_SUBCAT_PROTO_FRAGM,
	CMCI_LOG_SUBCAT_PROTO_FRAME,
	CMCI_LOG_SUBCAT_PROTO_HEART,
	CMCI_LOG_SUBCAT_PROTO_REASSEMBLY,
	CMCI_LOG_SUBCAT_PROTO_COUNT,
} cmci_log_subcat_proto_t;

/* TRANSPORT */
typedef enum {
	CMCI_LOG_SUBCAT_TRANSPORT_LINK = 0,
	CMCI_LOG_SUBCAT_TRANSPORT_UART,
	CMCI_LOG_SUBCAT_TRANSPORT_UDP,
	CMCI_LOG_SUBCAT_TRANSPORT_COUNT,
} cmci_log_subcat_transport_t;

/* ================================================================
 *  Level-filter check (inline — zero overhead when level is low)
 * ================================================================ */
extern uint8_t cmci_g_log_level[CMCI_LOG_CAT_COUNT][CMCI_LOG_SUBCAT_MAX];

static inline int cmci_log_should_log(cmci_log_cat_t cat, int subcat, cmci_log_level_t level)
{
	if ((int)cat < 0 || cat >= CMCI_LOG_CAT_COUNT ||
	    subcat < 0 || subcat >= CMCI_LOG_SUBCAT_MAX)
		return 0;
	return cmci_g_log_level[cat][subcat] >= (uint8_t)level;
}
#undef cmci_log_should_log /* keep no trailing macro */

/* ================================================================
 *  Macros — CMCI_LOG / CMCI_LOGC
 *
 *  Both macros take explicit (cat, subcat) parameters.
 *  No per-file #define CMCI_LOG_CAT / CMCI_LOG_SUBCAT needed.
 *
 *  CMCI_LOG(level, cat, subcat, fmt, ...)   — normal log
 *  CMCI_LOGC(cat, subcat, level, fmt, ...)  — alias with same signature
 * ================================================================ */

#define CMCI_LOG(level, cat, subcat, fmt, ...) \
	do { \
		if (cmci_log_should_log((cat), (subcat), (level))) \
			_cmci_log((level), (cat), (subcat), \
			          "%d %s() " fmt, __LINE__, __func__, ##__VA_ARGS__); \
	} while (0)

/* Cross-module override variant — identical signature */
#define CMCI_LOGC(cat, subcat, level, fmt, ...) \
	do { \
		if (cmci_log_should_log((cat), (subcat), (level))) \
			_cmci_log((level), (cat), (subcat), \
			          "%d %s() " fmt, __LINE__, __func__, ##__VA_ARGS__); \
	} while (0)

/* ================================================================
 *  Backend function
 * ================================================================ */
void _cmci_log(
	cmci_log_level_t level,
	cmci_log_cat_t   cat,
	int              subcat,
	const char      *fmt,
	...
);

/* ================================================================
 *  Setter API — runtime log-level control
 * ================================================================ */
void cmci_log_set_all(cmci_log_level_t level);
void cmci_log_set_cat_level(cmci_log_cat_t cat, cmci_log_level_t level);
void cmci_log_set_subcat_level(cmci_log_cat_t cat, int subcat,
                               cmci_log_level_t level);
cmci_log_level_t cmci_log_get_level(cmci_log_cat_t cat, int subcat);
void cmci_log_reset_levels(void);

#ifdef __cplusplus
}
#endif

#endif /* CMCI_LOG_H */
