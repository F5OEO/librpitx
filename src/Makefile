CFLAGS ?= -Wall -O3 -Wno-unused-variable
CFLAGS += -std=c++11 -fPIC
CXXFLAGS ?= -std=c++11 -Wall -O3 -Wno-unused-variable -I /opt/vc/include 
CXXFLAGS += -std=c++11 -Wall -O3 -Wno-unused-variable -fPIC
LDFLAGS ?= -lm -lrt -lpthread -L/opt/vc/lib -lbcm_host
LDFLAGS += -fPIC
CXX ?= c++
CC ?= cc
PREFIX ?= /usr/local/
SRCCC=$(wildcard *.c)
SRCXX=$(wildcard *.cpp)
OBJS=$(SRCCC:.c=.o)
OBJS+=$(SRCXX:.cpp=.o)

all: librpitx.a librpitx.so

%.o: %.c
	$(CXX) $(CFLAGS) -c $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^

librpitx.a: $(OBJS)
	$(AR) rc $@ $^

librpitx.so: $(OBJS)
	$(CXX) -fPIC -shared -o $@ $^ $(LDFLAGS) 

install: librpitx.a
	mkdir -p $(PREFIX)/include/librpitx
	install *.h $(PREFIX)/include/librpitx
	mkdir -p $(PREFIX)/lib
	install librpitx.a $(PREFIX)/lib

clean:
	rm -f $(OBJS) *.a *.so
