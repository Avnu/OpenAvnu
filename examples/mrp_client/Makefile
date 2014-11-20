OPT=-O2
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses -ggdb

ifeq ($(ARCH),arm)
CC=arm-none-linux-gnueabi-gcc
else
CC=gcc
endif
#INCFLAGS=-I../../lib/igb
#LDLIBS=-ligb -lpci -lz -pthread
#LDFLAGS=-L../../lib/igb

all: mrpq mrpl mrpValidate

mrpl: mrpl.o mrpdclient.o

mrpq: mrpq.o mrpdclient.o

mrpValidate: mrpValidate.o mrpdclient.o

mrpl.o: mrpl.c mrpdclient.h ../../daemons/mrpd/mrpd.h
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpl.c

mrpq.o: mrpq.c mrpdclient.h ../../daemons/mrpd/mrpd.h
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpq.c

mrpValidate.o: mrpValidate.c mrpdclient.h ../../daemons/mrpd/mrpd.h
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpValidate.c

mrpdclient.o: mrpdclient.c mrpdclient.h mrpdhelper.h
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpdclient.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) mrpl mrpq mrpValidate
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`

