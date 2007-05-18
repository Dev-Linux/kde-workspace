/* config.h.  Generated by cmake from config.h.cmake  */

/* media HAL backend compilation */
#undef COMPILE_HALBACKEND

/* Define if you have DPMS support */
#cmakedefine HAVE_DPMS 1

/* Define if you have the DPMSCapable prototype in <X11/extensions/dpms.h> */
#cmakedefine HAVE_DPMSCAPABLE_PROTO 1

/* Define if you have the DPMSInfo prototype in <X11/extensions/dpms.h> */
#cmakedefine HAVE_DPMSINFO_PROTO 1

/* Defines if your system has the libfontconfig library */
#cmakedefine HAVE_FONTCONFIG 1

/* Define if you have gethostname */
#cmakedefine HAVE_GETHOSTNAME 1

/* Define if you have the gethostname prototype */
#cmakedefine HAVE_GETHOSTNAME_PROTO 1

/* Define to 1 if you have the `getpeereid' function. */
#cmakedefine HAVE_GETPEEREID 1

/* Defines if you have Solaris' libkstat */
/* #undef HAVE_KSTAT */

/* Define if you have long long as datatype */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the `nice' function. */
#define HAVE_NICE 1

/* Define to 1 if you have the <sasl.h> header file. */
#cmakedefine HAVE_SASL_H 1

/* Define to 1 if you have the <sasl/sasl.h> header file. */
#cmakedefine HAVE_SASL_SASL_H 1

/* Define to 1 if you have the `setpriority' function. */
#cmakedefine HAVE_SETPRIORITY 1

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the `sigset' function. */
#define HAVE_SIGSET 1

/* Define to 1 if you have the <string.h> header file. */
#cmakedefine HAVE_STRING_H 1

/* Define if you have the struct ucred */
#cmakedefine HAVE_STRUCT_UCRED 1

/* Define to 1 if you have the <sys/loadavg.h> header file. */
#cmakedefine HAVE_SYS_LOADAVG_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#cmakedefine HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#cmakedefine HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#cmakedefine HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#cmakedefine HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H 1

/* Define if you have unsetenv */
#cmakedefine HAVE_UNSETENV 1

/* Define if you have the unsetenv prototype */
#cmakedefine HAVE_UNSETENV_PROTO 1

/* Define if you have usleep */
#cmakedefine HAVE_USLEEP 1

/* Define if you have the usleep prototype */
#cmakedefine HAVE_USLEEP_PROTO 1

/* Define to 1 if you have the `vsnprintf' function. */
#cmakedefine HAVE_VSNPRINTF 1

/* KDE's binaries directory */
#define KDE_BINDIR "${CMAKE_INSTALL_PREFIX}/bin"

/* KDE's configuration directory */
#define KDE_CONFDIR "${CMAKE_INSTALL_PREFIX}/share/config"

/* KDE's static data directory */
#define KDE_DATADIR "${CMAKE_INSTALL_PREFIX}/share/apps"

/* Define where your java executable is */
#undef PATH_JAVA

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#cmakedefine TIME_WITH_SYS_TIME 1

/* Define the file for utmp entries */
#define UTMP "${UTMP_FILE}"

/* X binaries directory */
#define XBINDIR "/usr/X11R6/bin"

/* X libraries directory */
#define XLIBDIR "/usr/X11R6/lib/X11"

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64


#if !defined(HAVE_GETHOSTNAME_PROTO)
#ifdef __cplusplus
extern "C" {
#endif
int gethostname (char *, unsigned int);
#ifdef __cplusplus
}
#endif
#endif



#if !defined(HAVE_UNSETENV_PROTO)
#ifdef __cplusplus
extern "C" {
#endif
void unsetenv (const char *);
#ifdef __cplusplus
}
#endif
#endif



#if !defined(HAVE_USLEEP_PROTO)
#ifdef __cplusplus
extern "C" {
#endif
int usleep (unsigned int);
#ifdef __cplusplus
}
#endif
#endif


/*
 * On HP-UX, the declaration of vsnprintf() is needed every time !
 */

#if !defined(HAVE_VSNPRINTF) || defined(hpux)
#if __STDC__
#include <stdarg.h>
#include <stdlib.h>
#else
#include <varargs.h>
#endif
#ifdef __cplusplus
extern "C"
#endif
int vsnprintf(char *str, size_t n, char const *fmt, va_list ap);
#ifdef __cplusplus
extern "C"
#endif
int snprintf(char *str, size_t n, char const *fmt, ...);
#endif

/* type to use in place of socklen_t if not defined */
#define kde_socklen_t socklen_t
