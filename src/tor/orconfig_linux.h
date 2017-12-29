/* orconfig.h.  Generated from orconfig.h.in by configure.  */
/* orconfig.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* tor's configuration directory */
#define CONFDIR "/usr/local/etc/tor"

/* Defined if we have a curve25519 implementation */
#define CURVE25519_ENABLED 1

/* Enable dmalloc's malloc function check */
/* #undef DMALLOC_FUNC_CHECK */

/* Define to 1 iff memset(0) sets doubles to 0.0 */
#define DOUBLE_0_REP_IS_ZERO_BYTES 1

/* Defined if we try to use freelists for buffer RAM chunks */
#define ENABLE_BUF_FREELISTS 1

/* Defined if we default to host local appdata paths on Windows */
/* #undef ENABLE_LOCAL_APPDATA */

/* Defined if we will try to use multithreading */
#define ENABLE_THREADS 1

/* Define if enum is always signed */
/* #undef ENUM_VALS_ARE_SIGNED */

/* Define to nothing if C supports flexible array members, and to 1 if it does
   not. That way, with a declaration like `struct s { int n; double
   d[FLEXIBLE_ARRAY_MEMBER]; };', the struct hack can be used with pre-C99
   compilers. When computing the size of such an object, don't use 'sizeof
   (struct s)' as it overestimates the size. Use 'offsetof (struct s, d)'
   instead. Don't use 'offsetof (struct s, d[0])', as this doesn't work with
   MSVC and with C++ compilers. */
#define FLEXIBLE_ARRAY_MEMBER /**/

/* Define to 1 if you have the `accept4' function. */
#define HAVE_ACCEPT4 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the `backtrace' function. */
#define HAVE_BACKTRACE 1

/* Define to 1 if you have the `backtrace_symbols_fd' function. */
#define HAVE_BACKTRACE_SYMBOLS_FD 1

/* Defined if the requested minimum BOOST version is satisfied */
#define HAVE_BOOST 1

/* Define to 1 if you have <boost/filesystem/path.hpp> */
#define HAVE_BOOST_FILESYSTEM_PATH_HPP 1

/* Define to 1 if you have <boost/system/error_code.hpp> */
#define HAVE_BOOST_SYSTEM_ERROR_CODE_HPP 1

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the <crt_externs.h> header file. */
/* #undef HAVE_CRT_EXTERNS_H */

/* Define to 1 if you have the <crypto_scalarmult_curve25519.h> header file.
   */
/* #undef HAVE_CRYPTO_SCALARMULT_CURVE25519_H */

/* Define to 1 if you have the <cygwin/signal.h> header file. */
/* #undef HAVE_CYGWIN_SIGNAL_H */

/* Define to 1 if you have the declaration of `mlockall', and to 0 if you
   don't. */
#define HAVE_DECL_MLOCKALL 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <dmalloc.h> header file. */
/* #undef HAVE_DMALLOC_H */

/* Define to 1 if you have the `dmalloc_strdup' function. */
/* #undef HAVE_DMALLOC_STRDUP */

/* Define to 1 if you have the `dmalloc_strndup' function. */
/* #undef HAVE_DMALLOC_STRNDUP */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `evdns_set_outgoing_bind_address' function. */
/* #undef HAVE_EVDNS_SET_OUTGOING_BIND_ADDRESS */

/* Define to 1 if you have the <event2/bufferevent_ssl.h> header file. */
#define HAVE_EVENT2_BUFFEREVENT_SSL_H 1

/* Define to 1 if you have the <event2/dns.h> header file. */
#define HAVE_EVENT2_DNS_H 1

/* Define to 1 if you have the <event2/event.h> header file. */
#define HAVE_EVENT2_EVENT_H 1

/* Define to 1 if you have the `event_base_loopexit' function. */
#define HAVE_EVENT_BASE_LOOPEXIT 1

/* Define to 1 if you have the `event_get_method' function. */
#define HAVE_EVENT_GET_METHOD 1

/* Define to 1 if you have the `event_get_version' function. */
#define HAVE_EVENT_GET_VERSION 1

/* Define to 1 if you have the `event_get_version_number' function. */
#define HAVE_EVENT_GET_VERSION_NUMBER 1

/* Define to 1 if you have the `event_set_log_callback' function. */
#define HAVE_EVENT_SET_LOG_CALLBACK 1

/* Define to 1 if you have the `evutil_secure_rng_set_urandom_device_file'
   function. */
/* #undef HAVE_EVUTIL_SECURE_RNG_SET_URANDOM_DEVICE_FILE */

/* Define to 1 if you have the <execinfo.h> header file. */
#define HAVE_EXECINFO_H 1

/* Defined if we have extern char **environ already declared */
#define HAVE_EXTERN_ENVIRON_DECLARED 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `flock' function. */
#define HAVE_FLOCK 1

