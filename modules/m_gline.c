/*
 *  NeoIRCd: NeoStats Group. Based on Hybird7
 *  m_gline.c: Votes towards globally banning a mask.
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
 *  $Id: m_gline.c,v 1.5 2002/09/13 06:50:06 fishwaldo Exp $
 */

#include "stdinc.h"
#include "tools.h"
#include "handlers.h"
#include "s_gline.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "irc_string.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "fdlist.h"
#include "s_bsd.h"
#include "s_conf.h"
#include "s_misc.h"
#include "scache.h"
#include "send.h"
#include "msg.h"
#include "fileio.h"
#include "s_serv.h"
#include "hash.h"
#include "parse.h"
#include "modules.h"
#include "list.h"
#include "s_log.h"

/* internal functions */
static void set_local_gline(
                            const char *oper_nick,
                            const char *oper_user,
                            const char *oper_host,
                            const char *oper_server,
                            const char *user,
                            const char *host,
                            const char *reason);

static void log_gline(const char *, const char *,const char *,
                      const char* oper_server,
                      const char *,const char *,const char *);


static int check_wild_gline(char *, char *);
static int invalid_gline(struct Client *, char *, char *, char *);
		       
static void ms_gline(struct Client*, struct Client*, int, char**);
static void mo_gline(struct Client*, struct Client*, int, char**);
static void mo_ungline(struct Client*, struct Client*, int, char**);

struct Message gline_msgtab[] = {
    {"GLINE", 0, 0, 3, 0, MFLG_SLOW, 0,
      {m_unregistered, m_not_oper, ms_gline, mo_gline}},
    {"UNGLINE", 0, 0, 2, 0, MFLG_SLOW, 0,
      {m_unregistered, m_not_oper, m_error, mo_ungline}}
};

#ifndef STATIC_MODULES

void
_modinit(void)
{
    mod_add_cmd(&gline_msgtab[0]);
    mod_add_cmd(&gline_msgtab[1]);

}

void
_moddeinit(void)
{
  mod_del_cmd(&gline_msgtab[0]);
  mod_del_cmd(&gline_msgtab[1]);
}

const char *_version = "$Revision: 1.5 $";
#endif
/*
 * mo_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects -
 *
 * Place a G line if 3 opers agree on the identical user@host
 * 
 */
/* Allow this server to pass along GLINE if received and
 * GLINES is not defined.
 *
 */

