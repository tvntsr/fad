CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS  = -std=c++17 -g3 -Wall -Winline -pipe -flto 

LDLIBS = -lboost_coroutine -lboost_context -lboost_program_options -lboost_system -pthread

TARGET_DIR = /usr/local/bin/

SRCS=fad.cpp log.cpp config.cpp fanotify.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

.PHONY: all
all: fad

fad: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	$(RM) ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

.PHONY: clean
clean:
	$(RM) $(OBJS) fad

.PHONY: distclean
distclean: clean
	$(RM) *~ .depend

.PHONY: install
install:
	cp $(BINS) $(TARGET_DIR)

.PHONY: uninstall
uninstall:
	rm -f $(TARGET_DIR)$(BINS)

include .depend
