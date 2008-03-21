// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <config.h>

#define MALLOC_ERR() {error("malloc error at %s:%i", __FILE__, __LINE__); goto malloc_error;} 

#define fatal(fmt, ...) _fatal(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define error(fmt, ...) _error(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define info(fmt, ...)  _info (__FILE__, __LINE__, fmt, ## __VA_ARGS__)

#ifdef _DEBUG
#  define debug(fmt, ...) _debug(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#  define trace(fmt, ...)
#else
#  define debug(fmt, ...)
#  define trace(fmt, ...)
#endif

extern void initialize_logging(const char *logfile);
extern void _fatal (const char *file, int line, const char *fmt, ...);
extern void _error (const char *file, int line, const char *fmt, ...);
extern void _info  (const char *file, int line, const char *fmt, ...);
extern void _debug (const char *file, int line, const char *fmt, ...);
extern void _trace (const char *file, int line, const char *fmt, ...);

#endif
