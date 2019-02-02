/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2017)
 */

using namespace std;

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>

#include "pfn2cache.hh"
#include "XrdVersion.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssStatInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"

static std::string gLFNprefix = "/atlas/rucio";  // default

extern "C" {

int XrdOssStatInfo(const char *path, struct stat *buff,
                   int         opts, XrdOucEnv   *envP)
{
     std::string cachePath, osslocalroot, tmp;
     std::size_t i, j;

     tmp = path;
     i = tmp.rfind(gLFNprefix);
     if (i != string::npos) 
     {   // the "path" may be like this: /data/xrd/namespace/atlas/rucio/scope/xx/xx/file
         osslocalroot = tmp.substr(0, i);
         cachePath = osslocalroot + pfn2cache("", gLFNprefix, tmp.substr(i, tmp.length() -i).c_str());
     }
     else 
     {
         i = tmp.find("/root:/");
         if (i == string::npos) i = tmp.find("/http:/");
         if (i == string::npos) i = tmp.find("/https:/");
      
         if (i != string::npos)
         {   // the "path" may be like this: /data/xrd/namespace/root:/host:port/...
             osslocalroot = tmp.substr(0, i);
             j = tmp.rfind("/rucio/");
             if (j != string::npos) // the "path" may be like this: /data/xrd/namespace/root:/host:port/xrootd/rucio/scope/xx/xx/file
                 cachePath = osslocalroot + pfn2cache("", gLFNprefix, tmp.substr(j, tmp.length() -j).c_str());
             else // the "path" may be like this: /data/xrdname/root:/host:port/xrootd/junk (no "/rucio")
             {
                 tmp.replace(0, i, "");  // tmp is now /root:/host:port/xrootd/junk
                 i = tmp.find(":/");
                 tmp.replace(0, i +2, "");;  // tmp is now host:port/xrootd/junk
                 i = tmp.find("/");
                 tmp.replace(0, i, "");;  // tmp is now /xrootd/junk
                 cachePath = osslocalroot + tmp;
             }
         }
         else // the "path" maybe like this: /data/xrd/namespace/tmp/README
             cachePath = path;
     }
     return (stat(cachePath.c_str(), buff)? -1 : 0);
}

XrdOssStatInfo_t XrdOssStatInfoInit(XrdOss        *native_oss,
                                    XrdSysLogger  *Logger,
                                    const char    *config_fn,
                                    const char    *parms)
{
    std::string opts, key, value;
    std::string::iterator it;
    int x;

    x = 0;
    key = "";
    value = "";

    opts = parms;
    opts += " ";
    for (it=opts.begin(); it!=opts.end(); ++it)
    {
        if (*it == '=') x = 1;
        else if (*it == ' ')
        {
            if (key == "glfnprefix")
                gLFNprefix = value;
            key = "";
            value = "";
            x = 0;
        }
        else
        {
            if (x == 0) key += *it;
            if (x == 1) value += *it;
        }
    }

    return (XrdOssStatInfo_t)XrdOssStatInfo;
}
};

XrdVERSIONINFO(XrdOssStatInfoInit,Stat-DCP-for-RUCIO);