/* Define to 1 if you have the `ftime' function. */
#define HAVE_FTIME 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define this if you have any gethostbyname_r() */
#define HAVE_GETHOSTBYNAME_R 1

/* Define this if gethostbyname_r takes 3 arguments */
/* #undef HAVE_GETHOSTBYNAME_R_3_ARG */

/* Define this if gethostbyname_r takes 5 arguments */
/* #undef HAVE_GETHOSTBYNAME_R_5_ARG */

/* Define this if gethostbyname_r takes 6 arguments */
#define HAVE_GETHOSTBYNAME_R_6_ARG 1

/* Define to 1 if you have the `getifaddrs' function. */
#define HAVE_GETIFADDRS 1

/* Define to 1 if you have the `getresgid' function. */
#define HAVE_GETRESGID 1

/* Define to 1 if you have the `getresuid' function. */
#define HAVE_GETRESUID 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `gmtime_r' function. */
#define HAVE_GMTIME_R 1

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `ioctl' function. */
#define HAVE_IOCTL 1

/* Define to 1 if you have the `issetugid' function. */
/* #undef HAVE_ISSETUGID */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/netfilter_ipv4.h> header file. */
#define HAVE_LINUX_NETFILTER_IPV4_H 1

/* Define to 1 if you have the <linux/types.h> header file. */
#define HAVE_LINUX_TYPES_H 1

/* Define to 1 if you have the `llround' function. */
/* #undef HAVE_LLROUND */

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `lround' function. */
/* #undef HAVE_LROUND */

/* Define to 1 if you have the <machine/limits.h> header file. */
/* #undef HAVE_MACHINE_LIMITS_H */

/* Defined if the compiler supports __FUNCTION__ */
#define HAVE_MACRO__FUNCTION__ 1

/* Defined if the compiler supports __FUNC__ */
/* #undef HAVE_MACRO__FUNC__ */

/* Defined if the compiler supports __func__ */
#define HAVE_MACRO__func__ 1

/* Define to 1 if you have the `mallinfo' function. */
#define HAVE_MALLINFO 1

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the <malloc/malloc.h> header file. */
/* #undef HAVE_MALLOC_MALLOC_H */

/* Define to 1 if you have the <malloc_np.h> header file. */
/* #undef HAVE_MALLOC_NP_H */

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mlockall' function. */
#define HAVE_MLOCKALL 1

/* Define to 1 if you have the <nacl/crypto_scalarmult_curve25519.h> header
   file. */
/* #undef HAVE_NACL_CRYPTO_SCALARMULT_CURVE25519_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in6.h> header file. */
/* #undef HAVE_NETINET_IN6_H */

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Define to 1 if you have the <net/pfvar.h> header file. */
/* #undef HAVE_NET_PFVAR_H */

/* Define to 1 if you have the `prctl' function. */
#define HAVE_PRCTL 1

/* Define to 1 if you have the `pthread_create' function. */
#define HAVE_PTHREAD_CREATE 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the `rint' function. */
/* #undef HAVE_RINT */

/* Define to 1 if the system has the type `rlim_t'. */
#define HAVE_RLIM_T 1

/* Define to 1 if the system has the type `sa_family_t'. */
#define HAVE_SA_FAMILY_T 1

/* Define to 1 if you have the <seccomp.h> header file. */
/* #undef HAVE_SECCOMP_H */

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `socketpair' function. */
#define HAVE_SOCKETPAIR 1

/* Define to 1 if the system has the type `ssize_t'. */
#define HAVE_SSIZE_T 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
/* #undef HAVE_STRLCAT */

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strptime' function. */
#define HAVE_STRPTIME 1

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if you have the `strtoull' function. */
#define HAVE_STRTOULL 1

/* Define to 1 if `min_heap_idx' is a member of `struct event'. */
/* #undef HAVE_STRUCT_EVENT_MIN_HEAP_IDX */

/* Define to 1 if the system has the type `struct in6_addr'. */
#define HAVE_STRUCT_IN6_ADDR 1

/* Define to 1 if `s6_addr16' is a member of `struct in6_addr'. */
#define HAVE_STRUCT_IN6_ADDR_S6_ADDR16 1

/* Define to 1 if `s6_addr32' is a member of `struct in6_addr'. */
#define HAVE_STRUCT_IN6_ADDR_S6_ADDR32 1

/* Define to 1 if the system has the type `struct sockaddr_in6'. */
#define HAVE_STRUCT_SOCKADDR_IN6 1

/* Define to 1 if `sin6_len' is a member of `struct sockaddr_in6'. */
/* #undef HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN */

