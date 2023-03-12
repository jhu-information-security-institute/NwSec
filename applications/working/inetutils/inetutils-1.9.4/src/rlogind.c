/*
  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
  2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
  2013, 2014, 2015 Free Software Foundation, Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

/*
 * PAM implementation by Mats Erik Andersson.
 *
 * The service names `rlogin' and `krlogin' are registered,
 * the latter when Shishi support is included, and two
 * facilities `auth' and `account' are used for confirmation.
 * These two facilities suffice, since further verification
 * is completed by login(1), which continues where `rlogind'
 * hands off execution.
 *
 * Sample settings:
 *
 *   GNU/Linux, et cetera:
 *
 *     auth    required  pam_nologin.so
 *     auth    required  pam_rhosts.so
 *
 *     account required  pam_nologin.so
 *     account required  pam_unix.so
 *
 *   BSD:
 *
 *     auth    required  pam_nologin.so   # NetBSD
 *     auth    required  pam_rhosts.so
 *
 *     account required  pam_nologin.so   # FreeBSD
 *     account required  pam_unix.so
 *
 *   OpenSolaris:
 *
 *     auth    required  pam_rhosts_auth.so
 *     auth    required  pam_unix_auth.so
 *
 *     account required  pam_unix_roles.so
 *     account required  pam_unix_account.so
 *
 */

#include <config.h>

#include <signal.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <termios.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif
#ifdef HAVE_SYS_TTY_H
# include <sys/tty.h>
#endif
#ifdef HAVE_SYS_PTYVAR_H
# include <sys/ptyvar.h>
#endif

#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_UTMP_H
# include <utmp.h>
#endif

#include <pwd.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>		/* Needed for chmod() */

#include <pty.h>

#ifdef HAVE_TCPD_H
# include <tcpd.h>
#endif

#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#endif

#include <progname.h>
#include <argp.h>
#include <libinetutils.h>
#include "unused-parameter.h"
#include "xalloc.h"

/*
  The TIOCPKT_* macros may not be implemented in the pty driver.
  Defining them here allows the program to be compiled.  */
#ifndef TIOCPKT
# define TIOCPKT                 _IOW('t', 112, int)
# define TIOCPKT_FLUSHWRITE      0x02
# define TIOCPKT_NOSTOP          0x10
# define TIOCPKT_DOSTOP          0x20
#endif /*TIOCPKT*/
#ifndef TIOCPKT_WINDOW
# define TIOCPKT_WINDOW 0x80
#endif
/* `defaults' for tty settings.  */
#ifndef TTYDEF_IFLAG
# define TTYDEF_IFLAG	(BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#endif
#ifndef TTYDEF_OFLAG
# ifndef OXTABS
#  define OXTABS 0
# endif
# define TTYDEF_OFLAG	(OPOST | ONLCR | OXTABS)
#endif
#ifndef TTYDEF_LFLAG
# define TTYDEF_LFLAG	(ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#endif
#define AUTH_KERBEROS_SHISHI 1
#define AUTH_KERBEROS_4 4
#define AUTH_KERBEROS_5 5
#if defined KERBEROS || defined SHISHI
# ifdef	KRB4
#  define SECURE_MESSAGE "This rlogin session is using DES encryption for all transmissions.\r\n"
#  ifdef HAVE_KERBEROSIV_DES_H
#   include <kerberosIV/des.h>
#  endif
#  ifdef HAVE_KERBEROSIV_KRB_H
#   include <kerberosIV/krb.h>
#  endif
#  define kerberos_error_string(c) krb_err_txt[c]
#  define AUTH_KERBEROS_DEFAULT AUTH_KERBEROS_4
# elif defined(KRB5)
#  define SECURE_MESSAGE "This rlogin session is using DES encryption for all transmissions.\r\n"
#  ifdef HAVE_KRB5_H
#   include <krb5.h>
#  endif
#  ifdef HAVE_COM_ERR_H
#   include <com_err.h>
#  endif
#  ifdef HAVE_KERBEROSIV_KRB_H
#   include <kerberosIV/krb.h>
#  endif
#  define kerberos_error_string(c) error_message (c)
#  define AUTH_KERBEROS_DEFAULT AUTH_KERBEROS_5
# elif defined(SHISHI)
#  define SECURE_MESSAGE "This rlogin session is using encryption for all transmissions.\r\n"
#  include <shishi.h>
#  include "shishi_def.h"
#  define AUTH_KERBEROS_DEFAULT AUTH_KERBEROS_SHISHI
#  define kerberos_error_string(c) shishi_strerror(c)
# endif
#endif /* KERBEROS */
#define ENVSIZE	(sizeof("TERM=")-1)	/* skip null for concatenation */
#ifndef DEFMAXCHILDREN
# define DEFMAXCHILDREN 10	/* Default maximum number of children */
#endif
#ifndef DEFPORT
# define DEFPORT 513
#endif
#ifndef DEFPORT_KLOGIN
# define DEFPORT_KLOGIN 543
#endif
#ifndef DEFPORT_EKLOGIN
# define DEFPORT_EKLOGIN 2105
#endif

#ifdef HAVE___CHECK_RHOSTS_FILE
extern int __check_rhosts_file;
#endif

#ifndef SHISHI
struct auth_data
{
  struct sockaddr_storage from;
  socklen_t fromlen;
  char *hostaddr;
  char *hostname;
  char *lusername;
  char *rusername;
  char *rprincipal;
  char *term;
  char *env[2];
# ifdef KERBEROS
#  ifdef KRB5
  int kerberos_version;
  krb5_principal client;
  krb5_context context;
  krb5_ccache ccache;
  krb5_keytab keytab;
#  endif
# endif /* KERBEROS */
};
#endif /* !SHISHI */

#define MODE_INETD 0
#define MODE_DAEMON 1
int mode = MODE_INETD;

int use_af = AF_UNSPEC;
int port = 0;
int maxchildren = DEFMAXCHILDREN;
int allow_root = 0;
int verify_hostname = 0;
int keepalive = 1;

#ifdef WITH_PAM
static int pam_rc = PAM_AUTH_ERR;
static pam_handle_t *pam_handle = NULL;
static int rlogin_conv (int, const struct pam_message **,
		       struct pam_response **, void *);
static struct pam_conv pam_conv = { rlogin_conv, NULL };
#endif /* WITH_PAM */

#if defined KERBEROS || defined SHISHI
int kerberos = 0;
char *servername = NULL;

# ifdef ENCRYPTION
int encrypt_io = 0;
# endif	/* ENCRYPTION */
#endif /* KERBEROS || SHISHI */

int reverse_required = 0;
int debug_level = 0;
int numchildren;
int netf;
char line[1024];		/* FIXME */
int confirmed;
const char *path_login = PATH_LOGIN;
char *local_domain_name;
int local_dot_count;

struct winsize win = { 0, 0, 0, 0 };

#ifdef WITH_WRAP
int allow_severity = LOG_INFO;
int deny_severity = LOG_NOTICE;

# if !HAVE_DECL_HOSTS_CTL
extern int hosts_ctl (char *, char *, char *, char *);
# endif
#endif /* WITH_WRAP */

#if defined __GLIBC__ && defined WITH_IRUSEROK
extern int iruserok (uint32_t raddr, int superuser,
                     const char *ruser, const char *luser);
#endif

void rlogin_daemon (int maxchildren, int port);
int rlogind_auth (int fd, struct auth_data *ap);
void setup_tty (int fd, struct auth_data *ap);
void exec_login (int authenticated, struct auth_data *ap);
int rlogind_mainloop (int infd, int outfd);
int do_rlogin (int infd, struct auth_data *ap);
int do_krb_login (int infd, struct auth_data *ap, const char **msg);
void getstr (int infd, char **ptr, const char *prefix);
void protocol (int f, int p, struct auth_data *ap);
int control (int pty, char *cp, size_t n);
void cleanup (int signo);
void fatal (int f, const char *msg, int syserr);
void rlogind_error (int f, int syserr, const char *msg, ...);
int in_local_domain (char *hostname);
char *topdomain (char *name, int max_dots);

#ifdef KRB5
int do_krb5_login (int infd, struct auth_data *ap, const char **err_msg);
#endif

#ifdef SHISHI
int do_shishi_login (int infd, struct auth_data *ad, const char **err_msg);
#endif

#ifdef WITH_PAM
int do_pam_check (int infd, struct auth_data *ap, const char *service);
#endif

#ifdef IP_OPTIONS
void prevent_routing (int fd, struct auth_data *ap);
#endif

void
rlogind_sigchld (int signo _GL_UNUSED_PARAMETER)
{
  pid_t pid;
  int status;

  while ((pid = waitpid (-1, &status, WNOHANG)) > 0)
    --numchildren;
}

