/************************************************************************
 *   IRC - Internet Relay Chat, src/ircd_signal.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: ircd_signal.c,v 1.4 2002/10/23 03:53:22 fishwaldo Exp $
 */

#include "stdinc.h"
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include "ircd_signal.h"
#include "ircd.h"         /* dorehash */
#include "restart.h"      /* server_reboot */
#include "s_log.h"
#include "memory.h"
#include "s_bsd.h"
#include "send.h"
#include "client.h"

/*
 * dummy_handler - don't know if this is really needed but if alarm is still
 * being used we probably will
 */ 
static void 
dummy_handler(int sig)
{
  /* Empty */
}

/*
 * sigterm_handler - exit the server
 */
static void 
sigterm_handler(int sig)  
{
  /* XXX we had a flush_connections() here - we should close all the
   * connections and flush data. read server_reboot() for my explanation.
   *     -- adrian
   */
  ilog(L_CRIT, "Server killed By SIGTERM");
  exit(-1);
}
  
/* 
 * sighup_handler - reread the server configuration
 */
static void 
sighup_handler(int sig)
{
  dorehash = 1;
}

/*
 * sigint_handler - restart the server
 */
static void 
sigint_handler(int sig)
{
  static int restarting = 0;

  if (server_state.foreground) 
    {
      ilog(L_WARN, "Server exiting on SIGINT");
      exit(0);
    }
  else
    {
      ilog(L_WARN, "Server Restarting on SIGINT");
      if (restarting == 0) 
        {
          restarting = 1;
          server_reboot();
        }
    }
}

static void
sigsegv_handler(int sig)
{
#ifdef HAVE_BACKTRACE
 	void *array[50];
     	size_t size;
        char **strings;
        size_t i;
/* thanks to gnulibc libary for letting me find this usefull function */
	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);
#endif
	ilog(L_ERROR, "Damn It, Server is cracking a Woobly... Crashing... ");
#ifdef HAVE_BACKTRACE
		for (i = size; i == 0; i--) {
			ilog(L_ERROR, "BackTrace(%d): %s", i-1, strings[i]);
		}
#endif 
	ilog(L_ERROR, "Attempting To Restart");
	sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Damn It, Server is Cracking a Woobly... Crashing...");
#ifdef HAVE_BACKTRACE
	sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Location *Could* be: %s", strings[size-1]);
	sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Check LogFile for More Details");
	MyFree(strings);
#endif
	server_reboot();

}


/*
 * setup_signals - initialize signal handlers for server
 */
void 
setup_signals()
{
  struct sigaction act;

  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGPIPE);
  sigaddset(&act.sa_mask, SIGALRM);
  sigaddset(&act.sa_mask, SIGTRAP);

# ifdef SIGWINCH
  sigaddset(&act.sa_mask, SIGWINCH);
  sigaction(SIGWINCH, &act, 0);
# endif
  sigaction(SIGPIPE, &act, 0);
  sigaction(SIGTRAP, &act, 0);

  act.sa_handler = dummy_handler;
  sigaction(SIGALRM, &act, 0);

  act.sa_handler = sighup_handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGHUP);
  sigaction(SIGHUP, &act, 0);

  act.sa_handler = sigint_handler;
  sigaddset(&act.sa_mask, SIGINT);
  sigaction(SIGINT, &act, 0);

  act.sa_handler = sigterm_handler;
  sigaddset(&act.sa_mask, SIGTERM);
  sigaction(SIGTERM, &act, 0);

  act.sa_handler = sigsegv_handler;
  sigaddset(&act.sa_mask, SIGSEGV);
  sigaction(SIGSEGV, &act, 0);
}


