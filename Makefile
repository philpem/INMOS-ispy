
#
#   ISPY / MTEST / FTEST / LOAD / CKMON / BOOTPATH Makefile
#

E               =
O               = .o
MODEL           =
COPTS           = $(MODEL) -Wall
OPTIM           = -O
CC              = gcc -c $(OPTIM) $(COPTS)
CCLM            = $(CC)
COPTSLM         = $(COPTS)
LINKMODULE      = b004link
DEFAULTLINK	= \"/dev/link0\"
LINKOBJS        = $(LINKMODULE)$(O)
LINK            = gcc $(OPTIM)
LIBRARIES       =
RM              = rm
OCCAM		= oc -y -a -n -k -v -e -w -h -T
FIND            = grep

.PHONY: all clean occam check test-host

all:     ispy$(E) mtest$(E) ftest$(E) netcfg$(E) ltest$(E)

FTESTOBJS = ftest$(O) testt2$(O) testt32$(O) testt414$(O) testt425$(O) \
            testt80x$(O) testt800$(O) testt801$(O) testlater$(O)

# Host regressions use checked-in blobs, without DOSBox or hardware.
check test-host:
		python3 -m unittest discover -s tests -v

TESTSRCS=test2.occ test414.occ test425.occ test80x.occ test800.occ test801.occ
TESTHSRCS=test2.h test414.h test425.h test32.h test80x.h test800.h test801.h
CSRCS= *.c checklib.h cklib.h
OCCSRCS=check*.occ mtest*.occ

archive:
	zip check.zip $(OCCSRCS) $(CSRCS)

# this makefile in 'included' by another makefile

clean:
		rm -f *.o ispy mtest ftest netcfg ltest .occam.stamp

#
#  occam code-blob headers
#
#  Compiles occam/*.occ with the original INMOS D7305 toolset under DOSBox and
#  regenerates type16.h/type32.h/... with tools/tco2h.py.  The toolset archive
#  is downloaded on demand (see tools/fetch-toolchain.sh). Ordinary builds use
#  the checked-in headers. Request `make occam all` to regenerate and relink.
#

occam: .occam.stamp

# If regeneration and host targets are requested together, wait for the new
# headers before deciding whether the embedding objects need recompilation.
ifneq ($(filter occam,$(MAKECMDGOALS)),)
check$(O) mtest$(O) ltest$(O) $(FTESTOBJS): .occam.stamp
endif