#ifdef WITH_WRAP
static int
check_host (struct sockaddr *sa, socklen_t len)
{
  int rc;
  char addr[INET6_ADDRSTRLEN];
# if HAVE_DECL_GETNAMEINFO
  char name[NI_MAXHOST];
# else
  struct hostent *hp;
  void *addrp;
  char *name;
# endif /* !HAVE_DECL_NAMEINFO */

  if (sa->sa_family != AF_INET
# ifdef IPV6
      && sa->sa_family != AF_INET6
# endif
     )
    return 1;

# if HAVE_DECL_GETNAMEINFO
  (void) getnameinfo(sa, len, addr, sizeof (addr), NULL, 0, NI_NUMERICHOST);
  rc = getnameinfo(sa, len, name, sizeof (name), NULL, 0, NI_NAMEREQD);
# else /* !HAVE_DECL_GETNAMEINFO */

  (void) len;		/* Silence warning.  */

  switch (sa->sa_family)
    {
#  ifdef IPV6
    case AF_INET6:
      addrp = (void *) &((struct sockaddr_in6 *) sa)->sin6_addr;
      hp = gethostbyaddr (addrp, sizeof (struct in6_addr),
			  sa->sa_family);
      break;
#  endif
    case AF_INET:
    default:
      addrp = (void *) &((struct sockaddr_in *) sa)->sin_addr;
      hp = gethostbyaddr (addrp, sizeof (struct in_addr),
			  sa->sa_family);
    }

  (void) inet_ntop (sa->sa_family, addrp, addr, sizeof (addr));
  if (hp)
    name = hp->h_name;
  rc = (hp == NULL);		/* Translate to getnameinfo style.  */
# endif /* !HAVE_DECL_GETNAMEINFO */

  if (!rc)
    {
      if (!hosts_ctl ("rlogind", name, addr, STRING_UNKNOWN))
	{
	  syslog (deny_severity, "tcpd rejects %s [%s]",
		  name, addr);
	  return 0;
	}
    }
  else
    {
      if (!hosts_ctl ("rlogind", STRING_UNKNOWN, addr, STRING_UNKNOWN))
	{
	  syslog (deny_severity, "tcpd rejects [%s]", addr);
	  return 0;
	}
    }
  return 1;
}
#endif /* WITH_WRAP */

#if defined KERBEROS && defined ENCRYPTION
# define ENCRYPT_IO encrypt_io
# define IF_ENCRYPT(stmt) if (encrypt_io) stmt
# define IF_NOT_ENCRYPT(stmt) if (!encrypt_io) stmt
# define ENC_READ(c, fd, buf, size, ap) \
 if (encrypt_io) \
     c = des_read(fd, buf, size); \
 else \
     c = read(fd, buf, size);
# define ENC_WRITE(c, fd, buf, size, ap) \
 if (encrypt_io) \
     c = des_write(fd, buf, size); \
 else \
     c = write(fd, buf, size);
#elif defined(SHISHI) && defined (ENCRYPTION)
# define ENCRYPT_IO encrypt_io
# define IF_ENCRYPT(stmt) if (encrypt_io) stmt
# define IF_NOT_ENCRYPT(stmt) if (!encrypt_io) stmt
# define ENC_READ(c, fd, buf, size, ap) \
 if (encrypt_io) \
     readenc (ap->h, fd, buf, &c, &ap->iv1, ap->enckey, ap->protocol); \
 else \
     c = read(fd, buf, size);
# define ENC_WRITE(c, fd, buf, size, ap) \
 if (encrypt_io) \
     writeenc (ap->h, fd, buf, size, &c, &ap->iv2, ap->enckey, ap->protocol); \
 else \
     c = write(fd, buf, size);
#else
# define ENCRYPT_IO 0
# define IF_ENCRYPT(stmt)
# define IF_NOT_ENCRYPT(stmt) stmt
# define ENC_READ(c, fd, buf, size, ap) c = read (fd, buf, size)
# define ENC_WRITE(c, fd, buf, size, ap) c = write (fd, buf, size)
#endif


const char doc[] =
#ifdef WITH_PAM
# ifdef SHISHI
  "Remote login server, using PAM service 'rlogin' and 'krlogin'.";
# else /* !SHISHI */
  "Remote login server, using PAM service 'rlogin'.";
# endif
#else /* !WITH_PAM */
  "Remote login server";
#endif
const char *program_authors[] = {
  "Alain Magloire",
  "Sergey Poznyakoff",
  NULL
};

static struct argp_option options[] = {
#define GRP 10
  { "ipv4", '4', NULL, 0,
    "daemon mode only accepts IPv4", GRP },
#ifdef IPV6
  { "ipv6", '6', NULL, 0,
    "only IPv6 in daemon mode", GRP },
#endif
  { "allow-root", 'o', NULL, 0,
    "allow uid 0 to login, disabled by default", GRP },
  { "verify-hostname", 'a', NULL, 0,
    "ask hostname for verification", GRP },
  { "daemon", 'd', "MAX", OPTION_ARG_OPTIONAL,
    "daemon mode, with instance limit", GRP },
#ifdef HAVE___CHECK_RHOSTS_FILE
  { "no-rhosts", 'l', NULL, 0,
    "ignore .rhosts file", GRP },
#endif
  { "no-keepalive", 'n', NULL, 0,
    "do not set SO_KEEPALIVE", GRP },
  { "local-domain", 'L', "NAME", 0,
    "set local domain name", GRP },
  { "debug", 'D', "LEVEL", OPTION_ARG_OPTIONAL,
    "set debug level", GRP },
  { "port", 'p', "PORT", 0,
    "listen on given port (valid only in daemon mode)", GRP },
  { "reverse-required", 'r', NULL, 0,
    "require reverse resolving of a remote host IP", GRP },
#undef GRP
#if defined KERBEROS || defined SHISHI
# define GRP 20
  { "kerberos", 'k', NULL, 0,
    "use Kerberos V authentication", GRP },
  { "server-principal", 'S', "NAME", 0,
    "set Kerberos server name, overriding canonical hostname", GRP },
# if defined ENCRYPTION
  { "encrypt", 'x', NULL, 0,
    "use encryption", GRP },
# endif
# undef GRP
#endif
  { NULL, 0, NULL, 0, NULL, 0 }
};

