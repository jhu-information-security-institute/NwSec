/*
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
  2009, 2010, 2011, 2012, 2013, 2014, 2015 Free Software Foundation, Inc.

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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <unused-parameter.h>
#include "extern.h"

#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#endif

/*
 * Mechanisms and prerequisites.
 *
 * The original code was tailored to rely on the side-effects
 * of the Linux-PAM module `pam_ftp.so', with its peculiarities:
 *
 *  1. If PAM_USER is `ftp' or `anonymous', fetch PAM_AUTHTOK
 *     and split it at `@' into PAM_RUSER and PAM_RHOST.
 *     Return with success and set PAM_USER to `ftp'.
 *
 *  2. For other values of PAM_USER, keep the gotten PAM_AUTHTOK
 *     unchanged, and fail.
 *
 * This module `pam_ftp.so' does not exist in OpenPAM, nor in
 * Solaris-PAM.  Thus portability requires some care.
 */

/* June 3rd, 2012:
 * The draft of A.G Morgan on behalf of the the Open-PAM
 * working group has clearly not been able to get the
 * additions PAM_INCOMPLETE and PAM_CONV_AGAIN accepted
 * sufficiently well in order that the present code should
 * force them upon BSD and Solaris.  They are thus protected
 * by preprocessor conditionals for the time being.
 */

#ifdef WITH_PAM

/* PAM authentication, now using the PAM's async feature.  */
static pam_handle_t *pamh;

static int PAM_conv (int num_msg, const struct pam_message **msg,
		     struct pam_response **resp, void *appdata_ptr);

static struct pam_conv PAM_conversation = { &PAM_conv, NULL };

static int
PAM_conv (int num_msg, const struct pam_message **msg,
	  struct pam_response **resp, void *appdata_ptr)
{
  struct pam_response *repl = NULL;
  int retval, count = 0, replies = 0;
  int size = sizeof (struct pam_response);
  struct credentials *pcred = (struct credentials *) appdata_ptr;

# define GET_MEM \
        if (!(repl = realloc (repl, size))) \
                return PAM_BUF_ERR; \
        size += sizeof (struct pam_response)

  retval = PAM_SUCCESS;

  for (count = 0; count < num_msg; count++)
    {
      int savemsg = 0;

      switch (msg[count]->msg_style)
	{
	case PAM_PROMPT_ECHO_ON:
	  GET_MEM;
	  repl[replies].resp_retcode = PAM_SUCCESS;
	  repl[replies].resp = sgetsave (pcred->name);
	  replies++;
	  break;
	case PAM_PROMPT_ECHO_OFF:
	  GET_MEM;
	  if (pcred->pass == NULL)
	    {
	      savemsg = 1;
# ifdef PAM_CONV_AGAIN
	      retval = PAM_CONV_AGAIN;		/* Linux-PAM */
# elif !defined WITH_LINUX_PAM
	      /*
	       * Simulate PAM_CONV_AGAIN.
	       * The alternate value PAM_TRY_AGAIN is not
	       * an expected return value here, so it will
	       * leave an audit trail.  */
	      retval = PAM_CRED_INSUFFICIENT;
# else /* !PAM_CONV_AGAIN && !WITH_LINUX_PAM */
	      retval = PAM_CONV_ERR;
# endif
	    }
	  else
	    {
	      repl[replies].resp_retcode = PAM_SUCCESS;
	      repl[replies].resp = sgetsave (pcred->pass);
	      replies++;
	    }
	  break;
	case PAM_TEXT_INFO:
	  savemsg = 1;
	  break;
	case PAM_ERROR_MSG:
	default:
	  /* Must be an error of some sort... */
	  savemsg = 1;
	  retval = PAM_CONV_ERR;
	}

      if (savemsg)
	{
	  /* FIXME:  This is a serious problem.  If the PAM message
	     is multilines, the reply _must_ be formated correctly.
	     The way to do this would be to consider \n as a boundary then
	     in the ftpd.c:user() or ftpd.c:pass() check for it and send
	     a lreply().  But I'm not sure the RFCs allow mutilines replies
	     for a passwd challenge.  Many clients will simply break.  */
	  /* XXX: Attempted solution; collect all messages, appended
	   * one after the other, separated by "\n".  Then print all
	   * of them in one single run.  This will circumvent the hard
	   * coded protocol convention of not allowing continuation
	   * massage to carry a deviating reply code relative to the
	   * final message.
	   */
	  if (pcred->message)	/* XXX: make sure we split newlines correctly */
	    {
	      size_t len = strlen (pcred->message);
	      char *s = realloc (pcred->message, len
				 + strlen (msg[count]->msg) + 2);
	      if (s == NULL)
		{
		  free (pcred->message);
		  pcred->message = NULL;
		}
	      else
		{
		  pcred->message = s;
		  strcat (pcred->message, "\n");
		  strcat (pcred->message, msg[count]->msg);
		}
	    }
	  else
	    pcred->message = sgetsave (msg[count]->msg);

	  if (pcred->message == NULL)
	    retval = PAM_CONV_ERR;
	  else
	    {
	      char *sp;
	      /* Remove trailing space only.  */
	      sp = pcred->message + strlen (pcred->message);
	      while (sp > pcred->message && strchr (" \t\n", *--sp))
		*sp = '\0';
	    }
	}

      /* In case of error, drop responses and return */
      if (retval)
	{
	  /* FIXME: drop_reply is not standard, need to clean this.  */
	  //_pam_drop_reply (repl, replies);
	  free (repl);
	  return retval;
	}
    }
  if (repl)
    *resp = repl;
  return PAM_SUCCESS;
}

