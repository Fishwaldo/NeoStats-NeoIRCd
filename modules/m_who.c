/*
 *  NeoIRCd: NeoStats Group. Based on Hybird7
 *  m_who.c: Shows who is on a channel.
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
 *  $Id: m_who.c,v 1.9 2002/09/14 17:03:00 fishwaldo Exp $
 */
#include "stdinc.h"
#include "tools.h"
#include "common.h"   
#include "handlers.h"
#include "client.h"
#include "channel.h"
#include "channel_mode.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "list.h"
#include "irc_string.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static void m_who(struct Client*, struct Client*, int, char**);
static void ms_who(struct Client*, struct Client*, int, char**);

struct Message who_msgtab = {
  "WHO", 0, 0, 2, 0, MFLG_SLOW, 0,
  {m_unregistered, m_who, ms_who, m_who}
};

#ifndef STATIC_MODULES
void
_modinit(void)
{
  mod_add_cmd(&who_msgtab);
}

void
_moddeinit(void)
{
  mod_del_cmd(&who_msgtab);
}
const char *_version = "$Revision: 1.9 $";
#endif
static void do_who_on_channel(struct Client *source_p,
			      struct Channel *chptr, char *real_name,
			      int server_oper, int member);

static void do_who_list(struct Client *source_p, struct Channel *chptr,
                        dlink_list *peons_list, dlink_list *chanops_list,
                        dlink_list *halfops_list, 
			dlink_list *chanadmins_list, 
			dlink_list *voiced_list,
                        char *chanop_flag,
			char *halfop_flag,
			char *voiced_flag,
			char *chanadmin_flag,
                        char *chname, int member);

static void who_global(struct Client *source_p, char *mask, int server_oper);

static void do_who(struct Client *source_p,
                   struct Client *target_p,
                   char *chname,
                   char *op_flags);


/*
** m_who
**      parv[0] = sender prefix
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag, only 'o' for now.
*/
static void m_who(struct Client *client_p,
                 struct Client *source_p,
                 int parc,
                 char *parv[])
{
  struct Client *target_p;
  char  *mask = parc > 1 ? parv[1] : NULL;
  dlink_node *lp;
  struct Channel *chptr=NULL;
  struct Channel *mychannel = NULL;
  char  flags[NUMLISTS][2];
  int   server_oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
  int   member;

  /* See if mask is there, collapse it or return if not there */

  if (mask != NULL)
    {
      (void)collapse(mask);

      if (*mask == '\0')
	{
	  sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], "*");
	  return;
	}
    }
  else
    {
      who_global(source_p, mask, server_oper);
      sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], "*" );
      return;
    }

  /* mask isn't NULL at this point. repeat after me... -db */

  /* '/who *' */

  if ((*(mask+1) == (char) 0) && (*mask == '*'))
    {
      if (source_p->user)
	if ((lp = source_p->user->channel.head))
	  mychannel = lp->data;

      if (!mychannel)
        {
          sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], "*");
          return;
        }

	do_who_on_channel(source_p, mychannel, "*", NO, YES);

      sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], "*");
      return;
    }

  /* '/who #some_channel' */

  if (IsChannelName(mask))
    {
      /*
       * List all users on a given channel
       */
      chptr = hash_find_channel(mask);
      if (chptr != NULL)
	{
	    {
	      if ( IsMember(source_p, chptr) )
		do_who_on_channel(source_p, chptr, chptr->chname, NO, YES);
	      else if(!SecretChannel(chptr))
		do_who_on_channel(source_p, chptr, chptr->chname, NO, NO);
	    }
	}
      sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], mask);
      return;
    }

  /* '/who nick' */

  if (((target_p = find_client(mask)) != NULL) &&
      IsPerson(target_p) && (!server_oper || IsOper(target_p)))
    {
      char *chname=NULL;
      int isinvis = 0;

      if(IsServer(client_p))
	client_burst_if_needed(client_p,target_p);

      isinvis = IsInvisible(target_p);
      for (lp = target_p->user->channel.head; lp; lp = lp->next)
	{
	  chptr = lp->data;
	  chname = chptr->chname;

          member = IsMember(source_p, chptr);
          if (isinvis && !member)
            continue;
          if (member || (!isinvis && PubChannel(chptr)))
            {
              break;
            }
	}

      if (chptr != NULL)
	{

	  /* XXX globalize this inside m_who.c ? */
	  /* jdc -- Check is_any_op() for +o > +h > +v priorities */
	  set_channel_mode_flags( flags, chptr, source_p );

	  if (is_chan_op(chptr,target_p))
	    do_who(source_p, target_p, chname, flags[0]);
	  else if(is_half_op(chptr,target_p))
	    do_who(source_p, target_p, chname, flags[1]);
	  else if(is_voiced(chptr,target_p))
	    do_who(source_p, target_p, chname, flags[2]);
	  else if(is_chan_admin(chptr, target_p))
	    do_who(source_p, target_p, chname, flags[4]);
	  else
	    do_who(source_p, target_p, chname, "");
	}
      else
	{
	  do_who(source_p, target_p, NULL, "");
	}

      sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], mask);
      return;
    }

  /* '/who 0' */
  if ((*(mask + 1) == '\0') && (*mask == '0'))
    who_global(source_p, NULL, server_oper);
  else
    who_global(source_p, mask, server_oper);

 /* Wasn't a nick, wasn't a channel, wasn't a '*' so ... */
  sendto_one(source_p, form_str(RPL_ENDOFWHO), me.name, parv[0], mask);
}

