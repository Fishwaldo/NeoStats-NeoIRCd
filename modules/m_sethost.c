/************************************************************************
 *   IRC - Internet Relay Chat, m_sethost.c
 *   Copyright (C) 2002 NeoIRCd Development Team
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
 *   $Id: m_sethost.c,v 1.3 2002/09/13 06:54:34 fishwaldo Exp $
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


/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */
static void ms_sethost(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */
struct Message sethost_msgtab = {
  "SETHOST", 0, 0, 3, 3, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_sethost, m_ignore}
};

/* Thats the msgtab finished */

#ifndef STATIC_MODULES
/* Here we tell it what to do when the module is loaded */
void
_modinit(void)
{
  /* This will add the commands in test_msgtab (which is above) */
  mod_add_cmd(&sethost_msgtab);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&sethost_msgtab);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.3 $";
#endif

/*
 * ms_sethost
 *	Changes the Targets Virtual Hostname, and also sets +x if its not set already on the target
 *      parv[0] = sender prefix
 *      parv[1] = target
 *      parv[2] = new hostname
 */
static void ms_sethost(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	struct Client *target_p;

	
	target_p = find_person(parv[1]);
	/* first find the target that we want to change */
	if (target_p != NULL) {
		ilog(L_WARN, "Found Target %s", target_p->name);
		
		if (IsServer(source_p)) {
			
			/* client is changing his own hostname */
			ilog(L_WARN, "Target is source");
			
			/* check its not a client on my server, because this is a error then */
			/* use svshost instead. */
			if (MyClient(target_p)) {
				ilog(L_WARN, "Target is my client?");
				return;
			}
			ilog(L_WARN, "Setting his own hostname %s", target_p->name);

			SetHidden(target_p);		
			strncpy(target_p->vhost, parv[2], HOSTLEN);
		
			/* send it to the rest of the net */
			sendto_server(source_p, source_p, NULL, 0, 0, LL_ICLIENT, ":%s SETHOST %s :%s", me.name, source_p->name, source_p->vhost);
	
			return;
		
		} else if (IsClient(source_p)) {
			/* can't change someone else's host. (services use svshost) */
			sendto_one(source_p, form_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return;
		} else 
		ilog(L_WARN, "shouldn't be here");
	} else {
		ilog(L_WARN, "Couldn't find target %s", parv[1]);
		/* we couldn't find the target. Just exit */
		return;
	}
}    
	  



