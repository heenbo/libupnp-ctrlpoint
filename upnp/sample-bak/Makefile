.PHONY: all clean
CROSS_COMPILE = 
CC = gcc

LDLIB = -L/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/upnp/.libs -L/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/ixml/.libs -lixml -lupnp -pthread
CFLAGS = -g -Wall -I/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/upnp/inc -I/home/heenbo/work/test/test_upnp_lib-2/libupnp-1.6.20/ixml/inc -I./include -I/usr/include/

all : main

main : main.o
	${CROSS_COMPILE}${CC} $^ -o $@ ${LDLIB}
main.o : main.c 
	${CROSS_COMPILE}${CC} -c $< -o $@ ${CFLAGS}
	
clean : 
	-rm -f *.o main
