// to compile g++ -o x.exe x.cc -I/afs/slac/package/xrootd/githead/xrootd/src -lXrdFileCache-4 -lXrdXrootd-4
//
//
using namespace std;

#include <stdio.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysTrace.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdFileCache/XrdFileCacheInfo.hh"

//______________________________________________________________________________

std::string LocalRoot;
XrdOss *oss; 

std::string checkPFCcinfoInit(const char* confg)
{
    XrdOucEnv myEnv;
    XrdSysLogger log;
    XrdSysError err(&log);

    int fd = open(confg, O_RDONLY, 0);
    
    XrdOucStream Config(&err, getenv("XRDINSTANCE"), &myEnv, "=====> ");
    Config.Attach(fd);

    // suppress oss init messages
    int efs = open("/dev/null",O_RDWR, 0);
    XrdSysLogger ossLog(efs);
    XrdSysError ossErr(&ossLog, "print");
    XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(0,&Config,&ossErr);
    bool ossSucc = ofsCfg->Load(XrdOfsConfigPI::theOssLib);
    if (ossSucc)
        ofsCfg->Plugin(oss);
    else
        oss = NULL;

//    int n = Config.FDNum();
    char *var;
    while((var = Config.GetMyFirstWord()))
    {
        if (! strncmp(var,"oss.localroot", strlen("oss.localroot")))
        {
             LocalRoot = Config.GetWord();
             return LocalRoot;
        }
    }
    return NULL;
}

bool checkPFCcinfoIsComplete(std::string cinfofile)
{
   bool rc;
   XrdOucEnv myEnv;

   if (oss == NULL) return false;

   XrdOssDF* fh = oss->newFile("nobody");
   fh->Open(cinfofile.c_str(), O_RDONLY, 0600, myEnv);

   struct stat stBuf;
   if (fh->Fstat(&stBuf)) return false;

   if (fh)
   {
       XrdSysTrace tr(""); tr.What = 2;
       XrdFileCache::Info cfi(&tr);
//       XrdFileCache::Info cfi(NULL);

       if (! cfi.Read(fh))
           rc = false;
       else
           rc = cfi.IsComplete();
       fh->Close();
   }
   else 
       rc = false;
   delete(fh);
   return rc;
}