static void
mo_gline(struct Client *client_p, struct Client *source_p,
	 int parc, char *parv[])
{
  char *user = NULL;
  char *host = NULL;	              /* user and host of GLINE "victim" */
  const char *reason = NULL;          /* reason for "victims" demise */
  char tempuser[USERLEN + 2];
  char temphost[HOSTLEN + 1];

  if (ConfigFileEntry.glines)
    {
      if (!IsOperGline(source_p))
	{
	  sendto_one(source_p,
		     ":%s NOTICE %s :You need gline = yes;",me.name,parv[0]);
	  return;
	}
			
      if ( (host = strchr(parv[1], '@')) || *parv[1] == '*' )
	{
	  /* Explicit user@host mask given */
	      
	  if(host != NULL)	/* Found user@host */
	    {
	      user = parv[1];   /* here is user part */
	      *(host++) = '\0'; /* and now here is host */

              /* gline for "@host", use *@host */
              if(*user == '\0')
                user = "*";
	    }
	  else
	    {
	      user = "*";               /* no @ found, assume its *@somehost */
	      host = parv[1];
	    }
	      
	  if (!*host)           /* duh. no host found, assume its '*' host */
	    host = "*";
	      
	  strlcpy(tempuser, user, USERLEN + 1);     /* allow for '*' */
	  strlcpy(temphost, host, HOSTLEN + 1);
	  user = tempuser;
	  host = temphost;
	}
      else
	{
	  sendto_one(source_p,
		     ":%s NOTICE %s :Can't G-Line a nick use user@host",
		     me.name, parv[0]);
	  return;
	}

      if(invalid_gline(source_p, user, host, parv[2]))
        return;
			
      /* Not enough non-wild characters were found,
       * assume they are trying to gline *@*.
       */

      if (check_wild_gline(user, host))
	{
	  if (MyClient(source_p))
	    sendto_one(source_p,
		       ":%s NOTICE %s :Please include at least %d non-wildcard characters with the user@host",
		       me.name, parv[0], ConfigFileEntry.min_nonwildcard);
	  return;
	}
			
      reason = parv[2];

      /* inform users about the gline before we call check_majority_gline()
       * so already voted comes below gline request --fl
       */
      sendto_realops_flags(FLAGS_ALL, L_ALL,
			"%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			source_p->name, source_p->username, source_p->host,
			me.name, user, host, reason);

      set_local_gline(source_p->name, source_p->username, source_p->host, me.name, user, host, reason);
      log_gline(source_p->name, source_p->username, source_p->host, me.name, user, host, reason);

      /* 4 param version for hyb-7 servers */
      sendto_server(NULL, source_p, NULL, CAP_GLN|CAP_UID, NOCAPS,
                    LL_ICLIENT,
                    ":%s GLINE %s %s :%s",
                    ID(source_p), user, host, reason);
      sendto_server(NULL, source_p, NULL, CAP_GLN, CAP_UID,
                    LL_ICLIENT,
                    ":%s GLINE %s %s :%s",
                    source_p->name, user, host, reason);

    }
  else
    {
      sendto_one(source_p,":%s NOTICE %s :GLINE disabled",me.name,parv[0]);  
    }
}

/*
 * ms_gline()
 *
 * inputs       - The usual for a m_ function
 * output       -
 * side effects -
 *
 * Place a G line if 3 opers agree on the identical user@host
 * 
 */
/* Allow this server to pass along GLINE if received and
 * GLINES is not defined.
 */

static void ms_gline(struct Client *client_p,
                     struct Client *source_p,
                     int parc,
                     char *parv[])
{
  /* These are needed for hyb6 compatibility.. if its ever removed we can
   * just use source_p->username etc.. 
   */
  const char *oper_nick = NULL;        /* nick of oper requesting GLINE */
  const char *oper_user = NULL;        /* username of oper requesting GLINE */
  const char *oper_host = NULL;        /* hostname of oper requesting GLINE */
  const char *oper_server = NULL;      /* server of oper requesting GLINE */
  char *user = NULL;
  char *host = NULL;             /* user and host of GLINE "victim" */
  const char *reason = NULL;           /* reason for "victims" demise */
  struct Client *acptr;

  /* hyb-7 style gline (post beta3) */
  if(parc == 4 && IsPerson(source_p))
    {
      oper_nick = parv[0];
      oper_user = source_p->username;
      oper_host = source_p->host;
      oper_server = source_p->user->server;
      user = parv[1];
      host = parv[2];
      reason = parv[3];
    }
  /* none of the above */
  else
    return;

  /* Its plausible that the server and/or client dont actually exist,
   * and its faked, as the oper isnt sending the gline..
   * check they're real --fl_ */
  /* we need acptr for LL introduction anyway -davidt */
  if((acptr = find_server(oper_server)))
  {
    if((acptr = find_client(oper_nick)) == NULL)
      return;
  }
  else
    return;

 if(invalid_gline(acptr, user, host, (char *)reason))
    return;
    
  /* send in hyb-7 to compatable servers */
  sendto_server(client_p, acptr, NULL, CAP_GLN, NOCAPS, LL_ICLIENT,
                ":%s GLINE %s %s :%s",
                oper_nick, user, host, reason);
  if (ConfigFileEntry.glines)
    {
     /* I dont like the idea of checking for x non-wildcard chars in a
      * gline.. it could lead to a desync... but we have to stop people
      * glining *@*..   -- fl
      */

     if (check_wild_gline(user, host))
        {
          sendto_realops_flags(FLAGS_ALL, L_ALL, 
                       "%s!%s@%s on %s is requesting a gline without %d non-wildcard characters for [%s@%s] [%s]",
                       oper_nick, oper_user, oper_host, oper_server, ConfigFileEntry.min_nonwildcard,
                       user, host, reason);
          return;
        }

      sendto_realops_flags(FLAGS_ALL, L_ALL,
			   "%s!%s@%s on %s is requesting gline for [%s@%s] [%s]",
			   oper_nick, oper_user, oper_host, oper_server,
			   user, host, reason);

      set_local_gline(oper_nick, oper_user, oper_host, oper_server, user, host, reason);	
      log_gline(oper_nick, oper_user, oper_host, oper_server, user, host, reason);

    }
}

