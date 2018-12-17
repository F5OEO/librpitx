CFLAGS = -Wall -O3 -Wno-unused-variable
CXXFLAGS = -std=c++11 -Wall -O3 -Wno-unused-variable
LDFLAGS = -lm -lrt -lpthread
CCP = c++
CC = cc

librpitx: librpitx.h gpio.h gpio.cpp dma.h dma.cpp mailbox.c raspberry_pi_revision.c fmdmasync.h fmdmasync.cpp ngfmdmasync.h ngfmdmasync.cpp dsp.h dsp.cpp iqdmasync.h iqdmasync.cpp serialdmasync.h serialdmasync.cpp phasedmasync.h phasedmasync.cpp fskburst.h fskburst.cpp ookburst.cpp ookburst.h
	$(CC) $(CFLAGS) -c -o mailbox.o mailbox.c
	$(CC) $(CFLAGS) -c -o raspberry_pi_revision.o raspberry_pi_revision.c
	$(CCP) $(CXXFLAGS) -c dsp.cpp iqdmasync.cpp ngfmdmasync.cpp fmdmasync.cpp  dma.cpp gpio.cpp serialdmasync.cpp phasedmasync.cpp amdmasync.h amdmasync.cpp fskburst.cpp ookburst.cpp
	$(AR) rc librpitx.a dsp.o iqdmasync.o ngfmdmasync.o fmdmasync.o  dma.o gpio.o mailbox.o raspberry_pi_revision.o  serialdmasync.o phasedmasync.o amdmasync.o fskburst.o ookburst.o

install: librpitx

clean:
	rm -f *.o *.a
