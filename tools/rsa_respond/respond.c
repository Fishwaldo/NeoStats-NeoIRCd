/*
 * tools/rsa_respond/respond.c
 * A simple RSA authentification challenge response generator for the
 * ircd-hybrid CHALLENGE command.
 *  This code is Copyright(C)2001 by the past and present ircd-hybrid
 *  developers.
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  $Id: respond.c,v 1.1 2002/08/13 14:36:50 fishwaldo Exp $
 */
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/md5.h>
#include <unistd.h>

static int pass_cb(char *buf, int size, int rwflag, void *u)
{
	int len;
        char *tmp;
	tmp = getpass("Enter passphrase for challenge: ");
        len = strlen(tmp);
        if (len <= 0) 
		return 0;
        if (len > size)
        	len = size;
        memcpy(buf, tmp, len);
        return len;
}

                                                                                        
static void
binary_to_hex( unsigned char * bin, char * hex, int length )
{
	char * trans = "0123456789ABCDEF";
	int i;

	for( i = 0; i < length; i++ )
	{
		hex[i<<1]     = trans[bin[i] >> 4];
		hex[(i<<1)+1] = trans[bin[i] & 0xf];
	}
	hex[i<<1] = '\0';
}

static int
hex_to_binary(const char *from, char *to, int len)
{
	char a, b=1;
	int p=0;
	const char *ptr = from;
	while (-1)
	{
		a = *ptr++;
		if (!a)
			break;
		b = *ptr++;
		
		/* If this happens, we got bad input. */
		if (!b)
			break;
		if (p >= len)
			break;
		if (!((a >= '0' && a <= '9') || (a >= 'A' && a <= 'F')))
			break;
		if (!((b >= '0' && b <= '9') || (b >= 'A' && b <= 'F')))
			break;
		to[p++] = ((a <= '9') ? (a - '0') : (a - 'A' + 0xA))<<4 |
			((b <= '9') ? (b - '0') : (b - 'A' + 0xA));
	}
	return p;
}

int
main(int argc, char **argv)
{
	FILE *kfile;
	RSA *rsa = NULL;
	char ndata[257], ddata[257];
	/* respond privatefile challenge */
	if (argc < 3)
	{
		puts("Usage: respond privatefile challenge");
		return 0;
	}

	if (!(kfile = fopen(argv[1], "r")))
	{
		puts("Could not open the private keyfile.");
		return 0;
	}
	
	SSLeay_add_all_ciphers();
	rsa = PEM_read_RSAPrivateKey(kfile, NULL,pass_cb, NULL);
  
	if(!rsa)
	{
		puts("Unable to read your private key, is the passphrase wrong?\n");
		return 0;
	}

	 fclose(kfile);
	if (hex_to_binary(argv[2], ndata, 128) != 128)
	{
		puts("Bad challenge.");
		return -1;
	}

	if (RSA_private_decrypt(128, (unsigned char*)ndata,
		(unsigned char*)ddata, rsa, RSA_PKCS1_PADDING) == -1)
	{
		puts("Decryption error.");
		return -1;
	}
	binary_to_hex((unsigned char*)ddata, ndata, 32);
	puts(ndata);
	return 0;
}
