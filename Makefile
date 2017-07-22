all: XrdName2NameDCP4RUCIO.so

ifeq ($(strip $(XRD_INC)),)
    XRD_INC=/usr/include/xrootd
endif

ifeq ($(strip $(XRD_LIB)),)
    XRD_LIB=/usr/lib64
endif

FLAGS=-D_REENTRANT -D_THREAD_SAFE -Wno-deprecated -std=c++0x 

HEADERS=rucioGetMetaLink.hh pfn2cache.hh
SOURCES=XrdOucName2NameDCP4RUCIO.cc XrdOssStatInfoDCP.cc rucioGetMetaLink.cc pfn2cache.cc 
OBJECTS=XrdOucName2NameDCP4RUCIO.o XrdOssStatInfoDCP.o rucioGetMetaLink.o pfn2cache.o

DEBUG=-g

XrdName2NameDCP4RUCIO.so: $(OBJECTS) Makefile
	g++ ${DEBUG} -shared -fPIC -o $@ $(OBJECTS) -ldl -lssl -lcurl

XrdOucName2NameDCP4RUCIO.o: XrdOucName2NameDCP4RUCIO.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

XrdOssStatInfoDCP.o: XrdOssStatInfoDCP.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

rucioGetMetaLink.o: rucioGetMetaLink.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

pfn2cache.o: pfn2cache.cc ${HEADERS} Makefile
	g++ ${DEBUG} ${FLAGS} -fPIC -I ${XRD_INC} -I ${XRD_LIB} -c -o $@ $<

clean:
	rm -vf XrdName2NameDCP4RUCIO.{o,so} rucioGetMetaLink.{o,so}


