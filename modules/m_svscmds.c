/************************************************************************
 *   IRC - Internet Relay Chat, doc/example_module.c
 *   Copyright (C) 2001 Hybrid Development Team
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
 *   $Id: m_svscmds.c,v 1.2 2002/09/12 05:45:20 fishwaldo Exp $
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
static void ms_svshost(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void ms_svsnick(struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[]);

static void ms_svsid(struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[]);


static int clean_nick_name(char *);

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */
struct Message svshost_msgtab = {
  "SVSHOST", 0, 0, 3, 3, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_svshost, m_ignore}
};

struct Message svsnick_msgtab = {
  "SVSNICK", 0, 0, 3, 3, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_svsnick, m_ignore}
};


struct Message svsid_msgtab = {
  "SVSID", 0, 0, 3, 3, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_svsid, m_ignore}
};

/* Thats the msgtab finished */

#ifndef STATIC_MODULES
/* Here we tell it what to do when the module is loaded */
void
_modinit(void)
{
  /* This will add the commands in test_msgtab (which is above) */
  mod_add_cmd(&svshost_msgtab);
  mod_add_cmd(&svsnick_msgtab);
  mod_add_cmd(&svsid_msgtab);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&svshost_msgtab);
  mod_del_cmd(&svsnick_msgtab);
  mod_del_cmd(&svsid_msgtab);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.2 $";
#endif

/*
 * ms_svshost
 *	Changes the Targets Virtual Hostname, and also sets +x if its not set already on the target
 *      parv[0] = sender prefix
 *      parv[1] = target
 *      parv[2] = new hostname
 */
static void ms_svshost(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	struct Client *target_p;

	
	target_p = find_person(parv[1]);
	/* first find the target that we want to change */
	if (target_p != NULL) {
		ilog(L_WARN, "svshost: Found Target %s", target_p->name);
		
		if (IsServer(source_p) && IsUlined(source_p)) {
			
			ilog(L_WARN, "svshost: Setting his own hostname %s (from %s)", target_p->name, client_p->name);

			SetHidden(target_p);		
			strncpy(target_p->vhost, parv[2], HOSTLEN);
		
			/* send it to the rest of the net */
			sendto_server(client_p, target_p, NULL, 0, 0, LL_ICLIENT, ":%s SVSHOST %s :%s", source_p->name, target_p->name, target_p->vhost);
	
			return;
		
		} else {
			sendto_realops_flags(FLAGS_ALL, L_ALL, "Non U-Lined Server %s is attempting to use svshost on %s", source_p->name, target_p->name);
			return;
		}
	} else {
		ilog(L_WARN, "svshost: Couldn't find target %s", parv[1]);
		/* we couldn't find the target. Just exit */
		return;
	}
}    
	  
/*
 * ms_svsnick
 *	Changes the targets nickname
 * 	parv[0] = sender prefix
 *	parv[1] = target 
 *	parv[2] = new nickname
 *	parv[3] = ts
 *
 */
static void ms_svsnick(struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[])
{
	struct Client *target_p;


	target_p = find_person(parv[1]);
	if (target_p == NULL) {
		ilog(L_WARN, "SVSNICK from %s was invalid. Can't find the client", parv[1]);
		return;
	}
	if (!IsClient(target_p)) 
		return;
	if (!IsUlined(source_p)) {
		sendto_realops_flags(FLAGS_ALL, L_ALL, "Non U-Lined Server %s is attempting to use svsnick on %s", source_p->name, target_p->name);
		return;
	}
	/* first find the target that we want to change */
	if (MyClient(target_p)) {
		if (!clean_nick_name(parv[2]) || !strcmp(target_p->name, parv[2])) {
			ilog(L_WARN, "svsnick: invalid nickname");
			return;
		}
		target_p->tsinfo = parv[4] ? atol(parv[3]) : CurrentTime;
		sendto_common_channels_local(target_p, ":%s!%s@%s NICK :%s", target_p->name, target_p->username, target_p->vhost, parv[2]);
		/* send it to the other servers */	
		if (target_p->user) {
			add_history(target_p, 1);
			sendto_server(NULL, target_p, NULL, 0, 0, LL_ICLIENT, ":%s NICK %s :%lu", target_p->name, parv[2], target_p->tsinfo);
		}	
		if (target_p->name[0])
			del_from_client_hash_table(target_p->name, target_p);
	
		strcpy(target_p->name, parv[2]);
		add_to_client_hash_table(target_p->name, target_p);

		/* del all the accept entries for this nick */
		del_all_accepts(target_p);
	} else {
		/* just relay the svsnick to a server that has this client */
		sendto_one(target_p, ":%s SVSNICK %s %s :%lu", source_p->name, target_p->name, parv[2], atol(parv[3]));
		
	}
	return;

}


/* clean_nick_name()
 *
 * input	- nickname
 * output	- none
 * side effects - walks through the nickname, returning 0 if erroneous
 */
static int clean_nick_name(char *nick)
{
  assert(nick);
  if(nick == NULL)
    return 0;

  /* nicks cant start with a digit or - */
  if (*nick == '-' || IsDigit(*nick))
    return 0;

  for(; *nick; nick++)
  {
    if(!IsNickChar(*nick))
      return 0;
  }

  return 1;
}


/*
 * ms_svsid
 *	Sets/Changes the Services ID for a particular Nickname
 * 	parv[0] = sender prefix
 *	parv[1] = target 
 *	parv[2] = newsvsid
 *
 */
static void ms_svsid(struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[])
{
	struct Client *target_p;


	target_p = find_person(parv[1]);
	if (target_p == NULL) {
		ilog(L_WARN, "SVSNICK from %s was invalid. Can't find the client", parv[1]);
		return;
	}
	if (!IsClient(target_p)) 
		return;
	if (!IsUlined(source_p)) {
		sendto_realops_flags(FLAGS_ALL, L_ALL, "Non U-Lined Server %s is attempting to use svsid on %s", source_p->name, target_p->name);
		return;
	}
	/* set the new ID */
	target_p->svsid = atol(parv[2]);

	/* and now send it to the rest of the network */
	sendto_server(client_p, target_p, NULL, 0, 0, LL_ICLIENT, ":%s SVSID %s %lu", source_p->name, target_p->name, target_p->svsid);

	return;

}
