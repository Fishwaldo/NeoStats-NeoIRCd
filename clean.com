$! $Id: clean.com,v 1.1 2002/08/13 14:34:56 fishwaldo Exp $
$! yes, its a hack. this needs to be merged into make.com.
$! (well, really, into a top-level descrip.mms.. one day).
$
$ WRITE SYS$OUTPUT "Cleaning tree from all compiled objects..."
$ DELETE [...]*.EXE;*
$ DELETE [...]*.OLB;*
$ DELETE [...]*.OBJ;*
$ WRITE SYS$OUTPUT "All done."

