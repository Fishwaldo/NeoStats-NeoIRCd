/*
 *  NeoIRCd: NeoStats Group. Based on Hybird7
 *  ssl.h: The ssl header.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *  Originally from Ultimate3, modified to work with NeoIRCd
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
 *  $Id: ssl.h,v 1.3 2003/05/09 12:30:19 fishwaldo Exp $
 */

#ifndef SSL_H
#define SSL_H

#ifdef USE_SSL
/* we dont need/want kerberos support */
#define OPENSSL_NO_KRB5 1
#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <openssl/rsa.h>       /* OpenSSL stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "client.h"

extern char ssl_cpath[BUFSIZE+1];
extern char ssl_kpath[BUFSIZE+1];

int safe_SSL_read(struct Client *, void *, int);
int safe_SSL_write(struct Client *, const void *, int);
int safe_SSL_accept(struct Client *, int);
int SSL_smart_shutdown(struct Client *);
int initssl(void);

#endif
#endif