/* who_common_channel
 * inputs	- pointer to client requesting who
 * 		- pointer to channel member chain.
 *		- char * mask to match
 *		- int if oper on a server or not
 *		- pointer to int maxmatches
 * output	- NONE
 * side effects - lists matching clients on specified channel,
 * 		  marks matched clients.
 *
 */
static void who_common_channel(struct Client *source_p,dlink_list chain,
		char *mask,int server_oper, int *maxmatches)
{
  dlink_node *clp;
 struct Client *target_p;

  for (clp = chain.head; clp; clp = clp->next)
   {
     target_p = clp->data;

     if (!IsInvisible(target_p) || IsMarked(target_p))
       continue;

     if (server_oper && !IsOper(target_p))
       continue;

     SetMark(target_p);

     if ((mask == NULL) ||
          match(mask, target_p->name) || match(mask, target_p->username) ||
          match(mask, target_p->host) || match(mask, target_p->vhost) ||
	  (match(mask, target_p->user->server) && 
	   (IsOper(source_p) || !ConfigServerHide.hide_servers)) ||
	  match(mask, target_p->info))
     {
		
       do_who(source_p, target_p, NULL, "");

       if (*maxmatches > 0)
       {
         --(*maxmatches);
         if(*maxmatches == 0)
            return;
       }

     }
   }
}

/*
 * who_global
 *
 * inputs	- pointer to client requesting who
 *		- char * mask to match
 *		- int if oper on a server or not
 * output	- NONE
 * side effects - do a global scan of all clients looking for match
 *		  this is slightly expensive on EFnet ...
 */

static void who_global(struct Client *source_p,char *mask, int server_oper)
{
  struct Channel *chptr=NULL;
  struct Client *target_p;
  dlink_node  *lp;
  int   maxmatches = 500;

  /* first, list all matching INvisible clients on common channels */
  for (lp = source_p->user->channel.head; lp; lp = lp->next)
  {
     chptr = lp->data;
     who_common_channel(source_p,chptr->chanops,mask,server_oper,&maxmatches);
     who_common_channel(source_p,chptr->halfops,mask,server_oper,&maxmatches);
     who_common_channel(source_p,chptr->voiced,mask,server_oper,&maxmatches);
     who_common_channel(source_p,chptr->peons,mask,server_oper,&maxmatches);
  }

  /* second, list all matching visible clients */
  for (target_p = GlobalClientList; target_p; target_p = target_p->next)
  {
    if (!IsPerson(target_p))
      continue;

    if (IsInvisible(target_p))
    {
      ClearMark(target_p);
      continue;
    }

    if (server_oper && !IsOper(target_p))
      continue;

    if (!mask ||
        match(mask, target_p->name) || match(mask, target_p->username) ||
	match(mask, target_p->host) || match(mask, target_p->user->server) ||
	match(mask, target_p->info) || match(mask, target_p->vhost))
    {
		
      do_who(source_p, target_p, NULL, "");
      if (maxmatches > 0)
      {
        --maxmatches;
        if( maxmatches == 0 )
  	  return;
      }

    }

  }
}


/*
 * do_who_on_channel
 *
 * inputs	- pointer to client requesting who
 *		- pointer to channel to do who on
 *		- The "real name" of this channel
 *		- int if source_p is a server oper or not
 *		- int if client is member or not
 * output	- NONE
 * side effects - do a who on given channel
 */