.occam.stamp: $(wildcard occam/*.occ) tools/occam-build.sh tools/tco2h.py tools/fetch-toolchain.sh
		tools/occam-build.sh $(CURDIR)
		touch $@

#
#  ispy
#

ispy$(E):      check$(O) cklib$(O) $(LINKOBJS)
		$(LINK) -o ispy$(E) check$(O) cklib$(O) $(LINKOBJS) $(LIBRARIES)

check$(O):      check.c checklib.h inmos.h cklib.h \
		type32.h type16.h check32.h check16.h c00416.h c00432.h iserver.h
		$(CC) -DDEFAULTLINK=$(DEFAULTLINK) -o check$(O) check.c


#
#  mtest
#

mtest$(E):      mtest$(O) cklib$(O) $(LINKOBJS)
		$(LINK) -o mtest$(E) mtest$(O) cklib$(O) $(LINKOBJS)

mtest$(O):      mtest.c cklib.h inmos.h  \
		mtest32.h mtest16.h size16.h size32.h checklib.h
		$(CC) -o mtest$(O) mtest.c

#
#  ftest
#

ftest$(E): $(FTESTOBJS) cklib$(O) $(LINKOBJS)
		$(LINK) -o $@ $(FTESTOBJS) cklib$(O) $(LINKOBJS) $(LIBRARIES)

$(FTESTOBJS): inmos.h checklib.h cklib.h testcode.h

ftest$(O): ftest.c route16.h route32.h test4k.h peek.h
		$(CC) -o $@ ftest.c

testt2$(O): testt2.c test222a.h test222b.h
		$(CC) -o $@ testt2.c

testt32$(O): testt32.c testt32a.h testt32b.h
		$(CC) -o $@ testt32.c

testt414$(O): testt414.c test414a.h
		$(CC) -o $@ testt414.c

testt425$(O): testt425.c test425a.h
		$(CC) -o $@ testt425.c

testt80x$(O): testt80x.c test80xa.h test80xb.h test80xc.h test80xd.h
		$(CC) -o $@ testt80x.c

testt800$(O): testt800.c test800a.h
		$(CC) -o $@ testt800.c

testt801$(O): testt801.c test801a.h
		$(CC) -o $@ testt801.c

testlater$(O): testlater.c probe85.h test85cm.h test85st.h test85co.h test85in.h \
              probe45.h test45cm.h test45st.h test45co.h test45in.h
		$(CC) -o $@ testlater.c


netcfg$(E): netcfg$(O) cklib$(O) $(LINKOBJS)
		$(LINK) -o $@ netcfg$(O) cklib$(O) $(LINKOBJS)

netcfg$(O): netcfg.c cklib.h inmos.h checklib.h
		$(CC) -o $@ netcfg.c

#
#  ckmon
#

ckmon$(E):      ckmon$(O) screen$(O) cklib$(O) $(LINKOBJS)
		$(LINK) -o ckmon$(E) ckmon$(O) screen$(O) cklib$(O) $(LINKOBJS)

ckmon$(O):      ckmon.c link.h screen.h inmos.h opcodes.h \
		cklib.h boot.h peek.h pp32.h pp16.h
		$(CC) -o ckmon$(O) ckmon.c

screen$(O):     screen.c screen.h
		$(CC) screen$(O) screen.c

#
#  support routines
#

cklib$(O):      cklib.c cklib.h checklib.h boot.h
		$(CC) -o cklib$(O) cklib.c

hostend$(O):    hostend.c inmos.h \
		iserver.h pack.h
		$(CC) -o hostend$(O) hostend.c
		
#
#  link code
#

$(LINKMODULE)$(O):      $(LINKMODULE).c inmos.h
		$(CCLM) $(COPTSLM) $(LINKMODULE).c -o $(LINKMODULE)$(O)

			

#
#  end of C makefile
#
#
#RM              = rm
#OCCAM		= oc -y -a -n -k -v -e -w -h -T
#FIND            = grep
#
#
#type32.h:       type32.occ checklib.occ 
#		$(OCCAM)A type32
#		ilist -c type32.tco | perl tco2h.pl > type32.h
#		$(FIND) "total code requirement"  type32.h
#		$(RM) type32.tco
#
#type16.h:       type16.occ checklib.occ 
#		$(OCCAM)2 type16
#		ilist -c type16.tco | perl tco2h.pl > type16.h
#		$(FIND) "total code requirement" < type16.h
#		$(RM) type16.tco
#
#check32.h:      check32.occ checklib.occ 
#		$(OCCAM)A check32
#		ilist -c check32.tco | perl tco2h.pl > check32.h
#		$(FIND) "total code requirement" < check32.h
#		$(RM) check32.tco
#
#check16.h:      check16.occ checklib.occ 
#		$(OCCAM)2 check16
#		ilist -c check16.tco | perl tco2h.pl > check16.h
#		$(FIND) "total code requirement" < check16.h
#		$(RM) check16.tco
#
# c00432.h:       c00432.occ checklib.occ 
# 		$(OCCAM)A c00432
# 		ilist -c c00432.tco | perl tco2h.pl > c00432.h
# 		$(FIND) "total code requirement" < c00432.h
# 		$(RM) c00432.tco
# 
# c00416.h:       c00416.occ checklib.occ 
# 		$(OCCAM)2 c00416
# 		ilist -c c00416.tco | perl tco2h.pl > c00416.h
# 		$(FIND) "total code requirement" < c00416.h
# 		$(RM) c00416.tco
# 
#
#  mtest
#
#
#mtest32.h:      mtest32.occ checklib.occ 
#		$(OCCAM)A mtest32
#		ilist -c mtest32.tco | perl tco2h.pl > mtest32.h
#		$(FIND) "total code requirement" < mtest32.h
#		$(RM) mtest32.tco
#
#mtest16.h:      mtest16.occ checklib.occ 
#		$(OCCAM)2 mtest16
#		ilist -c mtest16.tco | perl tco2h.pl > mtest16.h
#		$(FIND) "total code requirement" < mtest16.h
#		$(RM) mtest16.tco
#
#
#  eof
#

ltest$(E): ltest$(O) cklib$(O) $(LINKOBJS)
		$(LINK) -o $@ ltest$(O) cklib$(O) $(LINKOBJS)

ltest$(O): ltest.c cklib.h inmos.h checklib.h ltest16.h ltest32.h
		$(CC) -o $@ ltest.c
