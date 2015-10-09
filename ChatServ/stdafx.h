// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>

//#ifdef NDEBUG
//#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )
//#endif

#ifndef WIN32_LEAN_AND_MEAN 
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _XKEYCHECK_H
#define _XKEYCHECK_H
#endif
//#include <windows.h>
//#include <windows.h>
//#include <time.h>
#include <evhttp.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include "Presence.h"
//#include "gettimewin.h"
