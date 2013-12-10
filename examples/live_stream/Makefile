CFLAGS=$(OPT) -Wall -W -Wno-parentheses
EXTRA_FLAGS=-I ../../lib/igb/ -I../../daemons/mrpd -I../common
EXTRA_FLAGS+=-L ../../lib/igb/ -ligb -lpci -lrt -lpthread
INCFLAGS=../common/avb.c

all:talker \
	listener

talker:talker.c
	gcc -o talker $(INCFLAGS) talker.c  $(CFLAGS) $(EXTRA_FLAGS)

listener:listener.c
	gcc  -o listener $(INCFLAGS) listener.c $(CFLAGS) $(EXTRA_FLAGS)

clean:
	rm -rf *.o *~ *.rej *.orig
	rm -rf talker listener
