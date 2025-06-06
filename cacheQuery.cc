/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2019)
 */

using namespace std;

#include <stdio.h>
#include <string.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "XrdVersion.hh"
#include "XrdOuc/XrdOucCacheCM.hh"
#include "XrdPosix/XrdPosixCache.hh"

//______________________________________________________________________________


XrdPosixCache *myCache;

extern "C" {
XrdOucCacheCMInit_t XrdOucCacheCMInit(XrdPosixCache &Cache,
                                      XrdSysLogger  *Logger,
                                      const char    *Config,
                                      const char    *Parms,
                                      XrdOucEnv     *envP)
{
    myCache = &Cache;
    return (XrdOucCacheCMInit_t)true;
}
};
XrdVERSIONINFO(XrdOucCacheCMInit,CacheCM-4-RUCIO);

int cachedFileIsComplete(std::string url, std::string *localfile)
{
     int rc;
     char pfn[1024];
     char *myurl = strdup(url.c_str());
     rc = myCache->CacheQuery(myurl, 0);
     myCache->CachePath(myurl, pfn, 1024);
     free(myurl);
     *localfile = pfn;
     return rc; 
}

