# Native Norcroft C 5 / RISC OS 3.1 build in the exported c/h/o layout.
# Generate the export with export.py; run amu in its directory.
CCflags = -c -IC: -I@. -IOSLib: -DRISCOS -apcs 3/26bit -throwback
OBJS = @.o.main @.o.graph @.o.quality @.o.examples @.o.job @.o.present @.o.hardware @.o.discover @.o.support @.o.driver @.o.tmblink
HEADERS = @.h.inmos @.h.linkio @.h.checklib @.h.cklib

all: @.!Ispy.!RunImage

@.!Ispy.!RunImage: $(OBJS) C:o.Stubs OSLib:o.OSLib
        link -aif -o @.!Ispy.!RunImage C:o.Stubs $(OBJS) OSLib:o.OSLib

@.o.main: @.c.main @.h.graph @.h.job @.h.hardware @.h.present
        cc $(CCflags) -o @.o.main @.c.main

@.o.graph: @.c.graph @.h.graph
        cc $(CCflags) -o @.o.graph @.c.graph

@.o.job: @.c.job @.h.job
        cc $(CCflags) -o @.o.job @.c.job

@.o.hardware: @.c.hardware @.h.hardware @.h.graph $(HEADERS) @.h.size16 @.h.size32
        cc $(CCflags) -o @.o.hardware @.c.hardware

@.o.discover: @.c.discover @.h.discover @.h.hwlegacy $(HEADERS) @.h.iserver @.h.type16 @.h.type32 @.h.check16 @.h.check32 @.h.c00416 @.h.c00432
        cc $(CCflags) -o @.o.discover @.c.discover

@.o.support: @.c.support @.h.support @.h.hwlegacy $(HEADERS) @.h.boot
        cc $(CCflags) -o @.o.support @.c.support

@.o.driver: @.c.driver @.h.driver @.h.hwlegacy
        cc $(CCflags) -o @.o.driver @.c.driver

@.o.tmblink: @.s.tmblink OSLibInclude:Hdr.Types OSLibInclude:Hdr.OS OSLibInclude:Hdr.Podule
        objasm -throwback -NoCache -from @.s.tmblink -to @.o.tmblink

@.o.present: @.c.present @.h.present @.h.graph
        cc $(CCflags) -o @.o.present @.c.present

@.o.quality: @.c.quality @.h.graph
        cc $(CCflags) -o @.o.quality @.c.quality

@.o.examples: @.c.examples @.h.graph
        cc $(CCflags) -o @.o.examples @.c.examples