/*
 * check_wild_gline
 *
 * inputs       - user, host of gline
 * output       - 1 if not enough non-wildchar char's, 0 if ok
 * side effects - NONE
 */

static int
check_wild_gline(char *user, char *host)
{
  char *p;
  char tmpch;
  int nonwild;

  nonwild = 0;
  p = user;

  while ((tmpch = *p++))
    {
       if (!IsKWildChar(tmpch))
         {
            /*
             * If we find enough non-wild characters, we can
             * break - no point in searching further.
             */
            if (++nonwild >= ConfigFileEntry.min_nonwildcard)
              break;
         }
    }

   if (nonwild < ConfigFileEntry.min_nonwildcard)
    {
       /*
        * The user portion did not contain enough non-wild
        * characters, try the host.
        */
       p = host;
       while ((tmpch = *p++))
         {
           if (!IsKWildChar(tmpch))
             if (++nonwild >= ConfigFileEntry.min_nonwildcard)
               break;
         }
     }

    if (nonwild < ConfigFileEntry.min_nonwildcard)
       return 1;
    else
       return 0;
}

/* invalid_gline
 *
 * inputs	- pointer to source client
 *		- pointer to ident
 *		- pointer to host
 *		- pointer to reason
 * outputs	- 1 if invalid, 0 if valid
 */
static int
invalid_gline(struct Client *source_p, char *luser, char *lhost,
                       char *lreason)
{
  if(strchr(luser, '!'))
  {
    sendto_one(source_p, ":%s NOTICE %s :Invalid character '!' in gline",
               me.name, source_p->name);
    return 1;
  }

  return 0;
}

/*
 * set_local_gline
 *
 * inputs	- pointer to oper nick
 * 		- pointer to oper username
 * 		- pointer to oper host
 *		- pointer to oper server
 *		- pointer to victim user
 *		- pointer to victim host
 *		- pointer reason
 * output	- NONE
 * side effects	-
 */
static void
set_local_gline(const char *oper_nick,
                            const char *oper_user,
                            const char *oper_host,
                            const char *oper_server,
                            const char *user,
                            const char *host,
                            const char *reason)
{
  char buffer[IRCD_BUFSIZE];
  struct ConfItem *aconf;
  const char *current_date;

  current_date = smalldate((time_t) 0);
          
  aconf = make_conf();
  aconf->status = CONF_KILL;
  DupString(aconf->host, host);

  ircsprintf(buffer, "%s (%s)",reason,current_date);
      
  DupString(aconf->passwd, buffer);
  DupString(aconf->name, (char *)user);
  DupString(aconf->host, (char *)host);
  aconf->hold = CurrentTime + ConfigFileEntry.gline_time;
  add_gline(aconf);
      
  sendto_realops_flags(FLAGS_ALL, L_ALL,
		       "%s!%s@%s on %s has triggered gline for [%s@%s] [%s]",
		       oper_nick, oper_user, oper_host, oper_server,
		       user, host, reason);
  check_klines();
}


/*
 * log_gline()
 *
 */
