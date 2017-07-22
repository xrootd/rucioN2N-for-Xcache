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

extern "C" {

int XrdOssStatInfo(const char *path, struct stat *buff,
                   int         opts, XrdOucEnv   *envP)
{
     std::string cachePath, prefix, tmp;
     std::size_t i;

     tmp = path;
     i = tmp.rfind("/atlas/rucio");
     if (i != string::npos) 
     {
         prefix = tmp.substr(0, i);
         cachePath = prefix + pfn2cache("", tmp.substr(i+1, tmp.length() -i).c_str());
     }
     else 
     {
         i = tmp.find("/root:/");
         if (i != string::npos)
         {
             prefix = tmp.substr(0, i);
             i = tmp.rfind("/rucio/");
             if (i != string::npos) 
                 cachePath = prefix + pfn2cache("", tmp.substr(i+1, tmp.length() -i).c_str());
             else
                 cachePath = path;
         }
         else
             cachePath = path;
     }
     return (stat(cachePath.c_str(), buff)? -1 : 0);
}

XrdOssStatInfo_t XrdOssStatInfoInit(XrdOss        *native_oss,
                                    XrdSysLogger  *Logger,
                                    const char    *config_fn,
                                    const char    *parms)
{
    return (XrdOssStatInfo_t)XrdOssStatInfo;
}
};

XrdVERSIONINFO(XrdOssStatInfoInit,Stat-DCP-for-RUCIO);