/* Define to 1 if `sin_len' is a member of `struct sockaddr_in'. */
/* #undef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

/* Define to 1 if `tv_sec' is a member of `struct timeval'. */
#define HAVE_STRUCT_TIMEVAL_TV_SEC 1

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/fcntl.h> header file. */
#define HAVE_SYS_FCNTL_H 1

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/limits.h> header file. */
/* #undef HAVE_SYS_LIMITS_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
#define HAVE_SYS_PRCTL_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/syslimits.h> header file. */
/* #undef HAVE_SYS_SYSLIMITS_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/ucontext.h> header file. */
#define HAVE_SYS_UCONTEXT_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
/* #undef HAVE_SYS_UTIME_H */

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <ucontext.h> header file. */
#define HAVE_UCONTEXT_H 1

/* Define to 1 if the system has the type `uint'. */
#define HAVE_UINT 1

/* Define to 1 if you have the `uname' function. */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if the system has the type `u_char'. */
#define HAVE_U_CHAR 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the `_NSGetEnviron' function. */
/* #undef HAVE__NSGETENVIRON */

/* Define to 1 if you have the `_vscprintf' function. */
/* #undef HAVE__VSCPRINTF */

/* Defined if we want to keep track of how much of each kind of resource we
   download. */
/* #undef INSTRUMENT_DOWNLOADS */

/* name of the syslog facility */
#define LOGFACILITY LOG_DAEMON

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to 1 iff malloc(0) returns a pointer */
#define MALLOC_ZERO_WORKS 1

/* Define to 1 if we are building with UPnP. */
/* #undef MINIUPNPC */

/* libminiupnpc version 1.5 found */
/* #undef MINIUPNPC15 */

/* Define to 1 if we are building with nat-pmp. */
/* #undef NAT_PMP */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Define to 1 iff memset(0) sets pointers to NULL */
#define NULL_REP_IS_ZERO_BYTES 1

/* "Define to handle pf on OpenBSD properly" */
/* #undef OPENBSD */

/* Name of package */
#define PACKAGE "tor"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "tor"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "tor 0.2.5.1-alpha-dev"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "tor"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.2.5.1-alpha-dev"

/* How to access the PC from a struct ucontext */
/* #define PC_FROM_UCONTEXT uc_mcontext.gregs[REG_RIP] */

/* Define to 1 iff right-shifting a negative value performs sign-extension */
#define RSHIFT_DOES_SIGN_EXTEND 1

/* The size of `cell_t', as computed by sizeof. */
#define SIZEOF_CELL_T 0

/* The size of `char', as computed by sizeof. */
#define SIZEOF_CHAR 1

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `int16_t', as computed by sizeof. */
#define SIZEOF_INT16_T 2

/* The size of `int32_t', as computed by sizeof. */
#define SIZEOF_INT32_T 4

/* The size of `int64_t', as computed by sizeof. */
#define SIZEOF_INT64_T 8

/* The size of `int8_t', as computed by sizeof. */
#define SIZEOF_INT8_T 1

/* The size of `intptr_t', as computed by sizeof. */
#define SIZEOF_INTPTR_T 8

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `pid_t', as computed by sizeof. */
#define SIZEOF_PID_T 4

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 8

/* The size of `socklen_t', as computed by sizeof. */
#define SIZEOF_SOCKLEN_T 4

/* The size of `time_t', as computed by sizeof. */
#define SIZEOF_TIME_T 8

/* The size of `uint16_t', as computed by sizeof. */
#define SIZEOF_UINT16_T 2

/* The size of `uint32_t', as computed by sizeof. */
#define SIZEOF_UINT32_T 4

/* The size of `uint64_t', as computed by sizeof. */
#define SIZEOF_UINT64_T 8

/* The size of `uint8_t', as computed by sizeof. */
#define SIZEOF_UINT8_T 1

/* The size of `uintptr_t', as computed by sizeof. */
#define SIZEOF_UINTPTR_T 8

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* The size of `__int64', as computed by sizeof. */
#define SIZEOF___INT64 0

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if time_t is signed */
#define TIME_T_IS_SIGNED 1

/* Defined if we're going to use Libevent's buffered IO API */
/* #undef USE_BUFFEREVENTS */

/* Defined if we should use an internal curve25519_donna{,_c64} implementation
   */
#define USE_CURVE25519_DONNA 1

/* Defined if we should use a curve25519 from nacl */
/* #undef USE_CURVE25519_NACL */

/* Debug memory allocation library */
/* #undef USE_DMALLOC */

/* "Define to enable transparent proxy support" */
#define USE_TRANSPARENT 1

/* Define to 1 iff we represent negative integers with two's complement */
#define USING_TWOS_COMPLEMENT 1

/* Version number of package */
#define VERSION "0.2.5.1-alpha-dev"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define on some platforms to activate x_r() functions in time.h */
/* #undef _REENTRANT */

/* Define to `unsigned short' if <sys/types.h> does not define. */
/* #undef u_int16_t */

/* Define to `unsigned long' if <sys/types.h> does not define. */
/* #undef u_int32_t */

/* Define to `unsigned long long' if <sys/types.h> does not define. */
/* #undef u_int64_t */

/* Define to `unsigned char' if <sys/types.h> does not define. */
/* #undef u_int8_t */
