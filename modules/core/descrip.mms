# MMS/MMK Makefile for OpenVMS
# Copyright (c) 2001 Edward Brocklesby
# $Id: descrip.mms,v 1.1 2002/08/14 05:47:41 fishwaldo Exp $

CC=	CC
CFLAGS=	/INCLUDE_DIRECTORY=([-.-.INCLUDE],[-.-.ADNS])/STANDARD=ISOC94
LDFLAGS=	

OBJECTS=	M_DIE.OBJ,M_KICK.OBJ,M_KILL.OBJ,M_MESSAGE.OBJ,M_MODE.OBJ,M_NICK.OBJ,-
		M_PART.OBJ,M_QUIT.OBJ,M_SERVER.OBJ,M_SJOIN.OBJ,M_SQUIT.OBJ

ALL : CORE.OLB($(OBJECTS))
	COPY CORE.OLB [-]

CLEAN :
	DELETE *.OLB;*
	DELETE *.OBJ;*