/* Non-zero return status means failure. */
static int
pam_doit (struct credentials *pcred)
{
  char *username;
  int error;

  error = pam_authenticate (pamh, 0);

  /* Probably being called with empty passwd.  */
  if (0
# ifdef PAM_CONV_AGAIN
      || error == PAM_CONV_AGAIN
# endif
# ifdef PAM_INCOMPLETE
      || error == PAM_INCOMPLETE
# endif
# ifndef WITH_LINUX_PAM
      /*
       * In absence of `pam_ftp.so', catch the simulated,
       * incomplete state reported by our PAM_conv().
       */
      || (error == PAM_CRED_INSUFFICIENT && pcred->pass == NULL)
# endif /* !WITH_LINUX_PAM */
     )
    {
      /* Avoid overly terse passwd messages and let the people
         upstairs do something sane.  */
      if (pcred->message && !strcasecmp (pcred->message, "password:"))
	{
	  free (pcred->message);
	  pcred->message = NULL;
	}
      return PAM_SUCCESS;
    }

  if (error == PAM_SUCCESS)	/* Alright, we got it */
    {
      error = pam_acct_mgmt (pamh, 0);
      if (error == PAM_SUCCESS)
	error = pam_setcred (pamh, PAM_ESTABLISH_CRED);
      if (error == PAM_SUCCESS)
	error = pam_open_session (pamh, 0);
      if (error == PAM_SUCCESS)
	error = pam_get_item (pamh, PAM_USER, (const void **) &username);
      if (error == PAM_SUCCESS)
	{
	  if (sgetcred (username, pcred) != 0)
	    error = PAM_AUTH_ERR;
	  else
	    {
	      if (strcasecmp (username, "ftp") == 0)
		pcred->guest = 1;
	    }
	}
    }
  if (error != PAM_SUCCESS)
    {
      pam_end (pamh, error);
      pamh = NULL;
    }

  return (error != PAM_SUCCESS);
}

#define TTY_FORMAT "/dev/ftp%d"

/* Non-zero return means failure. */
int
pam_user (const char *username, struct credentials *pcred)
{
  int error;
  char tty_name[strlen (TTY_FORMAT) + strlen ("9999999")];

  if (pamh != NULL)
    {
      pam_end (pamh, PAM_ABORT);
      pamh = NULL;
    }

  free (pcred->name);
  pcred->name = strdup (username);
  free (pcred->message);
  pcred->message = NULL;

  (void) snprintf (tty_name, sizeof (tty_name), TTY_FORMAT, (int) getpid());

  /* Arrange our creditive.  */
  PAM_conversation.appdata_ptr = (void *) pcred;

  error = pam_start ("ftp", pcred->name, &PAM_conversation, &pamh);
  if (error == PAM_SUCCESS)
    error = pam_set_item (pamh, PAM_RHOST, pcred->remotehost);
  if (error == PAM_SUCCESS)
    error = pam_set_item (pamh, PAM_TTY, tty_name);
  if (error != PAM_SUCCESS)
    {
      pam_end (pamh, error);
      pamh = 0;
    }

  if (pamh)
    error = pam_doit (pcred);

  return (error != PAM_SUCCESS);
}

/* Nonzero value return for error.  */
int
pam_pass (const char *passwd, struct credentials *pcred)
{
  int error = PAM_AUTH_ERR;
  if (pamh)
    {
      pcred->pass = (char *) passwd;
      error = pam_doit (pcred);
      pcred->pass = NULL;
    }
  return error != PAM_SUCCESS;
}

void
pam_end_login (struct credentials * pcred _GL_UNUSED_PARAMETER)
{
  int error;

  if (pamh)
    {
      error = pam_close_session (pamh, PAM_SILENT);
      if (logging && error != PAM_SUCCESS)
	syslog (LOG_ERR, "pam_session: %s", pam_strerror (pamh, error));

      error = pam_setcred (pamh, PAM_SILENT | PAM_DELETE_CRED);
      if (logging && error != PAM_SUCCESS)
	syslog (LOG_ERR, "pam_setcred: %s", pam_strerror (pamh, error));

      error = pam_end (pamh, error);
      if (logging && error != PAM_SUCCESS)
	syslog (LOG_ERR, "pam_end: %s", pam_strerror (pamh, error));

      pamh = NULL;
    }
}
#endif /* WITH_PAM */
