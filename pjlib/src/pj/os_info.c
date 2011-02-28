/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/string.h>

/*
 * FYI these links contain useful infos about predefined macros across
 * platforms:
 *  - http://predef.sourceforge.net/preos.html
 */

#if defined(PJ_HAS_SYS_UTSNAME_H) && PJ_HAS_SYS_UTSNAME_H != 0
/* For uname() */
#   include <sys/utsname.h>
#   include <stdlib.h>
#   define PJ_HAS_UNAME		1
#endif

#if defined(PJ_HAS_LIMITS_H) && PJ_HAS_LIMITS_H != 0
/* Include <limits.h> to get <features.h> to get various glibc macros.
 * See http://predef.sourceforge.net/prelib.html
 */
#   include <limits.h>
#endif

#if defined(_MSC_VER)
/* For all Windows including mobile */
#   include <windows.h>
#endif


#ifndef PJ_SYS_INFO_BUFFER_SIZE
#   define PJ_SYS_INFO_BUFFER_SIZE	64
#endif

static char *ver_info(pj_uint32_t ver, char *buf)
{
    int len;

    if (ver == 0) {
	*buf = '\0';
	return buf;
    }

    sprintf(buf, "-%u.%u",
	    (ver & 0xFF000000) >> 24,
	    (ver & 0x00FF0000) >> 16);
    len = strlen(buf);

    if (ver & 0xFFFF) {
	sprintf(buf+len, ".%u", (ver & 0xFF00) >> 8);
	len = strlen(buf);

	if (ver & 0x00FF) {
	    sprintf(buf+len, ".%u", (ver & 0xFF));
	}
    }

    return buf;
}

PJ_DEF(const pj_sys_info*) pj_get_sys_info(void)
{
    static char si_buffer[PJ_SYS_INFO_BUFFER_SIZE];
    static pj_sys_info si;
    static pj_bool_t si_initialized;
    unsigned left = PJ_SYS_INFO_BUFFER_SIZE, len;

    if (si_initialized)
	return &si;

#define ALLOC_CP_STR(str,field)	\
	do { \
	    len = pj_ansi_strlen(str); \
	    if (len && left >= len+1) { \
		si.field.ptr = si_buffer + PJ_SYS_INFO_BUFFER_SIZE - left; \
		si.field.slen = len; \
		pj_memcpy(si.field.ptr, str, len+1); \
		left -= (len+1); \
	    } \
	} while (0)

    /*
     * Machine and OS info.
     */
#if defined(PJ_HAS_UNAME) && PJ_HAS_UNAME
    {
	struct utsname u;
	char *tok;
	int i, maxtok;

	if (uname(&u) != 0)
	    goto get_sdk_info;
	ALLOC_CP_STR(u.machine, machine);
	ALLOC_CP_STR(u.sysname, os_name);

	maxtok = 4;
	for (tok = strtok(u.release, ".-"), i=0;
	     tok && i<maxtok;
	     ++i, tok=strtok(NULL, ".-"))
	{
	    int n;

	    if (!pj_isdigit(*tok))
		break;

	    n = atoi(tok);
	    si.os_ver |= (n << ((3-i)*8));
	}
    }
#elif defined(_MSC_VER)
    {
	OSVERSIONINFO ovi;

	ovi.dwOSVersionInfoSize = sizeof(ovi);

	if (GetVersionInfoEx(&ovi) == FALSE)
	    goto get_sdk_info;

	si.os_ver = (ovi.dwMajorVersion << 24) |
		    (ovi.dwMinorVersion << 16);
	#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
	    si.os_name = pj_str("wince");
	#else
	    si.os_name = pj_str("win32");
	#endif
    }

    {
	SYSTEM_INFO wsi;

	GetSystemInfo(&wsi);
	switch (wsi.wProcessorArchitecture) {
    #if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
	case PROCESSOR_ARCHITECTURE_ARM:
	    si.machine = pj_str("arm");
	    break;
	case PROCESSOR_ARCHITECTURE_SHX:
	    si.machine = pj_str("shx");
	    break;
    #else
	case PROCESSOR_ARCHITECTURE_AMD64:
	    si.machine = pj_str("x86_64");
	    break;
	case PROCESSOR_ARCHITECTURE_IA64:
	    si.machine = pj_str("ia64");
	    break;
	case PROCESSOR_ARCHITECTURE_INTEL:
	    si.machine = pj_str("i386");
	    break;
    #endif	/* PJ_WIN32_WINCE */
	}
    }
#endif

    /*
     * SDK info.
     */
get_sdk_info:

#if defined(__GLIBC__)
    si.sdk_ver = (__GLIBC__ << 24) |
		 (__GLIBC_MINOR__ << 16);
    si.sdk_name = pj_str("glibc");
#elif defined(__GNU_LIBRARY__)
    si.sdk_ver = (__GNU_LIBRARY__ << 24) |
	         (__GNU_LIBRARY_MINOR__ << 16);
    si.sdk_name = pj_str("libc");
#elif defined(__UCLIBC__)
    si.sdk_ver = (__UCLIBC_MAJOR__ << 24) |
    	         (__UCLIBC_MINOR__ << 16);
    si.sdk_name = pj_str("uclibc");
#elif defined(_WIN32_WCE) && _WIN32_WCE
    /* Old window mobile declares _WIN32_WCE as decimal (e.g. 300, 420, etc.),
     * but then it was changed to use hex, e.g. 0x420, etc. See
     * http://social.msdn.microsoft.com/forums/en-US/vssmartdevicesnative/thread/8a97c59f-5a1c-4bc6-99e6-427f065ff439/
     */
    #if _WIN32_WCE <= 500
	si.sdk_ver = ( (_WIN32_WCE / 100) << 24) |
		     ( ((_WIN32_WCE % 100) / 10) << 16) |
		     ( (_WIN32_WCE % 10) << 8);
    #else
	si.sdk_ver = ( ((_WIN32_WCE & 0xFF00) >> 8) << 24) |
		     ( ((_WIN32_WCE & 0x00F0) >> 4) << 16) |
		     ( ((_WIN32_WCE & 0x000F) >> 0) << 8);
    #endif
    si.sdk_name = pj_str("cesdk");
#elif defined(_MSC_VER)
    /* No SDK info is easily obtainable for Visual C, so lets just use
     * _MSC_VER. The _MSC_VER macro reports the major and minor versions
     * of the compiler. For example, 1310 for Microsoft Visual C++ .NET 2003.
     * 1310 represents version 13 and a 1.0 point release.
     * The Visual C++ 2005 compiler version is 1400.
     */
    si.sdk_ver = ((_MSC_VER / 100) << 24) |
    	         (((_MSC_VER % 100) / 10) << 16) |
    	         ((_MSC_VER % 10) << 8);
    si.sdk_name = pj_str("msvc");
#endif

    /*
     * Build the info string.
     */
    {
	char tmp[PJ_SYS_INFO_BUFFER_SIZE];
	char os_ver[20], sdk_ver[20];
	int len;

	len = pj_ansi_snprintf(tmp, sizeof(tmp),
			       "%s%s/%s/%s%s",
			       si.os_name.ptr,
			       ver_info(si.os_ver, os_ver),
			       si.machine.ptr,
			       si.sdk_name.ptr,
			       ver_info(si.sdk_ver, sdk_ver));
	if (len > 0 && len < sizeof(tmp)) {
	    ALLOC_CP_STR(tmp, info);
	}
    }

    si_initialized = PJ_TRUE;
    return &si;
}