static void do_who_on_channel(struct Client *source_p,
			      struct Channel *chptr,
			      char *chname,
			      int server_oper, int member)
{
  char flags[NUMLISTS][2];

  /* jdc -- Check is_any_op() for +o > +h > +v priorities */
  set_channel_mode_flags( flags, chptr, source_p );
  do_who_list(source_p, chptr,
              &chptr->peons,
              &chptr->chanops,
              &chptr->halfops,
	      &chptr->chanadmins,
              &chptr->voiced,
              flags[0],
              flags[1],
              flags[2],
              flags[4],
              chname, member);

}

static void do_who_list(struct Client *source_p, struct Channel *chptr,
			dlink_list *peons_list,
                        dlink_list *chanops_list,
			dlink_list *halfops_list,
			dlink_list *chanadmins_list,
			dlink_list *voiced_list,
			char *chanop_flag,
			char *halfop_flag,
			char *voiced_flag,
			char *admins_flag,
			char *chname, int member)
{
  struct Client *target_p;

  dlink_node *chanops_ptr;
  dlink_node *peons_ptr;
  dlink_node *voiced_ptr;
  dlink_node *halfops_ptr;
  dlink_node *chanadmins_ptr;
  int done=0;

  peons_ptr   = peons_list->head;
  chanops_ptr = chanops_list->head;
  voiced_ptr  = voiced_list->head;
  halfops_ptr = halfops_list->head;
  chanadmins_ptr = chanadmins_list->head;

  while (done < NUMLISTS)
    {
      done = 0;

      if(peons_ptr != NULL)
        {
          target_p = peons_ptr->data;

          if(member || !IsInvisible(target_p))
            do_who(source_p, target_p, chname, "");
          peons_ptr = peons_ptr->next;
        }
      else
        done++;

      if(chanops_ptr != NULL)
        {
          target_p = chanops_ptr->data;

          if(member || !IsInvisible(target_p))
            do_who(source_p, target_p, chname, chanop_flag);
          chanops_ptr = chanops_ptr->next;
        }
      else
        done++;
      if(chanadmins_ptr != NULL)
        {
          target_p = chanadmins_ptr->data;
          if(member || !IsInvisible(target_p))
            do_who(source_p, target_p, chname, admins_flag);
          chanadmins_ptr = chanadmins_ptr->next;
        }
      else
        done++;

      if(halfops_ptr != NULL)
        {
          target_p = halfops_ptr->data;

          if(member || !IsInvisible(target_p))
            do_who(source_p, target_p, chname, halfop_flag);
          halfops_ptr = halfops_ptr->next;
        }
      else
        done++;

      if(voiced_ptr != NULL)
        {
          target_p = voiced_ptr->data;

          if(member || !IsInvisible(target_p))
          {
            if(target_p == source_p && is_voiced(chptr, source_p) && 
               chptr->mode.mode & MODE_HIDEOPS)
               do_who(source_p, target_p, chname, "+");
            else
              do_who(source_p, target_p, chname, voiced_flag);
          }
          voiced_ptr = voiced_ptr->next;
        }
      else
        done++;

    }
}
/*
 * do_who
 *
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- The reported name
 *		- channel flags
 * output	- NONE
 * side effects - do a who on given person
 */

static void do_who(struct Client *source_p,
                   struct Client *target_p,
                   char *chname,
                   char *op_flags)
{
  char  status[5];
  ircsprintf(status,"%c%s%s", 
	     target_p->user->away ? 'G' : 'H',
	     IsOper(target_p) ? "*" : "", op_flags );

  if(ConfigServerHide.hide_servers)
    {
      	sendto_one(source_p, form_str(RPL_WHOREPLY), me.name, source_p->name,
			 (chname) ? (chname) : "*",
			 target_p->username,
			 IsOper(source_p) ? target_p->host : target_p->vhost, IsOper(source_p) ? target_p->user->server : "*",
			 target_p->name,
			 status, 0, target_p->info);
    }
  else
    {
      sendto_one(source_p, form_str(RPL_WHOREPLY), me.name, source_p->name,
		 (chname) ? (chname) : "*",
		 target_p->username,
		 IsOper(source_p) ? target_p->host : target_p->vhost,  target_p->user->server, target_p->name,
		 status, target_p->hopcount, target_p->info);
    }
}

/*
** ms_who
**      parv[0] = sender prefix
**      parv[1] = nickname mask list
**      parv[2] = additional selection flag, only 'o' for now.
*/
static void ms_who(struct Client *client_p,
                  struct Client *source_p,
                  int parc,
                  char *parv[])
{
  /* If its running as a hub, and linked with lazy links
   * then allow leaf to use normal client m_who()
   * other wise, ignore it.
   */

  if( ServerInfo.hub )
    {
      if(!IsCapable(client_p->from,CAP_LL))
	return;
    }

  m_who(client_p,source_p,parc,parv);
}