static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state _GL_UNUSED_PARAMETER)
{
  switch (key)
    {
    case '4':
      use_af = AF_INET;
      break;

#ifdef IPV6
    case '6':
      use_af = AF_INET6;
      break;
#endif

    case 'a':
      verify_hostname = 1;
      break;

    case 'D':
      if (arg)
	debug_level = strtoul (arg, NULL, 10);
      break;

    case 'd':
      mode = MODE_DAEMON;
      if (arg)
	maxchildren = strtoul (arg, NULL, 10);
      if (maxchildren == 0)
	maxchildren = DEFMAXCHILDREN;
      break;

#ifdef HAVE___CHECK_RHOSTS_FILE
    case 'l':
      __check_rhosts_file = 0;	/* FIXME: extern var? */
      break;
#endif

    case 'L':
      local_domain_name = arg;
      break;

    case 'n':
      keepalive = 0;
      break;

#if defined KERBEROS || defined SHISHI
    case 'k':
      kerberos = AUTH_KERBEROS_DEFAULT;
      break;

    case 'S':
      servername = arg;
      break;

# ifdef ENCRYPTION
    case 'x':
      encrypt_io = 1;
      break;
# endif	/* ENCRYPTION */
#endif /* KERBEROS */

    case 'o':
      allow_root = 1;
      break;

    case 'p':
      port = strtoul (arg, NULL, 10);
      break;

    case 'r':
      reverse_required = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

static struct argp argp = { options, parse_opt, NULL, doc, NULL, NULL, NULL};



int
main (int argc, char *argv[])
{
  int index;

  set_program_name (argv[0]);

  /* Parse command line */
  iu_argp_init ("rlogind", program_authors);
  argp_parse (&argp, argc, argv, 0, &index, NULL);

  openlog ("rlogind", LOG_PID | LOG_CONS, LOG_DAEMON);
  argc -= index;
  if (argc > 0)
    {
      syslog (LOG_ERR, "%d extra arguments", argc);
      exit (EXIT_FAILURE);
    }

  setsig (SIGHUP, SIG_IGN);

  if (!local_domain_name)
    {
      char *p = localhost ();

      if (!p)
	{
	  syslog (LOG_ERR, "can't determine local hostname");
	  exit (EXIT_FAILURE);
	}
      local_dot_count = 2;
      local_domain_name = topdomain (p, local_dot_count);
    }
  else
    {
      char *p;

      local_dot_count = 0;
      for (p = local_domain_name; *p; p++)
	if (*p == '.')
	  local_dot_count++;
    }

  if (mode == MODE_DAEMON)
    rlogin_daemon (maxchildren, port);
  else
    exit (rlogind_mainloop (fileno (stdin), fileno (stdout)));

  return 0;		/* Not reachable.  */
}

/* Create a listening socket for the indicated
 * address family and port.  Then bind a wildcard
 * address to it.
 */
int
find_listenfd (int family, int port)
{
  int fd, on = 1;
  socklen_t size;
#if HAVE_DECL_GETADDRINFO
  int rc;
  struct sockaddr_storage saddr;
  struct addrinfo hints, *ai, *res;
  char portstr[16];
#else /* !HAVE_DECL_GETADDRINFO */
  struct sockaddr_in saddr;

  /* Enforce IPv4, lacking getaddrinfo().  */
  if (family != AF_INET)
    return -1;
#endif

#if HAVE_DECL_GETADDRINFO
  memset (&hints, 0, sizeof hints);
  hints.ai_family = family;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_STREAM;
  snprintf (portstr, sizeof portstr, "%u", port);

  rc = getaddrinfo (NULL, portstr, &hints, &res);
  if (rc)
    {
      syslog (LOG_ERR, "getaddrinfo: %s", gai_strerror (rc));
      return -1;
    }

  /* Passive socket, so grab the first relevant answer.  */
  for (ai = res; ai; ai = ai->ai_next)
    if (ai->ai_family == family)
      break;

  if (ai == NULL)
    {
      syslog (LOG_ERR, "address family not available");
      freeaddrinfo (res);
      return -1;
    }

  size = ai->ai_addrlen;
  memcpy (&saddr, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo (res);

#else /* !HAVE_DECL_GETADDRINFO */
  size = sizeof saddr;
  memset (&saddr, 0, size);
  saddr.sin_family = family;
# ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  saddr.sin_len = sizeof (struct sockaddr_in);
# endif
  saddr.sin_addr.s_addr = htonl (INADDR_ANY);
  saddr.sin_port = htons (port);
#endif

  fd = socket (family, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  (void) setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

#ifdef IPV6
  /* Make it a single ended socket.  */
  if (family == AF_INET6)
    (void) setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof on);
#endif

  if (bind (fd, (struct sockaddr *) &saddr, size) == -1)
    {
      syslog (LOG_ERR, "bind: %s", strerror (errno));
      close (fd);
      return -1;
    }

  if (listen (fd, 128) == -1)
    {
      syslog (LOG_ERR, "listen: %s", strerror (errno));
      close (fd);
      return -1;
    }

  return fd;
}

void
rlogin_daemon (int maxchildren, int port)
{
  int listenfd[2], fd, numfd, maxfd;

  if (port == 0)
    {
      char *service;
      struct servent *svp;

#if defined KERBEROS || defined SHISHI
# ifdef ENCRYPTION
      if (kerberos && encrypt_io)
	{
	  service = "eklogin";
	  port = DEFPORT_EKLOGIN;
	}
      else
# endif /* ENCRYPTION */
	if (kerberos)
	  {
	    service = "klogin";
	    port = DEFPORT_KLOGIN;
	  }
	else
#endif /* KERBEROS || SHISHI */
	  {
	    service = "login";
	    port = DEFPORT;
	  }

      svp = getservbyname (service, "tcp");
      if (svp != NULL)
	port = ntohs (svp->s_port);
    }

  /* Become a daemon. Take care to close inherited fds and reserve
     the first three, lest master/slave ptys may clash with standard
     input, output, or error.  */
  if (daemon (0, 0) < 0)
    {
      syslog (LOG_ERR, "failed to become a daemon %s", strerror (errno));
      fatal (fileno (stderr), "fork failed, exiting", 0);
    }

  setsig (SIGCHLD, rlogind_sigchld);

  numfd = maxfd = 0;

  if ((use_af == AF_UNSPEC) || (use_af == AF_INET))
    {
      fd = find_listenfd (AF_INET, port);
      if (fd >= 0)
	listenfd[numfd++] = maxfd = fd;
    }

#ifdef IPV6
  if ((use_af == AF_UNSPEC) || (use_af == AF_INET6))
    {
      fd = find_listenfd (AF_INET6, port);
      if (fd >= 0)
	{
	  listenfd[numfd++] = fd;
	  if (fd > maxfd)
	    maxfd = fd;
	}
    }
#endif

  if (numfd == 0)
    {
      syslog (LOG_ERR, "socket creation failed");
      exit (EXIT_FAILURE);
    }

  while (1)
    {
      int n, j;
      fd_set lfdset;

      if (numchildren > maxchildren)
	{
	  syslog (LOG_ERR, "too many children (%d)", numchildren);
	  pause ();
	  continue;
	}

      FD_ZERO (&lfdset);
      FD_SET (listenfd[0], &lfdset);
      if (numfd > 1)
	FD_SET (listenfd[1], &lfdset);

      n = select (maxfd + 1, &lfdset, NULL, NULL, NULL);
      if (n <= 0)
	continue;

      for (j = 0; j < numfd; j++)
	{
	  pid_t pid;
	  socklen_t size;
#if HAVE_DECL_GETADDRINFO
	  struct sockaddr_storage saddr;
#else /* !HAVE_DECL_GETADDRINFO */
	  struct sockaddr_in saddr;
#endif

	  if (!FD_ISSET (listenfd[j], &lfdset))
	    continue;

	  size = sizeof (saddr);
	  fd = accept (listenfd[j], (struct sockaddr *) &saddr, &size);

	  if (fd < 0)
	    {
	      if (errno == EINTR)
		continue;
	      syslog (LOG_ERR, "accept: %s", strerror (errno));
	      continue;
	    }

	  pid = fork ();
	  if (pid == -1)
	    syslog (LOG_ERR, "fork: %s", strerror (errno));
	  else if (pid == 0)	/* child */
	    {
	      close (listenfd[0]);
	      if (numfd > 1)
		close (listenfd[1]);
#ifdef WITH_WRAP
	      if (!check_host ((struct sockaddr *) &saddr, size))
		exit (EXIT_FAILURE);
#endif
	      exit (rlogind_mainloop (fd, fd));
	    }
	  /* parent only */
	  numchildren++;
	  close (fd);
	}
    }
  /* NOT REACHED */
}

int
rlogind_auth (int fd, struct auth_data *ap)
{
#if HAVE_DECL_GETNAMEINFO && HAVE_DECL_GETADDRINFO
  int rc;
  char hoststr[NI_MAXHOST];
#else
  struct hostent *hp;
  void *addrp;
#endif
  char *hostname = "";
  int authenticated = 0;
  int port;

#ifdef SHISHI
  int len, c;
#endif

  switch (ap->from.ss_family)
    {
    case AF_INET6:
#if !HAVE_DECL_GETADDRINFO || !HAVE_DECL_GETNAMEINFO
      addrp = (void *) &((struct sockaddr_in6 *) &ap->from)->sin6_addr;
#endif
      port = ntohs (((struct sockaddr_in6 *) &ap->from)->sin6_port);
      break;
    case AF_INET:
    default:
#if !HAVE_DECL_GETADDRINFO || !HAVE_DECL_GETNAMEINFO
      addrp = (void *) &((struct sockaddr_in *) &ap->from)->sin_addr;
#endif
      port = ntohs (((struct sockaddr_in *) &ap->from)->sin_port);
    }

  confirmed = 0;

  /* Check the remote host name */
#if HAVE_DECL_GETNAMEINFO
  rc = getnameinfo ((struct sockaddr *) &ap->from, ap->fromlen,
		    hoststr, sizeof (hoststr), NULL, 0, NI_NAMEREQD);
  if (!rc)
    hostname = hoststr;
#else /* !HAVE_DECL_GETNAMEINFO */
  switch (ap->from.ss_family)
    {
    case AF_INET6:
      hp = gethostbyaddr (addrp, sizeof (struct in6_addr),
			  ap->from.ss_family);
      break;
    case AF_INET:
    default:
      hp = gethostbyaddr (addrp, sizeof (struct in_addr),
			  ap->from.ss_family);
    }
  if (hp)
    hostname = hp->h_name;
#endif /* !HAVE_DECL_GETNAMEINFO */

  else if (reverse_required)
    {
      syslog (LOG_NOTICE, "can't resolve remote IP address");
      fatal (fd, "Permission denied", 0);
    }
  else
    hostname = ap->hostaddr;

  ap->hostname = strdup (hostname);

  if (verify_hostname || in_local_domain (ap->hostname))
    {
      int match = 0;
#if HAVE_DECL_GETADDRINFO && HAVE_DECL_GETNAMEINFO
      struct addrinfo hints, *ai, *res;
      char astr[INET6_ADDRSTRLEN];

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = ap->from.ss_family;
      hints.ai_socktype = SOCK_STREAM;

      rc = getaddrinfo (ap->hostname, NULL, &hints, &res);
      if (!rc)
	{
	  for (ai = res; ai; ai = ai->ai_next)
	    {
	      rc = getnameinfo (ai->ai_addr, ai->ai_addrlen,
				astr, sizeof (astr), NULL, 0,
				NI_NUMERICHOST);
	      if (rc)
		continue;
	      match = strcmp (astr, ap->hostaddr) == 0;
	      if (match)
		break;
	    }
	  freeaddrinfo (res);
	}
#else /* !HAVE_DECL_GETADDRINFO */
      for (hp = gethostbyname (ap->hostname); hp && !match; hp->h_addr_list++)
	{
	  if (hp->h_addr_list[0] == NULL)
	    break;
	  match = memcmp (hp->h_addr_list[0], addrp, hp->h_length) == 0;
	}
#endif /* !HAVE_DECL_GETADDRINFO */
      if (!match)
	{
	  syslog (LOG_ERR | LOG_AUTH, "cannot verify matching IP for %s (%s)",
		  ap->hostname, ap->hostaddr);
	  fatal (fd, "Permission denied", 0);
	}
    }

#ifdef IP_OPTIONS
  prevent_routing (fd, ap);
#endif

#if defined KERBEROS || defined SHISHI
  if (kerberos)
    {
      const char *err_msg;
      int c = 0;

      if (do_krb_login (fd, ap, &err_msg) == 0)
	authenticated++;
      else
	fatal (fd, err_msg, 0);
      write (fd, &c, 1);
      confirmed = 1;		/* We have sent the null!  */
    }
  else
#endif
    {
      if ((ap->from.ss_family != AF_INET
#ifndef KERBEROS
	   && ap->from.ss_family != AF_INET6
#endif
	  )
	  || port >= IPPORT_RESERVED || port < IPPORT_RESERVED / 2)
	{
	  syslog (LOG_NOTICE, "Connection from %s on illegal port %d",
		  ap->hostaddr, port);
	  fatal (fd, "Permission denied", 0);
	}

      if (do_rlogin (fd, ap) == 0)
	authenticated++;
    }

  if (confirmed == 0)
    {
      write (fd, "", 1);
      confirmed = 1;		/* we sent the null! */
    }
#ifdef SHISHI
  len = sizeof (SECURE_MESSAGE) - 1;
  IF_ENCRYPT (writeenc
	      (ap->h, fd, SECURE_MESSAGE, len, &c, &ap->iv2, ap->enckey,
	       ap->protocol));
#else
  IF_ENCRYPT (des_write (fd, SECURE_MESSAGE, sizeof (SECURE_MESSAGE) - 1));
#endif
  return authenticated;
}

#ifdef IP_OPTIONS
void
prevent_routing (int fd, struct auth_data *ap)
{
  unsigned char optbuf[BUFSIZ / 3], *cp;
  char lbuf[BUFSIZ], *lp;
  socklen_t optsize = sizeof (optbuf);
  int ipproto;
  struct protoent *ip;

  ip = getprotobyname ("ip");
  if (ip != NULL)
    ipproto = ip->p_proto;
  else
    ipproto = IPPROTO_IP;

  if (getsockopt (fd, ipproto, IP_OPTIONS, (char *) optbuf, &optsize) == 0
      && optsize != 0)
    {
      lp = lbuf;
      for (cp = optbuf; optsize > 0; )
	{
	  sprintf (lp, " %2.2x", *cp);
	  lp += 3;

	  /* These two open an attack vector.  */
	  if (*cp == IPOPT_SSRR || *cp == IPOPT_LSRR)
	    {
	      syslog (LOG_NOTICE,
		      "Discarding connection from %s with set source routing",
		      ap->hostaddr);
	      exit (EXIT_FAILURE);
	    }

	  if (*cp == IPOPT_EOL)
	    break;

	  if (*cp == IPOPT_NOP)
	    cp++, optsize--;
	  else
	    {
	      /* Options using a length octet, see RFC 791.  */
	      int inc = cp[1];

	      optsize -= inc;
	      cp += inc;
	    }
	}

      syslog (LOG_NOTICE, "Ignoring IP options: %s", lbuf);

      if (setsockopt (fd, ipproto, IP_OPTIONS, (char *) NULL, optsize))
	{
	  syslog (LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
	  exit (EXIT_FAILURE);
	}
    }
}
#endif /* IP_OPTIONS */

void
setup_tty (int fd, struct auth_data *ap)
{
  register char *cp = strchr (ap->term + ENVSIZE, '/');
  char *speed;
  struct termios tt;

  tcgetattr (fd, &tt);
  if (cp)
    {
      *cp++ = '\0';
      speed = cp;
      cp = strchr (speed, '/');
      if (cp)
	*cp++ = '\0';
#ifdef HAVE_CFSETSPEED
      cfsetspeed (&tt, strtoul (speed, NULL, 10));
#else
      cfsetispeed (&tt, strtoul (speed, NULL, 10));
      cfsetospeed (&tt, strtoul (speed, NULL, 10));
#endif
    }
  tt.c_iflag = TTYDEF_IFLAG;
  tt.c_oflag = TTYDEF_OFLAG;
  tt.c_lflag = TTYDEF_LFLAG;
  tcsetattr (fd, TCSAFLUSH, &tt);
  ap->env[0] = ap->term;
  ap->env[1] = 0;
}

void
setup_utmp (char *line, char *host)
{
  char *ut_id = utmp_ptsid (line, "rl");

  utmp_init (line + sizeof (PATH_TTY_PFX) - 1, ".rlogin", ut_id, host);
}

void
exec_login (int authenticated, struct auth_data *ap)
{
  if (authenticated)
    {
#ifdef SOLARIS10
      execle (path_login, "login", "-p", "-r", ap->hostname,
	      "-d", line, "-U", ap->rusername,
# if defined KERBEROS || defined SHISHI
	      "-s", (kerberos ? "krlogin" : "rlogin"),
	      "-u", (ap->rprincipal ? ap->rprincipal : ap->rusername),
# else /* !KERBEROS && !SHISHI */
	      "-s", "rlogin",
# endif
	      ap->lusername, NULL, ap->env);

#elif defined SOLARIS	/* !SOLARIS10 */
      execle (path_login, "login", "-p", "-r", ap->hostname,
	      "-d", line, ap->lusername, NULL, ap->env);

#else /* !SOLARIS */
      /* Some GNU/Linux systems, but not all,  provide `-r'
       * for use instead of `-h'.  Some BSD systems provide `-u'.
       */
      execle (path_login, "login", "-p", "-h", ap->hostname, "-f",
	      ap->lusername, NULL, ap->env);
#endif
    }
  else
    {
#ifdef SOLARIS10
      /* `-U' in not strictly needed, but is probably harmless.  */
      execle (path_login, "login", "-p", "-r", ap->hostname,
	      "-d", line, "-s", "rlogin", "-U", ap->rusername,
	      ap->lusername, NULL, ap->env);
#elif defined SOLARIS
      execle (path_login, "login", "-p", "-r", ap->hostname,
	      "-d", line, ap->lusername, NULL, ap->env);
#else
      /* Some GNU/Linux systems, but not all,  provide `-r'
       * for use instead of `-h'.  Some BSD systems provide `-u'.
       */
      execle (path_login, "login", "-p", "-h", ap->hostname,
	      ap->lusername, NULL, ap->env);
#endif
    }
  syslog (LOG_ERR, "can't exec login: %m");
}

int
rlogind_mainloop (int infd, int outfd)
{
  struct auth_data auth_data;
  char addrstr[INET6_ADDRSTRLEN];
  const char *reply;
  int true;
  char c;
  int authenticated;
  pid_t pid;
  int master;

  memset (&auth_data, 0, sizeof (auth_data));
  auth_data.fromlen = sizeof (auth_data.from);
  if (getpeername (infd, (struct sockaddr *) &auth_data.from,
		   &auth_data.fromlen) < 0)
    {
      syslog (LOG_ERR, "Can't get peer name of remote host: %m");
      fatal (outfd, "Can't get peer name of remote host", 1);
    }

  reply = inet_ntop (auth_data.from.ss_family,
		     (auth_data.from.ss_family == AF_INET6)
		       ? (void *) &((struct sockaddr_in6 *) &auth_data.from)->sin6_addr
		       : (void *) &((struct sockaddr_in *) &auth_data.from)->sin_addr,
		     addrstr, sizeof (addrstr));
  if (reply == NULL)
    {
      syslog (LOG_ERR, "Get numerical address: %m");
      fatal (outfd, "Cannot get numerical address of peer.", 1);
    }
  auth_data.hostaddr = xstrdup (addrstr);

  syslog (LOG_INFO, "Connect from %s:%d", auth_data.hostaddr,
	  (auth_data.from.ss_family == AF_INET6)
	  ? ntohs (((struct sockaddr_in6 *) &auth_data.from)->sin6_port)
	  : ntohs (((struct sockaddr_in *) &auth_data.from)->sin_port));

  true = 1;
  if (keepalive
      && setsockopt (infd, SOL_SOCKET, SO_KEEPALIVE, &true, sizeof true) < 0)
    syslog (LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");

#if defined IP_TOS && defined IPPROTO_IP && defined IPTOS_LOWDELAY
  true = IPTOS_LOWDELAY;
  if (auth_data.from.ss_family == AF_INET &&
      setsockopt (infd, IPPROTO_IP, IP_TOS,
		  (char *) &true, sizeof true) < 0)
    syslog (LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif

  alarm (60);			/* Wait at most 60 seconds. FIXME: configurable? */

  /* Read the null byte */
  if (read (infd, &c, 1) != 1 || c != 0)
    {
      syslog (LOG_ERR, "protocol error: expected 0 byte");
      exit (EXIT_FAILURE);
    }

  alarm (0);

  authenticated = rlogind_auth (infd, &auth_data);

  pid = forkpty (&master, line, NULL, &win);

  if (pid < 0)
    {
      if (errno == ENOENT)
	{
	  syslog (LOG_ERR, "Out of ptys");
	  fatal (infd, "Out of ptys", 0);
	}
      else
	{
	  syslog (LOG_ERR, "forkpty: %m");
	  fatal (infd, "Forkpty", 1);
	}
    }

  if (pid == 0)
    {
      /* Child */
      if (infd > 2)
	close (infd);

      setup_tty (0, &auth_data);
      setup_utmp (line, auth_data.hostname);

      exec_login (authenticated, &auth_data);
      fatal (infd, "can't execute login", 1);
    }

  /* Parent */
  true = 1;
  IF_NOT_ENCRYPT (ioctl (infd, FIONBIO, &true));
  ioctl (master, FIONBIO, &true);
  ioctl (master, TIOCPKT, &true);
  netf = infd;			/* Needed for cleanup() */
  setsig (SIGCHLD, cleanup);
  protocol (infd, master, &auth_data);
  setsig (SIGCHLD, SIG_IGN);

#ifdef SHISHI
  if (kerberos)
    {
      int i;

      shishi_done (auth_data.h);
# ifdef ENCRYPTION
      if (encrypt_io)
	{
	  shishi_key_done (auth_data.enckey);
	  for (i = 0; i < 2; i++)
	    {
	      shishi_crypto_close (auth_data.ivtab[i]->ctx);
	      free (auth_data.ivtab[i]->iv);
	    }
	}
# endif
    }
#endif /* SHISHI */

  cleanup (0);
  /* NOT REACHED */

  return 0;
}


int
do_rlogin (int infd, struct auth_data *ap)
{
  struct passwd *pwd;
  int rc;
#if defined WITH_IRUSEROK_AF || defined WITH_IRUSEROK
  void *addrp;

  switch (ap->from.ss_family)
    {
    case AF_INET6:
      addrp = (void *) &((struct sockaddr_in6 *) &ap->from)->sin6_addr;
      break;
    case AF_INET:
    default:
      addrp = (void *) &((struct sockaddr_in *) &ap->from)->sin_addr;
    }
#endif /* WITH_IRUSEROK_AF || WITH_IRUSEROK */

  getstr (infd, &ap->rusername, NULL);		/* Requesting user.  */
  getstr (infd, &ap->lusername, NULL);		/* Acting user.  */
  getstr (infd, &ap->term, "TERM=");

  pwd = getpwnam (ap->lusername);
  if (pwd == NULL)
    {
      syslog (LOG_ERR | LOG_AUTH, "no passwd entry for %s", ap->lusername);
      fatal (infd, "Permission denied", 0);
    }
  if (!allow_root && pwd->pw_uid == 0)
    {
      syslog (LOG_ERR | LOG_AUTH, "root logins are not permitted");
      fatal (infd, "Permission denied", 0);
    }

#ifdef WITH_PAM
  rc = do_pam_check (infd, ap, "rlogin");
  if (rc != PAM_SUCCESS)
    return rc;
#endif /* WITH_PAM */

#if defined WITH_IRUSEROK_SA || defined WITH_IRUSEROK_AF \
    || defined WITH_IRUSEROK
# ifdef WITH_IRUSEROK_SA
  rc = iruserok_sa ((struct sockaddr *) &ap->from, ap->fromlen, 0,
		    ap->rusername, ap->lusername);
# elif defined WITH_IRUSEROK_AF
  rc = iruserok_af (addrp, 0, ap->rusername, ap->lusername,
		    ap->from.ss_family);
# else /* WITH_IRUSEROK */
  rc = iruserok (addrp, 0, ap->rusername, ap->lusername);
# endif /* WITH_IRUSEROK_SA || WITH_IRUSEROK_AF || WITH_IRUSEROK */
  if (rc)
    syslog (LOG_ERR | LOG_AUTH,
	    "iruserok failed: rusername=%s, lusername=%s",
	    ap->rusername, ap->lusername);
#elif defined WITH_RUSEROK_AF || defined WITH_RUSEROK
# ifdef WITH_RUSEROK_AF
  rc = ruserok_af (ap->hostaddr, 0, ap->rusername, ap->lusername,
		   ap->from.ss_family);
# else /* WITH_RUSEROK */
  rc = ruserok (ap->hostaddr, 0, ap->rusername, ap->lusername);
# endif /* WITH_RUSEROK_AF || WITH_RUSEROK */
  if (rc)
    syslog (LOG_ERR | LOG_AUTH,
	    "ruserok failed: rusername=%s, lusername=%s",
	    ap->rusername, ap->lusername);
#else /* !WITH_IRUSEROK* && !WITH_RUSEROK* */
#error Unable to use mandatory iruserok/ruserok.  This should not happen.
#endif /* !WITH_IRUSEROK* && !WITH_RUSEROK* */

  return rc;
}

#if defined KERBEROS || defined SHISHI
int
do_krb_login (int infd, struct auth_data *ap, const char **err_msg)
{
# if defined SHISHI
  int rc = SHISHI_VERIFY_FAILED;
# else /* KERBEROS */
  int rc = 1;
# endif

  *err_msg = NULL;
# if defined KRB5
  if (kerberos == AUTH_KERBEROS_5)
    rc = do_krb5_login (infd, ap, err_msg);
# elif defined SHISHI
  if (kerberos == AUTH_KERBEROS_SHISHI)
    rc = do_shishi_login (infd, ap, err_msg);
# else
  rc = do_krb4_login (infd, ap, err_msg);
# endif

  if (rc && !*err_msg)
    *err_msg = kerberos_error_string (rc);

  return rc;
}

# ifdef KRB4
int
do_krb4_login (int infd, struct auth_data *ap, const char **err_msg)
{
  int rc;
  char instance[INST_SZ], version[VERSION_SZ];
  long authopts = 0L;		/* !mutual */
  struct sockaddr_in faddr;
  unsigned char auth_buf[sizeof (AUTH_DAT)];
  unsigned char tick_buf[sizeof (KTEXT_ST)];
  Key_schedule schedule;
  AUTH_DAT *kdata;
  KTEXT ticket;
  struct passwd *pwd;

  kdata = (AUTH_DAT *) auth_buf;
  ticket = (KTEXT) tick_buf;

  instance[0] = '*';
  instance[1] = '\0';

#  ifdef ENCRYPTION
  if (encrypt_io)
    {
      rc = sizeof faddr;
      if (getsockname (0, (struct sockaddr *) &faddr, &rc))
	{
	  *err_msg = "getsockname failed";
	  syslog (LOG_ERR, "getsockname failed: %m");
	  return 1;
	}
      authopts = KOPT_DO_MUTUAL;
      rc = krb_recvauth (authopts, 0,
			 ticket, "rcmd",
			 instance, &ap->from, &faddr,
			 kdata, "", schedule, version);
      des_set_key (kdata->session, schedule);

    }
  else
#  endif
    rc = krb_recvauth (authopts, 0,
		       ticket, "rcmd",
		       instance, &ap->from, NULL, kdata, "", NULL, version);

  if (rc != KSUCCESS)
    return 1;

  getstr (infd, &ap->lusername, NULL);
  /* get the "cmd" in the rcmd protocol */
  getstr (infd, &ap->term, "TERM=");

  pwd = getpwnam (ap->lusername);
  if (pwd == NULL)
    {
      *err_msg = "getpwnam failed";
      syslog (LOG_ERR | LOG_AUTH, "getpwnam failed: %m");
      return 1;
    }
  /* returns nonzero for no access */
  if (kuserok (kdata, ap->lusername) != 0)
    return 1;

  if (pwd->pw_uid == 0)
    syslog (LOG_INFO | LOG_AUTH,
	    "ROOT Kerberos login from %s.%s@%s on %s\n",
	    kdata->pname, kdata->pinst, kdata->prealm, ap->hostname);
  else
    syslog (LOG_INFO | LOG_AUTH,
	    "%s Kerberos login from %s.%s@%s on %s\n",
	    pwd->pw_name,
	    kdata->pname, kdata->pinst, kdata->prealm, ap->hostname);

  return 0;
}
# endif /* KRB4 */

# ifdef KRB5
int
do_krb5_login (int infd, struct auth_data *ap, const char **err_msg)
{
  krb5_auth_context auth_ctx = NULL;
  krb5_error_code status;
  krb5_data inbuf;
  krb5_data version;
  krb5_authenticator *authenticator;
  krb5_principal server;
  krb5_rcache rcache;
  krb5_keyblock *key;
  krb5_ticket *ticket;
  struct sockaddr_in laddr;
  int len;
  struct passwd *pwd;

  status = krb5_init_context (&ap->context);
  if (status)
    {
      syslog (LOG_ERR, "Error initializing krb5: %s",
	      error_message (status));
      return status;
    }

  if (servername && *servername)
    {
      status = krb5_parse_name (ap->context, servername, &server);
      if (status)
	{
	  syslog (LOG_ERR, "Invalid principal '%s': %s",
		  servername, error_message (status));
	  return status;
	}

      /* A realm name missing in `servername' has been augmented
       * by krb5_parse_name(), so setting it is always harmless.
       */
      status = krb5_set_default_realm (ap->context,
				       krb5_princ_realm (ap->context,
							 server)->data);
      krb5_free_principal (ap->context, server);
      if (status)
	{
	  syslog (LOG_ERR, "Setting krb5 realm: %s",
		  error_message (status));
	  return status;
	}
    }

  if ((status = krb5_auth_con_init (ap->context, &auth_ctx))
      || (status = krb5_auth_con_genaddrs (ap->context, auth_ctx, infd,
					   KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))
      || (status = krb5_auth_con_getrcache (ap->context, auth_ctx, &rcache)))
    return status;

  if (!rcache)
    {
      status = krb5_sname_to_principal (ap->context, 0, 0, KRB5_NT_SRV_HST,
					&server);
      if (status)
	return status;

      status = krb5_get_server_rcache (ap->context,
				       krb5_princ_component (ap->context,
							     server, 0),
				       &rcache);
      krb5_free_principal (ap->context, server);

      if (status)
	return status;

      status = krb5_auth_con_setrcache (ap->context, auth_ctx, rcache);
      if (status)
	return status;
    }

  len = sizeof (laddr);
  if (getsockname (infd, (struct sockaddr *) &laddr, &len))
    return errno;

  status = krb5_recvauth (ap->context, &auth_ctx, &infd, NULL, 0,
			  0, ap->keytab, &ticket);
  if (status)
    return status;

  status = krb5_auth_con_getauthenticator (ap->context, auth_ctx,
					   &authenticator);
  if (status)
    return status;

  getstr (infd, &ap->lusername, NULL);
  getstr (infd, &ap->term, "TERM=");

  pwd = getpwnam (ap->lusername);
  if (pwd == NULL)
    {
      *err_msg = "getpwnam failed";
      syslog (LOG_ERR | LOG_AUTH, "getpwnam failed: %m");
      return 1;
    }

  getstr (infd, &ap->rusername, NULL);

  status = krb5_copy_principal (ap->context, ticket->enc_part2->client,
				&ap->client);
  if (status)
    return status;

  /*OK:: */
  if (ap->client && !krb5_kuserok (ap->context, ap->client, ap->lusername))
    return 1;

  ap->rprincipal = NULL;

  krb5_unparse_name (ap->context, ap->client, &ap->rprincipal);

  syslog (LOG_INFO | LOG_AUTH,
	  "%sKerberos V login from %s on %s\n",
	  (pwd->pw_uid == 0) ? "ROOT " : "",
	  ap->rprincipal ? ap->rprincipal : ap->rusername,
	  ap->hostname);

  return 0;
}

# endif /* KRB5 */

# ifdef SHISHI
int
do_shishi_login (int infd, struct auth_data *ad, const char **err_msg)
{
  int rc;
  int error = 0;
  int keylen, keytype;
  struct passwd *pwd = NULL;
  int cksumtype;
  char *cksum;
  char *compcksum;
  size_t compcksumlen, cksumlen = 30;
  char cksumdata[100];
  struct sockaddr_storage sock;
  socklen_t socklen = sizeof (sock);

#  ifdef ENCRYPTION
  rc = get_auth (infd, &ad->h, &ad->ap, &ad->enckey, err_msg, &ad->protocol,
		 &cksumtype, &cksum, &cksumlen, servername);
#  else
  rc = get_auth (infd, &ad->h, &ad->ap, NULL, err_msg, &ad->protocol,
		 &cksumtype, &cksum, &cksumlen, servername);
#  endif
  if (rc != SHISHI_OK)
    return rc;

#  ifdef ENCRYPTION
  /* init IV */
  if (encrypt_io)
    {
      int i;

      ad->ivtab[0] = &ad->iv1;
      ad->ivtab[1] = &ad->iv2;

      keytype = shishi_key_type (ad->enckey);
      keylen = shishi_cipher_blocksize (keytype);

      for (i = 0; i < 2; i++)
	{
	  ad->ivtab[i]->ivlen = keylen;

	  switch (keytype)
	    {
	    case SHISHI_DES_CBC_CRC:
	    case SHISHI_DES_CBC_MD4:
	    case SHISHI_DES_CBC_MD5:
	    case SHISHI_DES_CBC_NONE:
	    case SHISHI_DES3_CBC_HMAC_SHA1_KD:
	      ad->ivtab[i]->keyusage = SHISHI_KEYUSAGE_KCMD_DES;
	      ad->ivtab[i]->iv = xmalloc (ad->ivtab[i]->ivlen);
	      memset (ad->ivtab[i]->iv, i, ad->ivtab[i]->ivlen);
	      ad->ivtab[i]->ctx =
		shishi_crypto (ad->h, ad->enckey, ad->ivtab[i]->keyusage,
			       shishi_key_type (ad->enckey), ad->ivtab[i]->iv,
			       ad->ivtab[i]->ivlen);
	      break;
	    case SHISHI_ARCFOUR_HMAC:
	    case SHISHI_ARCFOUR_HMAC_EXP:
	      ad->ivtab[i]->keyusage = SHISHI_KEYUSAGE_KCMD_DES + 6 - 4 * i;
	      ad->ivtab[i]->ctx =
		shishi_crypto (ad->h, ad->enckey, ad->ivtab[i]->keyusage,
			       shishi_key_type (ad->enckey), NULL, 0);
	      break;
	    default:
	      ad->ivtab[i]->keyusage = SHISHI_KEYUSAGE_KCMD_DES + 6 - 4 * i;
	      ad->ivtab[i]->iv = xmalloc (ad->ivtab[i]->ivlen);
	      memset (ad->ivtab[i]->iv, 0, ad->ivtab[i]->ivlen);
	      if (ad->protocol == 2)
		ad->ivtab[i]->ctx =
		  shishi_crypto (ad->h, ad->enckey, ad->ivtab[i]->keyusage,
				 shishi_key_type (ad->enckey),
				 ad->ivtab[i]->iv, ad->ivtab[i]->ivlen);
	    }
	}
    }
#  endif /* ENCRYPTION */

  getstr (infd, &ad->lusername, NULL);		/* Acting user.  */
  getstr (infd, &ad->term, "TERM=");
  getstr (infd, &ad->rusername, NULL);		/* Requesting user.  */

  rc = read (infd, &error, sizeof (int));	/* XXX: not protocol */
  if ((rc != sizeof (int)) || error)
    {
      *err_msg = "Authentication exchange failed.";
      free (cksum);
      return EXIT_FAILURE;
    }

  pwd = getpwnam (ad->lusername);
  if (pwd == NULL)
    {
      *err_msg = "getpwnam failed";
      free (cksum);
      syslog (LOG_ERR | LOG_AUTH, "getpwnam failed: %m");
      return 1;
    }
  if (!allow_root && pwd->pw_uid == 0)
    {
      syslog (LOG_ERR | LOG_AUTH, "root logins are not permitted");
      fatal (infd, "Permission denied", 0);
    }

  /* verify checksum */

  if (getsockname (infd, (struct sockaddr *) &sock, &socklen) < 0)
    {
      syslog (LOG_ERR, "Can't get sock name");
      fatal (infd, "Can't get sockname", 1);
    }

  snprintf (cksumdata, sizeof (cksumdata), "%u:%s%s",
	    (sock.ss_family == AF_INET6)
	      ? ntohs (((struct sockaddr_in6 *) &sock)->sin6_port)
	      : ntohs (((struct sockaddr_in *) &sock)->sin_port),
	    ad->term + 5, ad->lusername);
  rc = shishi_checksum (ad->h, ad->enckey, 0, cksumtype, cksumdata,
			strlen (cksumdata), &compcksum, &compcksumlen);
  if (rc != SHISHI_OK
      || compcksumlen != cksumlen
      || memcmp (compcksum, cksum, cksumlen) != 0)
    {
      *err_msg = "Authentication exchange failed.";
      syslog (LOG_ERR, "checksum verify failed: %s", shishi_error (ad->h));
      free (cksum);
      free (compcksum);
      return rc;
    }

  free (cksum);
  free (compcksum);

  rc = shishi_authorized_p (ad->h, shishi_ap_tkt (ad->ap), ad->lusername);
  if (!rc)
    {
      syslog (LOG_ERR | LOG_AUTH,
	      "User %s@%s is not authorized to log in as: %s.",
	      ad->rusername, ad->hostname, ad->lusername);
      shishi_ap_done (ad->ap);
      rlogind_error (infd, 0, "Failed to get authorized as `%s'.\n",
		     ad->lusername);
      return rc;
    }

  rc = shishi_encticketpart_clientrealm (ad->h,
			shishi_tkt_encticketpart (shishi_ap_tkt (ad->ap)),
			&ad->rprincipal, NULL);
  if (rc != SHISHI_OK)
    ad->rprincipal = NULL;

  shishi_ap_done (ad->ap);

#  ifdef WITH_PAM
  rc = do_pam_check (infd, ad, "krlogin");
  if (rc != PAM_SUCCESS)
    {
      *err_msg = "Permission denied";
      return rc;
    }
#  endif /* WITH_PAM */

  syslog (LOG_INFO | LOG_AUTH,
	  "Kerberos V %slogin from %s on %s as `%s'.\n",
	  ENCRYPT_IO ? "encrypted " : "",
	  ad->rprincipal ? ad->rprincipal : ad->rusername,
	  ad->hostname,
	  ad->lusername);

  return SHISHI_OK;
}
# endif /* SHISHI */
#endif /* KERBEROS || SHISHI */

#define BUFFER_SIZE 128

void
getstr (int infd, char **ptr, const char *prefix)
{
  char c;
  char *buf;
  int pos;
  int size = BUFFER_SIZE;

  if (prefix)
    {
      int len = strlen (prefix);

      if (size < len + 1)
	size = len + 1;
    }

  buf = malloc (size);
  if (!buf)
    {
      syslog (LOG_ERR, "not enough memory");
      exit (EXIT_FAILURE);
    }

  pos = 0;
  if (prefix)
    {
      strcpy (buf, prefix);
      pos += strlen (buf);
    }

  do
    {
      if (read (infd, &c, 1) != 1)
	{
	  syslog (LOG_ERR, "read error: %m");
	  exit (EXIT_FAILURE);
	}
      if (pos == size)
	{
	  size += BUFFER_SIZE;
	  buf = realloc (buf, size);
	  if (!buf)
	    {
	      syslog (LOG_ERR, "not enough memory");
	      exit (EXIT_FAILURE);
	    }
	}
      buf[pos++] = c;
    }
  while (c != 0);
  *ptr = buf;
}

#define pkcontrol(c) ((c)&(TIOCPKT_FLUSHWRITE|TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))

char magic[2] = { 0377, 0377 };
char oobdata[] = { TIOCPKT_WINDOW };	/* May be modified by protocol/control */

#ifdef SHISHI
char oobdata_new[] = { 0377, 0377, 'o', 'o', TIOCPKT_WINDOW };
#endif

#ifdef SHISHI_ENCRYPT_BUFLEN
# undef BUFLEN
# define BUFLEN SHISHI_ENCRYPT_BUFLEN
#elif !defined BUFLEN
# define BUFLEN 1024
#endif

void
protocol (int f, int p, struct auth_data *ap)
{
  char fibuf[BUFLEN], *pbp = NULL, *fbp = NULL;
  int pcc = 0, fcc = 0;
  int cc, n;
  unsigned int nfd;
  char cntl;

#ifndef SHISHI
  (void) ap;		/* Silence warning.  */
#endif
  /*
   * Must ignore SIGTTOU, otherwise we'll stop
   * when we try and set slave pty's window shape
   * (our controlling tty is the master pty).
   */
  setsig (SIGTTOU, SIG_IGN);
#ifdef SHISHI
  if (kerberos && (ap->protocol == 2))
    {
      ENC_WRITE (n, f, oobdata_new, 5, ap);
    }
  else
#endif
    send (f, oobdata, 1, MSG_OOB);	/* indicate new rlogin */
  if (f > p)
    nfd = f + 1;
  else
    nfd = p + 1;
  if (nfd > FD_SETSIZE)
    {
      syslog (LOG_ERR, "select mask too small, increase FD_SETSIZE");
      fatal (f, "internal error (select mask too small)", 0);
    }

  while (1)
    {
      fd_set ibits, obits, ebits, *omask;

      FD_ZERO (&ebits);
      FD_ZERO (&ibits);
      FD_ZERO (&obits);
      omask = (fd_set *) NULL;

      if (fcc)
	{
	  FD_SET (p, &obits);
	  omask = &obits;
	}
      else
	FD_SET (f, &ibits);

      if (pcc >= 0)
	{
	  if (pcc)
	    {
	      FD_SET (f, &obits);
	      omask = &obits;
	    }
	  else
	    FD_SET (p, &ibits);
	}

      FD_SET (p, &ebits);

      n = select (nfd, &ibits, omask, &ebits, 0);
      if (n < 0)
	{
	  if (errno == EINTR)
	    continue;
	  fatal (f, "select", 1);
	}
      if (n == 0)
	{
	  /* shouldn't happen... */
	  sleep (5);
	  continue;
	}

      if (FD_ISSET (p, &ebits))
	{
	  cc = read (p, &cntl, 1);
	  if (cc == 1 && pkcontrol (cntl))
	    {
	      cntl |= oobdata[0];
	      send (f, &cntl, 1, MSG_OOB);
	      if (cntl & TIOCPKT_FLUSHWRITE)
		{
		  pcc = 0;
		  FD_CLR (p, &ibits);
		}
	    }
	}

      if (FD_ISSET (f, &ibits))
	{
	  ENC_READ (fcc, f, fibuf, sizeof (fibuf), ap);

	  if (fcc < 0 && errno == EWOULDBLOCK)
	    fcc = 0;
	  else
	    {
	      register char *cp;
	      int left;

	      if (fcc <= 0)
		break;
	      fbp = fibuf;

	      for (cp = fibuf; cp < fibuf + fcc - 1; cp++)
		if (cp[0] == magic[0] && cp[1] == magic[1])
		  {
		    int len;

		    left = fcc - (cp - fibuf);
		    len = control (p, cp, left);
		    if (len)
		      {
			left -= len;
			if (left > 0)
			  memmove (cp, cp + len, left);
			fcc -= len;
			cp--;
		      }
		  }
	      FD_SET (p, &obits);	/* try write */
	    }
	}

      if (FD_ISSET (p, &obits) && fcc > 0)
	{
	  cc = write (p, fbp, fcc);
	  if (cc > 0)
	    {
	      fcc -= cc;
	      fbp += cc;
	    }
	}

      if (FD_ISSET (p, &ibits))
	{
	  char dbuf[BUFLEN + 1];

	  pcc = read (p, dbuf, sizeof dbuf);

	  pbp = dbuf;
	  if (pcc < 0)
	    {
	      if (errno == EWOULDBLOCK)
		pcc = 0;
	      else
		break;
	    }
	  else if (pcc == 0)
	    {
	      break;
	    }
	  else if (dbuf[0] == 0)
	    {
	      pbp++;
	      pcc--;
	      IF_NOT_ENCRYPT (FD_SET (f, &obits));	/* try write */
	    }
	  else
	    {
	      if (pkcontrol (dbuf[0]))
		{
		  dbuf[0] |= oobdata[0];
		  send (f, &dbuf[0], 1, MSG_OOB);
		}
	      pcc = 0;
	    }
	}

      if ((FD_ISSET (f, &obits)) && pcc > 0)
	{
	  ENC_WRITE (cc, f, pbp, pcc, ap);

	  if (cc < 0 && errno == EWOULDBLOCK)
	    {
	      /*
	       * This happens when we try write after read
	       * from p, but some old kernels balk at large
	       * writes even when select returns true.
	       */
	      if (!FD_ISSET (p, &ibits))
		sleep (5);
	      continue;
	    }
	  if (cc > 0)
	    {
	      pcc -= cc;
	      pbp += cc;
	    }
	}
    }
}

/* Handle a "control" request (signaled by magic being present)
   in the data stream.  For now, we are only willing to handle
   window size changes. */
int
control (int pty, char *cp, size_t n)
{
  struct winsize w;

  if (n < 4 + sizeof (w) || cp[2] != 's' || cp[3] != 's')
    return (0);
  oobdata[0] &= ~TIOCPKT_WINDOW;	/* we know he heard */
  memmove (&w, cp + 4, sizeof w);
  w.ws_row = ntohs (w.ws_row);
  w.ws_col = ntohs (w.ws_col);
  w.ws_xpixel = ntohs (w.ws_xpixel);
  w.ws_ypixel = ntohs (w.ws_ypixel);
  ioctl (pty, TIOCSWINSZ, &w);
  return (4 + sizeof w);
}

void
cleanup (int signo)
{
  int status = EXIT_FAILURE;
  char *p;

  if (signo == SIGCHLD)
    {
      (void) waitpid ((pid_t) -1, &status, WNOHANG);

      status = WEXITSTATUS (status);
    }

  p = line + sizeof (PATH_TTY_PFX) - 1;
#if !defined HAVE_LOGOUT || !defined HAVE_LOGWTMP
  utmp_logout (p);
  chmod (line, 0644);
  chown (line, 0, 0);
#else
  if (logout (p))
    logwtmp (p, "", "");
  chmod (line, 0666);
  chown (line, 0, 0);
  *p = 'p';
  chmod (line, 0666);
  chown (line, 0, 0);
#endif
  shutdown (netf, 2);
  exit (status);
}

int
in_local_domain (char *hostname)
{
  char *p = topdomain (hostname, local_dot_count);

  return p && strcasecmp (p, local_domain_name) == 0;
}

char *
topdomain (char *name, int max_dots)
{
  char *p;
  int dot_count = 0;

  for (p = name + strlen (name) - 1; p >= name; p--)
    {
      if (*p == '.' && ++dot_count == max_dots)
	return p + 1;
    }
  return name;
}

void
rlogind_error (int f, int syserr, const char *msg, ...)
{
  int len;
  char buf[BUFSIZ], buf2[BUFSIZ], *bp = buf;
  va_list ap;
  va_start (ap, msg);

  /*
   * Error message proper, with variadic parts.
   */
  vsnprintf (buf2, sizeof (buf2) - 1, msg, ap);
  va_end (ap);

  /*
   * Prepend binary one to message if we haven't sent
   * the magic null as confirmation.
   */
  if (!confirmed)
    *bp++ = '\01';		/* error indicator */
  if (syserr)
    snprintf (bp, sizeof buf - (bp - buf),
	      "rlogind: %s: %s.\r\n", buf2, strerror (errno));
  else
    snprintf (bp, sizeof buf - (bp - buf), "rlogind: %s\r\n", buf2);

  len = strlen (bp);
  write (f, buf, bp + len - buf);
}

void
fatal (int f, const char *msg, int syserr)
{
  rlogind_error (f, syserr, (msg && *msg) ? msg : "unspecified error");
#ifdef WITH_PAM
  if (pam_handle)
    pam_end (pam_handle, pam_rc);
#endif

  exit (EXIT_FAILURE);
}

#ifdef WITH_PAM
int
do_pam_check (int infd, struct auth_data *ap, const char *service)
{
  char *user;
  struct passwd *pwd;

  pam_rc = pam_start (service, ap->lusername, &pam_conv, &pam_handle);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_RHOST, ap->hostname);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_RUSER, ap->rusername);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_set_item (pam_handle, PAM_TTY, service);
  if (pam_rc == PAM_SUCCESS)
    pam_rc = pam_get_item (pam_handle, PAM_USER, (const void **) &user);
  if (pam_rc != PAM_SUCCESS)
    {
      if (pam_handle)
	{
	  pam_end (pam_handle, pam_rc);
	  pam_handle = NULL;
	}

      /* Failed set-up is deemed serious.  Abort!  */
      syslog (LOG_ERR | LOG_AUTH, "PAM set-up failed.");
      fatal (infd, "Permission denied", 0);
    }

  pam_rc = pam_authenticate (pam_handle, PAM_SILENT);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_ABORT:
	  /* Serious enough to merit immediate abortion.  */
	  pam_end (pam_handle, pam_rc);
	  syslog (LOG_ERR | LOG_AUTH, "PAM authentication said PAM_ABORT.");
	  exit (EXIT_FAILURE);

	case PAM_NEW_AUTHTOK_REQD:
	  pam_rc = pam_chauthtok (pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	  if (pam_rc == PAM_SUCCESS)
	    pam_rc = pam_authenticate (pam_handle, PAM_SILENT);
	  break;

	default:
	  break;			/* Non-zero status.  */
	}
    }

  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_NOTICE | LOG_AUTH,
	      "PAM authentication of '%s' from %s(%s): %s",
	      user, ap->hostname, ap->hostaddr,
	      pam_strerror (pam_handle, pam_rc));
      pam_end (pam_handle, pam_rc);
      pam_handle = NULL;

      return pam_rc;
    }

  pam_rc = pam_acct_mgmt (pam_handle, PAM_SILENT);
  if (pam_rc != PAM_SUCCESS)
    {
      switch (pam_rc)
	{
	case PAM_NEW_AUTHTOK_REQD:
	  pam_rc = pam_chauthtok (pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	  if (pam_rc == PAM_SUCCESS)
	    pam_rc = pam_acct_mgmt (pam_handle, PAM_SILENT);
	  break;

	case PAM_ACCT_EXPIRED:
	case PAM_AUTH_ERR:
	case PAM_PERM_DENIED:
	case PAM_USER_UNKNOWN:
	default:
	  break;			/* Non-zero status.  */
	}
    }

  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_INFO | LOG_AUTH,
	      "PAM accounting of '%s' from %s(%s): %s",
	      user, ap->hostname, ap->hostaddr,
	      pam_strerror (pam_handle, pam_rc));
      pam_end (pam_handle, pam_rc);
      pam_handle = NULL;

      return pam_rc;
    }

  /* Renew client information, since the PAM stack may have
   * mapped the request onto another identity.
   */
  pam_rc = pam_get_item (pam_handle, PAM_USER, (const void **) &user);
  if (pam_rc != PAM_SUCCESS)
    {
      syslog (LOG_NOTICE | LOG_AUTH, "pam_get_item(PAM_USER): %s",
	      pam_strerror (pam_handle, pam_rc));
      user = "<invalid>";
    }

  pwd = getpwnam (user);
  free (ap->lusername);
  ap->lusername = xstrdup (user);

  if (pwd == NULL)
    {
      syslog (LOG_INFO | LOG_AUTH, "%s@%s as %s: unknown login.",
	      ap->rusername, ap->hostname, ap->lusername);
      pam_rc = PAM_AUTH_ERR;		/* Non-zero status.  */
    }

  pam_end (pam_handle, pam_rc);		/* PAM access is complete.  */
  pam_handle = NULL;

  return pam_rc;
}

