/************************************************************************
 *   IRC - Internet Relay Chat, m_svscmds.c
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
 *   $Id: m_svscmds.c,v 1.5 2002/09/17 11:03:21 fishwaldo Exp $
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "handlers.h"
#include "client.h"
#include "channel.h"
#include "channel_mode.h"
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

static void ms_svsjoin(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[]);
static void ms_svspart(struct Client *client_p, struct Client *source_p,
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

struct Message svsjoin_msgtab = {
  "SVSJOIN", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_svsjoin, m_ignore}
};
struct Message svspart_msgtab = {
  "SVSPART", 0, 0, 3, 0, MFLG_SLOW, 0,
  {m_ignore, m_ignore, ms_svspart, m_ignore}
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
  mod_add_cmd(&svsjoin_msgtab);
  mod_add_cmd(&svspart_msgtab);
}

/* here we tell it what to do when the module is unloaded */
void
_moddeinit(void)
{
  /* This will remove the commands in test_msgtab (which is above) */
  mod_del_cmd(&svshost_msgtab);
  mod_del_cmd(&svsnick_msgtab);
  mod_del_cmd(&svsid_msgtab);
  mod_del_cmd(&svsjoin_msgtab);
  mod_del_cmd(&svspart_msgtab);
}

/* When we last modified the file (shown in /modlist), this is usually:
 */
const char *_version = "$Revision: 1.5 $";
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
			sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Non U-Lined Server %s is attempting to use svshost on %s", source_p->name, target_p->name);
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
		sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Non U-Lined Server %s is attempting to use svsnick on %s", source_p->name, target_p->name);
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
		sendto_realops_flags(FLAGS_ALL|FLAGS_REMOTE, L_ALL, "Non U-Lined Server %s is attempting to use svsid on %s", source_p->name, target_p->name);
		return;
	}
	/* set the new ID */
	target_p->svsid = atol(parv[2]);

	/* and now send it to the rest of the network */
	sendto_server(client_p, target_p, NULL, 0, 0, LL_ICLIENT, ":%s SVSID %s %lu", source_p->name, target_p->name, target_p->svsid);

	return;

}

/*
 * m_svsjoin
 *      parv[0] = sender prefix
 *      parv[1] = user to force
 *      parv[2] = channel to force them into
 */
