/************************************************************************
 *   IRC - Internet Relay Chat. m_smo.c module - recieve global oper messages
 *   Copyright (C) 2001 NeoIRCd Development Team
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
 *   $Id: m_smo.c,v 1.1 2002/09/13 16:30:03 fishwaldo Exp $
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


/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */
static void ms_smo(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */
struct Message smo_msgtab = {
  "SMO", 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_smo, m_ignore}
};

/* Thats the msgtab finished */

#ifndef STATIC_MODULES
/* Here we tell it what to do when the module is loaded */
void
_modinit(void)
{
  /* This will add the commands in test_msgtab (which is above) */
  mod_add_cmd(&smo_msgtab);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&smo_msgtab);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.1 $";
#endif

/*
 * m_smo
 * 	process a global oper message
 *      parv[0] = sender prefix (Server)
 *      parv[1] = flags for realops (Contains FLAGS_REMOTE.. be carefull)
 *      parv[2] = level for realops 
 *      parv[3] = actual message
 */
static void ms_smo(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	int flags;

	flags = atoi(parv[1]);
	/* this is just a double check for servers that *DONT* set the right flags */
	flags &= ~FLAGS_REMOTE;

	/* if its not from a server, or a Services Client drop the message */
	if (!IsServer(source_p))
		return;
	if (IsClient(source_p) && (!IsServices(source_p)))
		return;
	
	printf("gothere %x %d\n", flags, flags);
	sendto_realops_flags(flags, atoi(parv[2]), "From %s: %s", source_p->name, parv[3]);

	sendto_server(client_p, NULL, NULL, NOCAPS, NOCAPS, NOFLAGS, ":%s SMO %s %s :%s", source_p->name, parv[1], parv[2], parv[3]);
}

