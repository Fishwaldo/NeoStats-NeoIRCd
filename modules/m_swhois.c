/*
 *  NeoIRCd: NeoStats Group. Based on Hybird7
 *  m_swhois.c: Sends a notice when someone uses WHOIS.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: m_swhois.c,v 1.2 2002/09/17 06:09:35 fishwaldo Exp $
 */
#include "stdinc.h"
#include "tools.h"
#include "common.h"  
#include "handlers.h"
#include "client.h"
#include "hash.h"
#include "channel.h"
#include "channel_mode.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include  "s_user.h"
#include "send.h"
#include "list.h"
#include "irc_string.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hook.h"

int send_swhois(struct hook_mfunc_data *);
static void mo_swhois(struct Client*, struct Client*, int, char**);
static void ms_swhois(struct Client*, struct Client*, int, char**);

struct Message swhois_msgtab = {
  "SWHOIS", 0, 0, 3, 0, MFLG_SLOW, 0L,
  {m_unregistered, m_not_oper, ms_swhois, mo_swhois}
};




void
_modinit(void)
{
  hook_add_hook("doing_whois", (hookfn *)send_swhois);
  mod_add_cmd(&swhois_msgtab);
}

void
_moddeinit(void)
{
  hook_del_hook("doing_whois", (hookfn *)send_swhois);
  mod_del_cmd(&swhois_msgtab);
}

const char *_version = "$Revision: 1.2 $";

/* show a whois notice
   source_p does a /whois on client_p */
int
send_swhois(struct hook_mfunc_data *data)
{
  if (strlen(data->client_p->swhois) > 1)
    sendto_one(data->source_p, form_str(RPL_SWHOIS), me.name, data->source_p->name, data->client_p->name, data->client_p->swhois);
  return 0;
}


/*
 * mo_swhois 
 * Allows a Admin to set swhois on clients 
 * inputs     - target to change
 *            - swhois string 
 * outputs    - none
 * sideeffect - Sets the swhois string for that user
 *
 */
static void mo_swhois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;
    
    target_p = find_client(parv[1]);
    if (!target_p) {
  	  sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
	  return;
    }
    if (!IsAdmin(source_p)) {
    	  sendto_one(source_p, form_str(ERR_NOPRIVILEGES),me.name,source_p->name);
    	  return;
    }
    strncpy(target_p->swhois, parv[2], REALLEN);
    sendto_one(source_p, ":%s NOTICE %s :*** NOTICE: swhois updated for %s", me.name, source_p->name, target_p->name);
    sendto_server(NULL, target_p, NULL, NOCAPS, NOCAPS, LL_ICLIENT, ":%s SWHOIS %s :%s", me.name, target_p->name, parv[2]);
    
}

/*
 * ms_swhois 
 * Allows a Admin to set swhois on clients 
 * inputs     - target to change
 *            - swhois string 
 * outputs    - none
 * sideeffect - Sets the swhois string for that user
 *
 */
static void ms_swhois(struct Client *client_p, struct Client *source_p, int parc, char *parv[])
{
    struct Client *target_p;
    
    target_p = find_client(parv[1]);
    if (!target_p) {
	  return;
    }
    if (!IsServer(source_p)) {
    	  return;
    }
    strncpy(target_p->swhois, parv[2], REALLEN);
    sendto_server(client_p, target_p, NULL, NOCAPS, NOCAPS, LL_ICLIENT, ":%s SWHOIS %s :%s", me.name, target_p->name, parv[2]);
    
}
