/************************************************************************
 *   IRC - Internet Relay Chat. m_vhost.c module
 *   Copyright (C) 2002 NeoIRCd development Team
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
 *   $Id: m_vhost.c,v 1.2 2002/09/13 06:54:34 fishwaldo Exp $
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "common.h"     /* FALSE bleah */
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_log.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hash.h"
#include "whowas.h"


/* define who hostserv is on your network */
#define HOSTSERV "HostServ"


/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */
static void m_vhost(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);


/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */
struct Message vhost_msgtab = {
  "VHOST", 0, 0, 3, 3, MFLG_SLOW, 0,
  {m_ignore, m_vhost, m_ignore, m_vhost}
};

/* Thats the msgtab finished */

#ifndef STATIC_MODULES
/* Here we tell it what to do when the module is loaded */
void
_modinit(void)
{
  /* This will add the commands in test_msgtab (which is above) */
  mod_add_cmd(&vhost_msgtab);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&vhost_msgtab);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.2 $";
#endif

/*
 * m_vhost
 *	Changes the Targets Virtual Hostname, and also sets +x if its not set already on the target
 *      parv[0] = sender prefix
 *      parv[1] = username
 *      parv[2] = password
 */
static void m_vhost(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	struct Client *target_p;
	

	
	target_p = find_person(HOSTSERV);

	/* if we found the target and its *IS* set a services, send the message to it, otherwise services are not online! */

	if ((target_p != NULL) && (IsServices(target_p))) {
		ilog(L_WARN, "vhost: Found Target %s", target_p->name);
		sendto_one(target_p, ":%s PRIVMSG %s!%s@%s :login %s %s", client_p->name, target_p->name, target_p->username, target_p->host, parv[1], parv[2]);	
	} else {
		sendto_one(source_p, ":%s 440 %s %s :Services are currently down. Please try again later.", me.name, source_p->name, HOSTSERV);
		/* we couldn't find the target. Just exit */
		return;
	}
}    
	  
