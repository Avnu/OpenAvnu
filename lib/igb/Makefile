OBJS=igb
INCL=e1000_82575.h e1000_defines.h e1000_hw.h e1000_osdep.h e1000_regs.h igb.h
AVBLIB=libigb.a
#CFLAGS=-ggdb

CC?=gcc
RANLIB?=ranlib

all: $(AVBLIB)

$(AVBLIB): $(addsuffix .o,$(OBJS))
	$(RM) $@
	$(AR) rcs $@ $^
	$(RANLIB) $@

igb.o: igb.c $(INCL)
	$(CC) -c $(INCFLAGS) $(CFLAGS) igb.c

clean:
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`