/* Call back function for passing user's information
 * to any PAM module requesting this information.
 */
static int
rlogin_conv (int num, const struct pam_message **pam_msg,
	     struct pam_response **pam_resp,
	     void *data _GL_UNUSED_PARAMETER)
{
  struct pam_response *resp;

  /* Reject composite calls at the time being.  */
  if (num <= 0 || num > 1)
    return PAM_CONV_ERR;

  /* Ensure an empty response.  */
  *pam_resp = NULL;

  switch ((*pam_msg)->msg_style)
    {
    case PAM_PROMPT_ECHO_OFF:
      /* Return an empty password.  */
      resp = (struct pam_response *) malloc (sizeof (*resp));
      if (!resp)
	return PAM_BUF_ERR;
      resp->resp_retcode = 0;
      resp->resp = strdup ("");
      if (!resp->resp)
	{
	  free (resp);
	  return PAM_BUF_ERR;
	}
      syslog (LOG_NOTICE | LOG_AUTH, "PAM message \"%s\".",
	      (*pam_msg)->msg);
      *pam_resp = resp;
      return PAM_SUCCESS;
      break;

    case PAM_TEXT_INFO:		/* Would break protocol.  */
    case PAM_ERROR_MSG:		/* Likewise.  */
    case PAM_PROMPT_ECHO_ON:	/* Interactivity is not supported.  */
    default:
      return PAM_CONV_ERR;
    }
  return PAM_CONV_ERR;		/* Never reached.  */
}
#endif /* WITH_PAM */
