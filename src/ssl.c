/*
 *  NeoIRCd: NeoStats Group. Based on Hybird7
 *  ssl.c: Listens on a port.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *  Originally copied from Ultimate3, modified to work with NeoIRCd. 
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
 *  $Id: ssl.c,v 1.2 2002/11/04 08:50:46 fishwaldo Exp $
 */

#include "stdinc.h"
#include "config.h"
#include "ircd_defs.h"
#include "s_log.h"
#include "common.h"
#include "ssl.h"
#include "client.h"
#include "send.h"

#ifdef USE_SSL
#define IRCDSSL_CPATH "/home/fish/ircd/etc/ircd.crt"
#define IRCDSSL_KPATH "/home/fish/ircd/etc/ircd.key"


#define SAFE_SSL_READ	1
#define SAFE_SSL_WRITE	2
#define SAFE_SSL_ACCEPT	3

extern int errno;

SSL_CTX *ircdssl_ctx;
int ssl_capable = 0;

int
initssl (void)
{
  SSL_load_error_strings ();
  SSLeay_add_ssl_algorithms ();
  ircdssl_ctx = SSL_CTX_new (SSLv23_server_method ());
  if (!ircdssl_ctx)
    {
      ilog(L_ERROR, "initssl(): Failed to Create SSL context");
      return 0;
    }
  if (SSL_CTX_use_certificate_file (ircdssl_ctx,
				    IRCDSSL_CPATH, SSL_FILETYPE_PEM) <= 0)
    {
      ilog(L_ERROR, "initssl(): Failed to initilize SSL Certificate File");
      SSL_CTX_free (ircdssl_ctx);
      return 0;
    }
  if (SSL_CTX_use_PrivateKey_file (ircdssl_ctx,
				   IRCDSSL_KPATH, SSL_FILETYPE_PEM) <= 0)
    {
      ilog(L_ERROR, "initssl(): Failed to use Private Certificate");
      SSL_CTX_free (ircdssl_ctx);
      return 0;
    }
  if (!SSL_CTX_check_private_key (ircdssl_ctx))
    {
      ilog(L_ERROR, "Server certificate does not match Server key");
      SSL_CTX_free (ircdssl_ctx);
      return 0;
    }
  ilog(L_INFO, "SSL Initilized Successfully");
  return 1;
}

static int fatal_ssl_error (int, int, struct Client *);

int
safe_SSL_read (struct Client * client_p, void *buf, int sz)
{
  int len, ssl_err;

  bzero(buf, sz);
  len = SSL_read(client_p->localClient->ssl, buf, sz);
printf("ssl read %s\n\nlen %d (%d)", buf, len, sz);

  if (len <= 0)
    {
    printf("len is less than 0 (%d)\n", len);
      switch (ssl_err = SSL_get_error (client_p->localClient->ssl, len))
	{
	case SSL_ERROR_SYSCALL:
printf("SSL_ERROR_SYSCALL\n");
	  if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
	    {
	case SSL_ERROR_WANT_READ:
	      errno = EWOULDBLOCK;
#ifdef DEBUG
	      printf("SSL_ERROR_WANT_READ\n");
	      fatal_ssl_error(ssl_err, SAFE_SSL_READ, client_p);
#endif	      
	      return -1;
	    }
	case SSL_ERROR_SSL:
#ifdef DEBUG
		printf("SSL_ERROR_SSL\n");
	      fatal_ssl_error(ssl_err, SAFE_SSL_READ, client_p);
#endif	      
	  if (errno == EAGAIN)
	    return -1;
	default:
	  printf("fatal error\n");
	  return fatal_ssl_error (ssl_err, SAFE_SSL_READ, client_p);
	}
    }
  return len;
}

int
safe_SSL_write (struct Client *client_p, const void *buf, int sz)
{
  int len, ssl_err;

printf("ssl write %s %d(%d)\n", buf, strlen(buf), sz);
  len = SSL_write (client_p->localClient->ssl, buf, sz);
printf("len %d\n", len);
  if (len <= 0)
    {
      switch (ssl_err = SSL_get_error (client_p->localClient->ssl, len))
	{
	case SSL_ERROR_SYSCALL:
	  if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
	    {
	case SSL_ERROR_WANT_WRITE:
	      errno = EWOULDBLOCK;
	      return 0;
	    }
	case SSL_ERROR_SSL:
	  if (errno == EAGAIN)
	    return 0;
	default:
	  return fatal_ssl_error (ssl_err, SAFE_SSL_WRITE, client_p);
	}
    }
  return len;
}

