# Copyright (c) 2008-2010 VMware, Inc. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

PROGRAM = ioblazer
CFILES = ioblazer.c
OBJS = $(subst .c,.o,$(CFILES))
CC = gcc
LD = gcc
OS := $(shell uname -s)
#MACHINE := $(shell uname -m)
MACHINE = x86_64

ifeq (${DEBUG}, true)
   DBGFLAGS = -D__DEBUG__ -ggdb
endif

ifeq (${MACHINE}, x86_64)
   ARCHFLAG = -m64
else
   ARCHFLAG = -m32
endif

ifeq (${OS}, Darwin)
   CFLAGS = -c ${ARCHFLAG} -Wall -Werror -D__OSX__ -D_LARGEFILE64_SOURCE ${DBGFLAGS}
   LDFLAGS = -arch ${MACHINE} ${DBGFLAGS}
   LIBS = -lpthread
else ifeq (${OS}, Linux)
   CFLAGS = -c ${ARCHFLAG} -Wall -Werror -D__LINUX__ -D_LARGEFILE64_SOURCE ${DBGFLAGS}
   LDFLAGS = ${ARCHFLAG} ${DBGFLAGS}
   LIBS = -laio -lpthread
endif


$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) -o $(PROGRAM) $(OBJS) $(LIBS)
	
$(OBJS): $(CFILES) Makefile
	$(CC) $(CFLAGS) $(CFILES)

clean:
	rm -f $(PROGRAM) $(OBJS)

all: $(PROGRAM)
