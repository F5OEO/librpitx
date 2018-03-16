all: testrpitx 

CFLAGS	= -Wall -g -O3 -Wno-unused-variable 
LDFLAGS	= -lm -lrt -lpthread -lliquid
CCP = g++ 
CC = gcc 


testrpitx: testrpitx.cpp ../src/librpitx.h ../src/librpitx.a
	
	$(CCP) $(CFLAGS) -o testrpitx testrpitx.cpp ../src/librpitx.a $(LDFLAGS) 

clean:
	
	rm -f  testrpitx
