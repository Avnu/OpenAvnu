OBJS=igb
INCL=e1000_82575.h e1000_defines.h e1000_hw.h e1000_osdep.h e1000_regs.h igb.h
AVBLIB=libigb.a
#CFLAGS=-ggdb

all: $(AVBLIB)

$(AVBLIB): $(addsuffix .o,$(OBJS))
	rm -f $@
	ar rcs $@ $^
	ranlib $@

igb.o: igb.c $(INCL)
	gcc -c $(INCFLAGS) $(CFLAGS) igb.c

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`


