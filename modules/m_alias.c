/************************************************************************
 *   IRC - Internet Relay Chat. m_alias.c module - Create Services alias's
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
 *   $Id: m_alias.c,v 1.1 2002/09/13 06:50:06 fishwaldo Exp $
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
#define CHANSERV "ChanServ"
#define NICKSERV "NickServ"
#define STATSERV "StatServ"
#define HOSTSERV "HostServ"
#define OPERSERV "OperServ"



/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */
static void m_chanserv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void m_nickserv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void m_identify(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void m_statserv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void m_hostserv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);

static void m_operserv(struct Client *client_p, struct Client *source_p,
                    int parc, char *parv[]);


static void send_alias (char *who, struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[]);
	    

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */
struct Message services_msgtab[] = {
  {CHANSERV, 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_chanserv, m_ignore, m_chanserv}},
  {NICKSERV, 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_nickserv, m_ignore, m_nickserv}},
  {"IDENTIFY", 0, 0, 2, 3, MFLG_SLOW, 0,
  {m_ignore, m_identify, m_ignore, m_identify}},
  {STATSERV, 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_statserv, m_ignore, m_statserv}},
  {HOSTSERV, 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_hostserv, m_ignore, m_hostserv}},
  {OPERSERV, 0, 0, 2, MAXPARA, MFLG_SLOW, 0,
  {m_ignore, m_operserv, m_ignore, m_operserv}}
};

/* Thats the msgtab finished */

#ifndef STATIC_MODULES
/* Here we tell it what to do when the module is loaded */
void
_modinit(void)
{
  /* This will add the commands in test_msgtab (which is above) */
  mod_add_cmd(&services_msgtab[0]);
  mod_add_cmd(&services_msgtab[1]);
  mod_add_cmd(&services_msgtab[2]);
  mod_add_cmd(&services_msgtab[3]);
  mod_add_cmd(&services_msgtab[4]);
  mod_add_cmd(&services_msgtab[5]);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&services_msgtab[0]);
  mod_del_cmd(&services_msgtab[1]);
  mod_del_cmd(&services_msgtab[2]);
  mod_del_cmd(&services_msgtab[3]);
  mod_del_cmd(&services_msgtab[4]);
  mod_del_cmd(&services_msgtab[5]);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.1 $";
#endif

/*
 * m_chanserv
 * 	send a message to chanserv
 *      parv[0] = sender prefix
 *      parv[1+++] = commands to send
 */
static void m_chanserv(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	send_alias(CHANSERV, client_p, source_p, parc, parv);
}    

	  
/*
 * m_nickserv
 * 	send a message to nickserv
 *      parv[0] = sender prefix
 *      parv[1+++] = commands to send
 */
static void m_nickserv(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	send_alias(NICKSERV, client_p, source_p, parc, parv);
}    

/*
 * m_identify
 * 	identify for a nickname or channel
 *      parv[0] = sender prefix
 *      parv[1] = if *parv[1][1] = # it a password for a channel, otherwise a nickserv password
 *	parv[2] = if its a channel password, its the actual password
 */
static void m_identify(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	struct Client *target_p;
	
	if (parv[1][0] == '#') {
		/* its a channel password */
		target_p = find_person(CHANSERV);
		if ((target_p != NULL) && (IsServices(target_p))) {
			/* send the message */
			sendto_one(target_p, ":%s PRIVMSG %s!%s@%s :identify %s", source_p->name, target_p->name, target_p->username, target_p->host, parv[1]);
			return;
		} else {
			sendto_one(source_p, ":%s 440 %s %s :Services are currently down. Please try again later.", me.name, source_p->name, CHANSERV);
			return;
		}
	} else {
		/* its a nickserv password */
		target_p = find_person(NICKSERV);
		if ((target_p != NULL) && (IsServices(target_p))) {
			/* send the message */
			sendto_one(target_p, ":%s PRIVMSG %s!%s@%s :identify %s", source_p->name, target_p->name, target_p->username, target_p->host, parv[1]);
			return;
		} else {
			sendto_one(source_p, ":%s 440 %s %s :Services are currently down. Please try again later.", me.name, source_p->name, NICKSERV);
			return;
		}
	}
	return;
}    
	  	  
/*
 * m_statserv
 * 	send a message to statserv
 *      parv[0] = sender prefix
 *	parv[1+++] = commands to send
 */
static void m_statserv(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	send_alias(STATSERV, client_p, source_p, parc, parv);
}    

/*
 * m_operserv
 * 	send a message to operserv
 *      parv[0] = sender prefix
 *	parv[1+++] = commands to send
 */
static void m_operserv(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	send_alias(OPERSERV, client_p, source_p, parc, parv);
}    

/*
 * m_hostserv
 * 	send a message to hostserv
 *      parv[0] = sender prefix
 *	parv[1+++] = commands to send
 */
static void m_hostserv(struct Client *client_p, struct Client *source_p,
                   int parc, char *parv[])
{
	send_alias(HOSTSERV, client_p, source_p, parc, parv);
}    


static void send_alias (char *who, struct Client *client_p, struct Client *source_p,
		    int parc, char *parv[])
{
	struct Client *target_p;

	target_p = find_person(who);
	
	/* if we found the target and they are set as Services, send it, otherwise send a services not online numeric */

	if ((target_p != NULL) && (IsServices(target_p))) {
		/* send the message */
		sendto_one(target_p, ":%s PRIVMSG %s!%s@%s :%s", source_p->name, target_p->name, target_p->username, target_p->host, parv[1]);	
	} else {
		sendto_one(source_p, ":%s 440 %s %s :Services are currently down. Please try again later.", me.name, source_p->name, who);
		return;
	}
}
