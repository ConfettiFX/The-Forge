# **********************************************************************
#
# Copyright (c) 2015-2017 ZeroC, Inc. All rights reserved.
#
# **********************************************************************

PREFIX ?= /opt/mcpp-2.7.2

ifeq ($(CFLAGS),)
	CFLAGS = -O2
endif

UNAME = $(shell uname)
MACHINE = $(shell uname -m)

LIBDIR = lib

ifeq ($(UNAME),Darwin)
        override CFLAGS += -mmacosx-version-min=10.9
endif

ifeq ($(UNAME),Linux)
        override CFLAGS += -fPIC -Wno-maybe-uninitialized -Wno-implicit-function-declaration -Wno-unused-result

        ifeq ($(MACHINE),i686)
                override CFLAGS += -m32
                #
                # Ubuntu.
                #
                ifeq ($(shell test -d /usr/lib/i386-linux-gnu && echo 0),0)
                        LIBDIR = lib/i386-linux-gnu
                endif
        else
                #
                # Ubuntu.
                #
                ifeq ($(shell test -d /usr/lib/x86_64-linux-gnu && echo 0),0)
                        LIBDIR = lib/x86_64-linux-gnu
                endif
                #
                # Rhel/SLES
                #
                ifeq ($(shell test -d /usr/lib64 && echo 0),0)
                        LIBDIR = lib64
                endif
        endif
endif

ifeq ($(UNAME),AIX)
        CC ?= xlc_r
endif

OBJS = directive.o eval.o expand.o main.o mbchar.o support.o system.o

$(LIBDIR)/libmcpp.a: $(OBJS)
	-mkdir -p $(LIBDIR)
	ar rcs $(LIBDIR)/libmcpp.a $(OBJS)
	ranlib $(LIBDIR)/libmcpp.a

install: $(LIBDIR)/libmcpp.a
	@mkdir -p $(PREFIX)/$(LIBDIR)
	cp $(LIBDIR)/libmcpp.a $(PREFIX)/$(LIBDIR)

clean:
	rm -f $(OBJS)
	rm -rf $(LIBDIR)
