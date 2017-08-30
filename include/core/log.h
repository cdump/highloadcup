#pragma once

// #include "fiber.h"

#define		lcc_escape(x)	"\033[01;" #x "m"
#define		lcc_GRAY		lcc_escape(30)
#define		lcc_RED			lcc_escape(31)
#define		lcc_GREEN		lcc_escape(32)
#define		lcc_YELLOW		lcc_escape(33)
#define		lcc_BLUE		lcc_escape(34)
#define		lcc_PURPLE		lcc_escape(35)
#define		lcc_CYAN		lcc_escape(36)
#define		lcc_WHITE		lcc_escape(37)
#define		lcc_NORMAL		"\033[00m"

extern int _log_level;

enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_NOTICE,
    LOG_WARNING,
    LOG_ERR,
    LOG_ALERT,
};

	// if (_level > _log_level)
	// 	break;
#include <stdio.h>
#include <inttypes.h>

#define _log(_color, _level, _level_str, _msg, ...) do {                                                                      \
    fprintf(stderr, "%f " _color _level_str " [%d:%"PRIu64":%s]" lcc_NORMAL " "__FILE__":%d " _color _msg lcc_NORMAL "\n", \
		ev_now(gthread_loop),                                                                                                 \
        gthread_id,                                                                                                           \
        fiber_current()->id, \
        fiber_current()->name,                                                                                                                 \
        __LINE__,                                                                                                             \
        ##__VA_ARGS__);                                                                                                       \
} while(0);


// #define log_t(msg, ...) _log(lcc_NORMAL, LOG_DEBUG,   "TRACE", msg, ##__VA_ARGS__)
#define log_t(msg, ...)
#define log_d(msg, ...) _log(lcc_WHITE,  LOG_DEBUG,   "DEBUG", msg, ##__VA_ARGS__)
#define log_i(msg, ...) _log(lcc_GREEN,  LOG_INFO,    "INFO ", msg, ##__VA_ARGS__)
#define log_n(msg, ...) _log(lcc_CYAN,   LOG_NOTICE,  "NOTIC", msg, ##__VA_ARGS__)
#define log_w(msg, ...) _log(lcc_YELLOW, LOG_WARNING, "WARN ", msg, ##__VA_ARGS__)
#define log_e(msg, ...) _log(lcc_RED,    LOG_ERR,     "ERROR", msg, ##__VA_ARGS__)
#define log_a(msg, ...) _log(lcc_PURPLE, LOG_ALERT,   "ALERT", msg, ##__VA_ARGS__)

// #ifdef NOLOGS
#undef _log
#define _log(...)
// #endif
