# Acorn C 5 / RISC OS 3.1 host build.
# Run in the c/h/s/o tree produced by tools/export-riscos.py.
# C:, OSLib: and OSLibInclude: must refer to the installed libraries.
# TMBLink uses 26-bit returns; compile the C objects with matching APCS.

CCflags = -c -IC: -I@. -IOSLib: -DRISCOS -apcs 3/26bit -throwback
Linkflags = -aif -o $@
ObjAsmflags = -throwback -NoCache

COMMON = @.o.cklib @.o.ro_link @.o.tmblink
TESTOBJS = @.o.testt2 @.o.testt32 @.o.testt414 @.o.testt425 \
           @.o.testt80x @.o.testt800 @.o.testt801 @.o.testlater
HEADERS = @.h.inmos @.h.checklib @.h.cklib

all: @.ispy @.mtest @.ftest

@.ispy: @.o.check $(COMMON) C:o.Stubs OSLib:o.OSLib
        link $(Linkflags) C:o.Stubs @.o.check $(COMMON) OSLib:o.OSLib

@.mtest: @.o.mtest $(COMMON) C:o.Stubs OSLib:o.OSLib
        link $(Linkflags) C:o.Stubs @.o.mtest $(COMMON) OSLib:o.OSLib

@.ftest: @.o.ftest $(TESTOBJS) $(COMMON) C:o.Stubs OSLib:o.OSLib
        link $(Linkflags) C:o.Stubs @.o.ftest $(TESTOBJS) $(COMMON) OSLib:o.OSLib

@.o.check: @.c.check $(HEADERS) @.h.linkio @.h.iserver \
          @.h.type16 @.h.type32 @.h.check16 @.h.check32 @.h.c00416 @.h.c00432
        cc $(CCflags) -o @.o.check @.c.check

@.o.mtest: @.c.mtest $(HEADERS) @.h.mtest16 @.h.mtest32
        cc $(CCflags) -o @.o.mtest @.c.mtest

@.o.ftest: @.c.ftest $(HEADERS) @.h.testcode @.h.test4k @.h.route16 @.h.route32 @.h.peek
        cc $(CCflags) -o @.o.ftest @.c.ftest

@.o.testt2: @.c.testt2 $(HEADERS) @.h.testcode @.h.test222a @.h.test222b
        cc $(CCflags) -o @.o.testt2 @.c.testt2

@.o.testt32: @.c.testt32 $(HEADERS) @.h.testcode @.h.testt32a @.h.testt32b
        cc $(CCflags) -o @.o.testt32 @.c.testt32

@.o.testt414: @.c.testt414 $(HEADERS) @.h.testcode @.h.test414a
        cc $(CCflags) -o @.o.testt414 @.c.testt414

@.o.testt425: @.c.testt425 $(HEADERS) @.h.testcode @.h.test425a
        cc $(CCflags) -o @.o.testt425 @.c.testt425

@.o.testt80x: @.c.testt80x $(HEADERS) @.h.testcode \
             @.h.test80xa @.h.test80xb @.h.test80xc @.h.test80xd
        cc $(CCflags) -o @.o.testt80x @.c.testt80x

@.o.testt800: @.c.testt800 $(HEADERS) @.h.testcode @.h.test800a
        cc $(CCflags) -o @.o.testt800 @.c.testt800

@.o.testt801: @.c.testt801 $(HEADERS) @.h.testcode @.h.test801a
        cc $(CCflags) -o @.o.testt801 @.c.testt801

@.o.testlater: @.c.testlater $(HEADERS) @.h.testcode \
              @.h.probe85 @.h.test85cm @.h.test85st @.h.test85co @.h.test85in \
              @.h.probe45 @.h.test45cm @.h.test45st @.h.test45co @.h.test45in
        cc $(CCflags) -o @.o.testlater @.c.testlater

@.o.cklib: @.c.cklib $(HEADERS) @.h.linkio @.h.boot
        cc $(CCflags) -o @.o.cklib @.c.cklib

@.o.ro_link: @.c.ro_link OSLib:oslib.h.os
        cc $(CCflags) -o @.o.ro_link @.c.ro_link

@.o.tmblink: @.s.tmblink OSLibInclude:Hdr.Types \
            OSLibInclude:Hdr.OS OSLibInclude:Hdr.Podule
        objasm $(ObjAsmflags) -from @.s.tmblink -to @.o.tmblink