int
safe_SSL_accept (struct Client *client_p, int fd)
{

  int ssl_err;

  if ((ssl_err = SSL_accept (client_p->localClient->ssl)) <= 0)
    {
      switch (ssl_err = SSL_get_error (client_p->localClient->ssl, ssl_err))
	{
	case SSL_ERROR_SYSCALL:
	  if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	    /* handshake will be completed later . . */
#ifdef DEBUG
	    fatal_ssl_error(ssl_err, SAFE_SSL_ACCEPT, client_p);
#endif
	    return 0;
	default:
	  return fatal_ssl_error (ssl_err, SAFE_SSL_ACCEPT, client_p);

	}
      /* NOTREACHED */
      return -1;
    }
  return 1;
}

int
SSL_smart_shutdown (struct Client *client_p)
{
  char i;
  int rc;

  rc = 0;
  for (i = 0; i < 4; i++)
    {
      if ((rc = SSL_shutdown (client_p->localClient->ssl)))
	break;
    }

  return rc;
}

static int
fatal_ssl_error (int ssl_error, int where, struct Client *client_p)
{
  /* don`t alter errno */
  int errtmp = errno;
  char *errstr = strerror (errtmp);
  char *ssl_errstr, *ssl_func;

  switch (where)
    {
    case SAFE_SSL_READ:
      ssl_func = "SSL_read()";
      break;
    case SAFE_SSL_WRITE:
      ssl_func = "SSL_write()";
      break;
    case SAFE_SSL_ACCEPT:
      ssl_func = "SSL_accept()";
      break;
    default:
      ssl_func =
	"undefined SSL func [this is a bug] reporto to fish@dynam.ac";
    }

  switch (ssl_error)
    {
    case SSL_ERROR_NONE:
      ssl_errstr = "No error";
      break;
    case SSL_ERROR_SSL:
      ssl_errstr = "Internal OpenSSL error or protocol error";
      break;
    case SSL_ERROR_WANT_READ:
      ssl_errstr = "OpenSSL functions requested a read()";
      break;
    case SSL_ERROR_WANT_WRITE:
      ssl_errstr = "OpenSSL functions requested a write()";
      break;
    case SSL_ERROR_WANT_X509_LOOKUP:
      ssl_errstr = "OpenSSL requested a X509 lookup which didn`t arrive";
      break;
    case SSL_ERROR_SYSCALL:
      ssl_errstr = "Underlying syscall error";
      break;
    case SSL_ERROR_ZERO_RETURN:
      ssl_errstr = "Underlying socket operation returned zero";
      break;
    case SSL_ERROR_WANT_CONNECT:
      ssl_errstr = "OpenSSL functions wanted a connect()";
      break;
    default:
      ssl_errstr = "Unknown OpenSSL error (huh?)";
    }

  sendto_realops_flags(FLAGS_DEBUG,L_ALL, "%s to %s!%s@%s aborted with %serror (%s). [%s]",
		      ssl_func, client_p->name ? client_p->name : "<unknown>",
		      client_p->username, 
		      client_p->host,
		      (errno > 0) ? " " : " no ", errstr, ssl_errstr);
  ilog (L_ERROR, "SSL error in %s: %s [%s]", ssl_func, errstr, ssl_errstr);

  /* if we reply() something here, we might just trigger another
   * fatal_ssl_error() call and loop until a stack overflow...
   * the client won`t get the ERROR : ... string, but this is
   * the only way to do it.
   * IRC protocol wasn`t SSL enabled .. --vejeta
   */

  errno = errtmp ? errtmp : EIO;	/* Stick a generic I/O error */
#if 0
  sptr->sockerr = IRCERR_SSL;
  sptr->flags |= FLAGS_DEADSOCKET;
#endif 
  return -1;
}
#endif