static void ms_svsjoin(struct Client *client_p, struct Client *source_p,
                         int parc, char *parv[])
{
  struct Client *target_p;
  struct Channel *chptr;
  int type;
  char mode;
  char sjmode;
  char *newch;

  /* if its from a client, and its not services, ignore it */
  if(!IsServices(source_p) && IsClient(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :Restricted to Services", me.name, parv[0]);
    return;
  }
  /* if its from a server, and its not ulined, ignore it */
  if (!IsUlined(source_p) && IsServer(source_p))
    return;

  if((hunt_server(client_p, source_p, ":%s FORCEJOIN %s %s", 1, parc, parv)) != HUNTED_ISME)
    return;

  /* if target_p is not existant, print message
   * to source_p and bail - scuzzy
   */
  if ((target_p = find_client(parv[1])) == NULL)
  {
    if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
	       source_p->name, parv[1]);
    return;
  }

  if(!IsClient(target_p))
    return;

  /* select our modes from parv[2] if they exist... (chanop)*/
  if(*parv[2] == '¤')
  {
    type = CHFL_ADMIN;
    mode = 'a';
    sjmode = '¤';
  }
  if(*parv[2] == '@')
  {
    type = CHFL_CHANOP;
    mode = 'o';
    sjmode = '@';
  }
  else if(*parv[2] == '%')
  {
    type = CHFL_HALFOP;
    mode = 'h';
    sjmode = '%';
  }
  else if(*parv[2] == '+')
  {
    type = CHFL_VOICE;
    mode = 'v';
    sjmode = '+';
  }
  else
  {
    type = CHFL_PEON;
    mode = sjmode = '\0';
  }
    
  if(mode != '\0')
    parv[2]++;
    
  if((chptr = hash_find_channel(parv[2])) != NULL)
    {
      if(IsMember(target_p, chptr))
      {
        /* debugging is fun... */
        if (IsClient(source_p)) sendto_one(source_p, ":%s NOTICE %s :*** Notice -- %s is already in %s", me.name,
		   source_p->name, target_p->name, chptr->chname);
	return;
      }

      add_user_to_channel(chptr, target_p, type);

      if (chptr->chname[0] != '&')
        sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
	              ":%s SJOIN %lu %s + :%c%s",
	              me.name, (unsigned long) chptr->channelts,
	              chptr->chname, type ? sjmode : ' ', target_p->name);

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
                             target_p->name, target_p->username,
                             target_p->host, chptr->chname);

      if(type)
        sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +%c %s",
	                     me.name, chptr->chname, mode, target_p->name);
        
      if(chptr->topic != NULL)
      {
	sendto_one(target_p, form_str(RPL_TOPIC), me.name,
	           target_p->name, chptr->chname, chptr->topic);
        sendto_one(target_p, form_str(RPL_TOPICWHOTIME),
	           me.name, source_p->name, chptr->chname,
	           chptr->topic_info, chptr->topic_time);
      }

      channel_member_names(target_p, chptr, chptr->chname, 1);
    }
  else
    {
      newch = parv[2];
      if (!check_channel_name(newch))
      {
        if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
		   source_p->name, (unsigned char*)newch);
	return;
      }

      /* channel name must begin with & or # */
      if (!IsChannelName(newch))
      {
        if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_BADCHANNAME), me.name,
		   source_p->name, (unsigned char*)newch);
        return;
      }

     /* it would be interesting here to allow an oper
      * to force target_p into a channel that doesn't exist
      * even more so, into a local channel when we disable
      * local channels... but...
      * I don't want to break anything - scuzzy
      */
      if (ConfigServerHide.disable_local_channels &&
	  (*newch == '&'))
      {
        if (IsClient(source_p)) sendto_one(source_p, ":%s NOTICE %s :No such channel (%s)", me.name,
		   source_p->name, newch);
        return;
      }

      /* newch can't be longer than CHANNELLEN */
      if (strlen(newch) > CHANNELLEN)
      {
	if (IsClient(source_p)) sendto_one(source_p, ":%s NOTICE %s :Channel name is too long", me.name,
		   source_p->name);
        return;
      }

      chptr = get_or_create_channel(target_p, newch, NULL);
      add_user_to_channel(chptr, target_p, CHFL_CHANOP);

      /* send out a join, make target_p join chptr */
      if (chptr->chname[0] != '&')
        sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
                      ":%s SJOIN %lu %s +nt :¤%s", me.name,
		      (unsigned long) chptr->channelts, chptr->chname,
		      target_p->name);

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
                             target_p->name, target_p->username,
                             target_p->host, chptr->chname);

      chptr->mode.mode |= MODE_TOPICLIMIT;
      chptr->mode.mode |= MODE_NOPRIVMSGS;

      sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +nt", me.name,
                           chptr->chname);

      target_p->localClient->last_join_time = CurrentTime;
      channel_member_names(target_p, chptr, chptr->chname, 1);

    }
}


static void ms_svspart(struct Client *client_p, struct Client *source_p,
		         int parc, char *parv[])
{
  struct Client *target_p;
  struct Channel *chptr;

  /* if its from a client, and its not services, ignore it */
  if(!IsServices(source_p) && IsClient(source_p))
  {
    sendto_one(source_p, ":%s NOTICE %s :Restricted to Services", me.name, parv[0]);
    return;
  }
  /* if its from a server, and its not ulined, ignore it */
  if (!IsUlined(source_p) && IsServer(source_p))
    return;

  if((hunt_server(client_p, source_p, ":%s FORCEPART %s %s", 1, parc, parv)) != HUNTED_ISME)
    return;

  /* if target_p == NULL then let the oper know */
  if ((target_p = find_client(parv[1])) == NULL)
  {
    if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_NOSUCHNICK), me.name,
               source_p->name, parv[1]);
    return;
  }

  if(!IsClient(target_p))
    return;

  if((chptr = hash_find_channel(parv[2])) == NULL)
  {
    if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_NOSUCHCHANNEL),
               me.name, parv[0], parv[1]);
    return;
  }

  if (!IsMember(target_p, chptr))
  {
    if (IsClient(source_p)) sendto_one(source_p, form_str(ERR_USERNOTINCHANNEL),
               me.name, parv[0], parv[2], parv[1]);
    return;
  }
  
  if (chptr->chname[0] != '&')
    sendto_server(target_p, target_p, chptr, NOCAPS, NOCAPS, LL_ICLIENT,
		  ":%s PART %s :%s",
		  target_p->name, chptr->chname,
		  target_p->name);

  sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
                       target_p->name, target_p->username,
 	               target_p->host,chptr->chname,
		       target_p->name);
  remove_user_from_channel(chptr, target_p);
}

