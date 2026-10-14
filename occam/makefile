
#
#   ISPY / MTEST / FTEST / LOAD / CKMON / BOOTPATH Makefile
#

E               = .exe
O               = .obj
MODEL           =
COPTS           = /DMSC /W3 $(MODEL)
OPTIM           = /Oailt
CC              = qcl /c $(OPTIM) $(COPTS)
CCLM            = $(CC)
COPTSLM         = $(COPTS)
LINKMODULE      = b004link
LINKOBJS        = $(LINKMODULE)$(O)
LINK            = qcl /NOE $(OPTIM) -F 2000 -o
RM              = rm
OCCAM		= oc /y /a /n /k /v /e /w /h /T
FIND            = grep
FTESTLINK       = link /NOI /STACK:2000 /CO @ftestb4.lnk
REDIR           = /Fo

all:    check$(E) mtest$(E)
#all:    yuk2h$(E) ispy$(E) mtest$(E) ftest$(E) load$(E) ckmon$(E)

TESTSRCS=test2.occ test414.occ test425.occ test32.occ test80x.occ test800.occ test801.occ
TESTHSRCS=test2.h test414.h test425.h test32.h test80x.h test800.h test801.h
CSRCS= *.c checklib.h cklib.h
OCCSRCS=check*.occ mtest*.occ ftest*.occ

archive:
	zip checklon.zip $(OCCSRCS) $(CSRCS)

# this makefile in 'included' by another makefile


TESTOBJS=test2$(O) test414$(O) test425$(O) test32$(O) test80x$(O) test800$(O) test801$(O)

#
#  ispy
#

check$(E):      check$(O) cklib$(O) $(LINKOBJS)
		$(LINK) check$(E) check$(O) cklib$(O) $(LINKOBJS)

check$(O):      check.c readcfb.h checklib.h inmos.h cklib.h \
		type32.h type16.h check32.h check16.h c00416.h c00432.h \
		iserver.h
		$(CC) $(REDIR)check$(O) check.c

type32.h:       type32.occ checklib.occ 
		$(OCCAM)A type32
		ilist /c type32.tco | perl tco2h.pl > type32.h
		$(FIND) "total code requirement"  type32.h
		$(RM) type32.tco

type16.h:       type16.occ checklib.occ 
		$(OCCAM)2 type16
		ilist /c type16.tco | perl tco2h.pl > type16.h
		$(FIND) "total code requirement" < type16.h
		$(RM) type16.tco

check32.h:      check32.occ checklib.occ 
		$(OCCAM)A check32
		ilist /c check32.tco | perl tco2h.pl > check32.h
		$(FIND) "total code requirement" < check32.h
		$(RM) check32.tco

check16.h:      check16.occ checklib.occ 
		$(OCCAM)2 check16
		ilist /c check16.tco | perl tco2h.pl > check16.h
		$(FIND) "total code requirement" < check16.h
		$(RM) check16.tco

c00432.h:       c00432.occ checklib.occ 
		$(OCCAM)A c00432
		ilist /c c00432.tco | perl tco2h.pl > c00432.h
		$(FIND) "total code requirement" < c00432.h
		$(RM) c00432.tco

c00416.h:       c00416.occ checklib.occ 
		$(OCCAM)2 c00416
		ilist /c c00416.tco | perl tco2h.pl > c00416.h
		$(FIND) "total code requirement" < c00416.h
		$(RM) c00416.tco



#
#  mtest
#

mtest$(E):      mtest$(O) cklib$(O) $(LINKOBJS)
		$(LINK) mtest$(E) mtest$(O) cklib$(O) $(LINKOBJS)

mtest$(O):      mtest.c cklib.h inmos.h  \
		mtest32.h mtest16.h checklib.h
		$(CC) $(REDIR)mtest$(O) mtest.c

mtest32.h:      mtest32.occ checklib.occ 
		$(OCCAM)A mtest32
		ilist /c mtest32.tco | perl tco2h.pl > mtest32.h
		$(FIND) "total code requirement" < mtest32.h
		$(RM) mtest32.tco

mtest16.h:      mtest16.occ checklib.occ 
		$(OCCAM)2 mtest16
		ilist /c mtest16.tco | perl tco2h.pl > mtest16.h
		$(FIND) "total code requirement" < mtest16.h
		$(RM) mtest16.tco

#
#  ftest
#

ftest$(E):      ftest$(O) cklib$(O) $(LINKOBJS) $(TESTOBJS)
		$(FTESTLINK)

ftest$(O):      ftest.c testcode.h inmos.h cklib.h checklib.h route32.h route16.h test4k.h
		$(CC) $(REDIR)ftest$(O) ftest.c 

test2$(O):      testcode.h inmos.h checklib.h cklib.h test222a.h test222b.h test2.c
		$(CC) $(REDIR)test2$(O) test2.c 

test32$(O):     testcode.h inmos.h checklib.h cklib.h testt32a.h testt32b.h test32.c
		$(CC) $(REDIR)test32$(O) test32.c 

test414$(O):    testcode.h inmos.h checklib.h cklib.h test414a.h test414.c
		$(CC) $(REDIR)test414$(O) test414.c

test425$(O):    testcode.h inmos.h checklib.h cklib.h test425a.h test425.c
		$(CC) $(REDIR)test425$(O) test425.c

test80x$(O):    testcode.h inmos.h checklib.h cklib.h \
		test80xa.h test80xb.h test80xc.h test80xd.h test80x.c
		$(CC) $(REDIR)test80x$(O) test80x.c

test800$(O):    testcode.h inmos.h checklib.h cklib.h test800a.h test800.c
		$(CC) $(REDIR)test800$(O) test800.c