static void
log_gline(const char *oper_nick, const char *oper_user, const char *oper_host,
	  const char *oper_server, const char *user, const char *host,
	  const char *reason)
{
  char         buffer[2*BUFSIZE];
  char         filenamebuf[PATH_MAX + 1];
  static  char timebuffer[MAX_DATE_STRING + 1];
  struct tm*   tmptr;
  FBFILE       *out;

  if(ConfigFileEntry.glinefile == NULL)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,"*** Problem opening glinefile.");
      return;
    }

  ircsprintf(filenamebuf, "%s.%s", 
                ConfigFileEntry.glinefile, small_file_date((time_t) 0));

  if ((out = fbopen(filenamebuf, "a")) == NULL)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
			   "*** Problem opening %s",filenamebuf);
      return;
    }

  tmptr = localtime((const time_t*)&CurrentTime);
  strftime(timebuffer, MAX_DATE_STRING, "%Y/%m/%d %H:%M:%S", tmptr);

  ircsprintf(buffer,"#Gline for %s@%s %s added by the following\n",
                   user,host,timebuffer);

  if (fbputs(buffer,out) == -1)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,
			   "*** Problem writing to %s",filenamebuf);
      fbclose(out);
      return;
    }

  ircsprintf(buffer, "#%s!%s@%s on %s [%s]\n",
                   oper_nick,
                   oper_user,
                   oper_host,
                   oper_server,
                   (reason)?reason:"No reason");

  if (fbputs(buffer,out) == -1)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,"*** Problem writing to %s",filenamebuf);
      fbclose(out);
      return;
    }

  ircsprintf(buffer, "\"%s\",\"%s\",\"%s %s\",\"%s\",%lu\n",
	     user,
	     host,
	     reason,
	     timebuffer,
	     oper_nick,
	     (unsigned long) CurrentTime);

  if (fbputs(buffer,out) == -1)
    {
      sendto_realops_flags(FLAGS_ALL, L_ALL,"*** Problem writing to %s",filenamebuf);
      fbclose(out);
      return;
    }

  fbclose(out);
}



/*
** m_ungline
** added May 29th 2000 by Toby Verrall <toot@melnet.co.uk>
** added to hybrid-7 7/11/2000 --is
**
**      parv[0] = sender nick
**      parv[1] = gline to remove
*/

static void mo_ungline(struct Client *client_p, struct Client *source_p,
                      int parc,char *parv[])
{
  char  *user,*host;

  if (!ConfigFileEntry.glines)
    {
      sendto_one(source_p,":%s NOTICE %s :UNGLINE disabled",me.name,parv[0]);
      return;
    }

  if (!IsOperUnkline(source_p) || !IsOperGline(source_p))
    {
      sendto_one(source_p,":%s NOTICE %s :You need unkline = yes;",
                 me.name,parv[0]);
      return;
    }

  if ( (host = strchr(parv[1], '@')) || *parv[1] == '*' )
    {
      /* Explicit user@host mask given */

      if(host)                  /* Found user@host */
        {
          user = parv[1];       /* here is user part */
          *(host++) = '\0';     /* and now here is host */
        }
      else
        {
          user = "*";           /* no @ found, assume its *@somehost */
          host = parv[1];
        }
    }
  else
    {
      sendto_one(source_p, ":%s NOTICE %s :Invalid parameters",
                 me.name, parv[0]);
      return;
    }

  if(remove_gline_match(user, host))
    {
      sendto_one(source_p, ":%s NOTICE %s :Un-glined [%s@%s]",
                 me.name, parv[0],user, host);
      sendto_realops_flags(FLAGS_ALL, L_ALL,
			   "%s has removed the G-Line for: [%s@%s]",
			   get_oper_name(source_p), user, host );
      ilog(L_NOTICE, "%s removed G-Line for [%s@%s]",
          get_oper_name(source_p), user, host);
      return;
    }
  else
    {
      sendto_one(source_p, ":%s NOTICE %s :No G-Line for %s@%s",
                 me.name, parv[0],user,host);
      return;
    }
}
