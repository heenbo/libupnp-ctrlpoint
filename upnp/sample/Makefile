.PHONY: all clean
CROSS_COMPILE = 
CC = gcc
SOURCE = main.c ctrlpoint.c
PROJECT = main.o ctrlpoint.o
OBJECT = main

LDLIB = -L/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/upnp/.libs -L/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/ixml/.libs -lixml -lupnp -pthread
CFLAGS = -g -Wall -D_GNU_SOURCE -I/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/upnp/inc -I/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/ixml/inc -I./include -I/usr/include/

all : $(OBJECT)

$(OBJECT):$(PROJECT)
	${CROSS_COMPILE}${CC} $^ -o $@ ${LDLIB}
$(PROJECT):$(SOURCE)
	${CROSS_COMPILE}${CC} $^ -c ${CFLAGS}
	
clean : 
	-rm -f *.o main
