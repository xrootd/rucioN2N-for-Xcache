all: XrdOucName2NamePfn2RucioLfn.so

ifeq ($(strip $(XRD_INC)),)
    XRD_INC=/usr/include/xrootd
endif

ifeq ($(strip $(XRD_LIB)),)
XRD_LIB=/usr/lib64
endif

FLAGS=-D_REENTRANT -D_THREAD_SAFE -Wno-deprecated -std=c++0x 

HEADERS=rucioGetMetaLink.hh
SOURCES=XrdOucName2NamePfn2RucioLfn.cc rucioGetMetaLink.cc
OBJECTS=XrdOucName2NamePfn2RucioLfn.o rucioGetMetaLink.o

DEBUG=-g

XrdOucName2NamePfn2RucioLfn.so: $(OBJECTS) Makefile
	g++ ${DEBUG} -shared -fPIC -o $@ $(OBJECTS) -ldl -lssl -lcurl

XrdOucName2NamePfn2RucioLfn.o: XrdOucName2NamePfn2RucioLfn.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

rucioGetMetaLink.o: rucioGetMetaLink.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

clean:
	rm -vf XrdOucName2NamePfn2RucioLfn.{o,so} rucioGetMetaLink.{o,so}


