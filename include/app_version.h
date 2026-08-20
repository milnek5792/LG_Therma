#ifndef APP_VERSION_H
#define APP_VERSION_H

/* Generuje scripts/gen_build_stamp.py při každém buildu (YYMMDD-HHMM). */
#if defined(__has_include)
#if __has_include("app_build_stamp.h")
#include "app_build_stamp.h"
#endif
#endif

#ifndef APP_FW_VERSION
#define APP_FW_VERSION "dev"
#endif

#endif