test801$(O):    testcode.h inmos.h checklib.h cklib.h test801a.h test801.c
		$(CC) $(REDIR)test801$(O) test801.c

testt32a.h:     testt32a.occ checklib.occ 
		$(OCCAM)A testt32a
		$(YUK) testt32a.tco > testt32a.h
		$(FIND) "total code requirement" < testt32a.h
		$(RM) testt32a.tco

testt32b.h:     testt32b.occ checklib.occ 
		$(OCCAM)A testt32b
		$(YUK) testt32b.tco > testt32b.h
		$(FIND) "total code requirement" < testt32b.h
		$(RM) testt32b.tco

test222a.h:     test222a.occ checklib.occ 
		$(OCCAM)2 test222a
		$(YUK) test222a.tco > test222a.h
		$(FIND) "total code requirement" < test222a.h
		$(RM) test222a.tco

test222b.h:     test222b.occ checklib.occ 
		$(OCCAM)2 test222b
		$(YUK) test222b.tco > test222b.h
		$(FIND) "total code requirement" < test222b.h
		$(RM) test222b.tco

test414a.h:     test414a.occ checklib.occ 
		$(OCCAM)4 test414a
		$(YUK) test414a.t4u > test414a.h
		$(FIND) "total code requirement" < test414a.h
		$(RM) test414a.t4u

test425a.h:     test425a.occ checklib.occ 
		$(OCCAM)5 test425a
		$(YUK) test425a.t5u > test425a.h
		$(FIND) "total code requirement" < test425a.h
		$(RM) test425a.t5u

test80xa.h:     test80xa.occ checklib.occ 
		$(OCCAM)8 test80xa
		$(YUK) test80xa.t8u > test80xa.h
		$(FIND) "total code requirement" < test80xa.h
		$(RM) test80xa.t8u

test800a.h:     test800a.occ checklib.occ 
		$(OCCAM)8 test800a
		$(YUK) test800a.t8u > test800a.h
		$(FIND) "total code requirement" < test800a.h
		$(RM) test800a.t8u

test801a.h:     test801a.occ checklib.occ 
		$(OCCAM)8 test801a
		$(YUK) test801a.t8u > test801a.h
		$(FIND) "total code requirement" < test801a.h
		$(RM) test801a.t8u

test80xb.h:     test80xb.occ checklib.occ 
		$(OCCAM)8 test80xb
		$(YUK) test80xb.t8u > test80xb.h
		$(FIND) "total code requirement" < test80xb.h
		$(RM) test80xb.t8u

test80xc.h:     test80xc.occ checklib.occ 
		$(OCCAM)8 test80xc
		$(YUK) test80xc.t8u > test80xc.h
		$(FIND) "total code requirement" < test80xc.h
		$(RM) test80xc.t8u

test80xd.h:     test80xd.occ checklib.occ 
		$(OCCAM)8 test80xd
		$(YUK) test80xd.t8u > test80xd.h
		$(FIND) "total code requirement" < test80xd.h
		$(RM) test80xd.t8u

route16.h:      route16.occ router.occ checklib.occ 
		$(OCCAM)2 route16
		$(YUK) route16.tco > route16.h
		$(FIND) "total code requirement" < route16.h
		$(RM) route16.tco

route32.h:      route32.occ router.occ checklib.occ 
		$(OCCAM)A route32
		$(YUK) route32.tco > route32.h
		$(FIND) "total code requirement" < route32.h
		$(RM) route32.tco

test4k.h:       test4k.occ checklib.occ 
		$(OCCAM)A test4k
		$(YUK) test4k.tco > test4k.h
		$(FIND) "total code requirement" < test4k.h
		$(RM) test4k.tco


#
#  load
#

load$(E):       load$(O) cklib$(O) $(LINKOBJS)
		$(LINK) load$(E) load$(O) cklib$(O) $(LINKOBJS)
 
load$(O):       load.c cklib.h inmos.h route32.h route16.h
		$(CC) $(REDIR)load$(O) load.c
		
#
#  ckmon
#

ckmon$(E):      ckmon$(O) screen$(O) cklib$(O) $(LINKOBJS)
		$(LINK) ckmon$(E) ckmon$(O) screen$(O) cklib$(O) $(LINKOBJS)

ckmon$(O):      ckmon.c link.h screen.h inmos.h opcodes.h \
		cklib.h boot.h peek.h pp32.h pp16.h
		$(CC) $(REDIR)ckmon$(O) ckmon.c

screen$(O):     screen.c screen.h
		$(CC) $(REDIR)screen$(O) screen.c

pp32.h:         pp32.occ peekpoke.occ router.occ checklib.occ 
		$(OCCAM)A pp32
		$(YUK) pp32.tco > pp32.h
		$(FIND) "total code requirement" < pp32.h
		$(RM) pp32.tco

pp16.h:         pp16.occ peekpoke.occ router.occ checklib.occ 
		$(OCCAM)2 pp16
		$(YUK) pp16.tco > pp16.h
		$(FIND) "total code requirement" < pp16.h
		$(RM) pp16.tco
#
#  support routines
#

cklib$(O):      cklib.c cklib.h checklib.h
		$(CC) $(REDIR)cklib$(O) cklib.c

hostend$(O):    hostend.c inmos.h \
		iserver.h pack.h
		$(CC) $(REDIR)hostend$(O) hostend.c
		
#
#  link code
#

$(LINKMODULE)$(O):      $(LINKMODULE).c inmos.h
		$(CCLM) $(COPTSLM) $(LINKMODULE).c $(REDIR)$(LINKMODULE)$(O)

			

#
#  eof
#
