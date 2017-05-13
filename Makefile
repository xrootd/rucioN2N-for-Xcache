all: XrdOucName2NamePfn2RucioLfn.so

ifeq ($(strip $(XRD_INC)),)
    XRD_INC=/usr/include/xrootd
endif

ifeq ($(strip $(XRD_LIB)),)
XRD_LIB=/usr/lib64
endif

FLAGS=-D_REENTRANT -D_THREAD_SAFE -Wno-deprecated

SOURCES=XrdOucName2NamePfn2RucioLfn.cc
OBJECTS=XrdOucName2NamePfn2RucioLfn.o

DEBUG=-g

XrdOucName2NamePfn2RucioLfn.so: $(OBJECTS) Makefile
	g++ ${DEBUG} -shared -fPIC -o $@ $(OBJECTS) -ldl -lssl

XrdOucName2NamePfn2RucioLfn.o: XrdOucName2NamePfn2RucioLfn.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

clean:
	rm -vf XrdOucName2NamePfn2RucioLfn.{o,so}


