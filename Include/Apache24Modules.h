// *
// * Apache24Modules.h :: Common Header for developing Apache 24 Module with C++
// * (C)2015-2018 Dark Network Systems
// * http://n.dark-x.net/
// *

// Includes & Macros for modules

#ifndef X_APACHE_24_MODULES_H
#define X_APACHE_24_MODULES_H

	#include "apr_strings.h"
	#include "ap_config.h"
	#include "httpd.h"
	#include "http_config.h"
	#include "http_protocol.h"
	#include "http_log.h"
	#include "util_script.h"
	#include "http_main.h"
	#include "http_request.h"
	#include "mod_core.h"

	#if defined(__cplusplus) && !defined(linux)
		extern int * const aplog_module_index; //error C2065: 'aplog_module_index' : undeclared identifier
	#endif
	#define Log(r, type, module, str, ...) ap_log_rerror(APLOG_MARK, type, 0, r, module ": " str, __VA_ARGS__)

#endif
