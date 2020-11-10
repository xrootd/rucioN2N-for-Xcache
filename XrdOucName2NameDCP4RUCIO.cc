/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2017)
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <errno.h>
#include <openssl/md5.h>
#include "XrdVersion.hh"
XrdVERSIONINFO(XrdOucgetName2Name, "N2N-Xcache4RUCIO");

#include "rucioGetMetaLink.hh"
#include "pfn2cache.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysError.hh"

class XrdOucName2NameDiskCacheProxy4Rucio : public XrdOucName2Name
{
public:
    virtual int lfn2pfn(const char* lfn, char* buff, int blen);
    virtual int lfn2rfn(const char* lfn, char* buff, int blen);
    virtual int pfn2lfn(const char* lfn, char* buff, int blen);

    XrdOucName2NameDiskCacheProxy4Rucio(XrdSysError *erp, const char* confg, const char* parms);
    virtual ~XrdOucName2NameDiskCacheProxy4Rucio() {};

    friend XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs);
private:
    string myName, localMetaLinkRootDir, gLFNprefix, rucioServer;
    XrdSysError *eDest;
    bool isCmsd;
};

XrdOucName2NameDiskCacheProxy4Rucio::XrdOucName2NameDiskCacheProxy4Rucio(XrdSysError* erp, 
                                                                         const char* confg,
                                                                         const char* parms)
{
    std::string myProg;
    std::string opts, message, key, value;
    std::string::iterator it;
    std::size_t i;
    int x;

    myName = "Xcache4RUCIO";
    eDest = erp;
    localMetaLinkRootDir = "";
    gLFNprefix = "/atlas/rucio";
    rucioServer = "rucio-lb-prod.cern.ch";
    
    isCmsd = false;
    if (getenv("XRDPROG")) 
    {
        myProg = getenv("XRDPROG");
        if (myProg == "cmsd") isCmsd = true;
    } 

    setenv("XRD_METALINKPROCESSING", "1", 0);
    setenv("XRD_LOCALMETALINKFILE", "1", 0);

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
            if (key == "metalinkdir")
                localMetaLinkRootDir = value;
            else if (key == "glfnprefix")
                gLFNprefix = value;
            else if (key == "rucioserver")
                rucioServer = value;
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
    // defaults
    if (localMetaLinkRootDir == "") localMetaLinkRootDir = "/dev/shm" + gLFNprefix;

    message = myName + " Init: glfnprefix = " + gLFNprefix;
    eDest->Say(message.c_str());
    message = myName + " Init: metalinkdir = " + localMetaLinkRootDir;
    eDest->Say(message.c_str());
    message = myName + " Init: RUCIO metalink server = " + rucioServer;
    eDest->Say(message.c_str());

    rucioGetMetaLinkInit(localMetaLinkRootDir, gLFNprefix, rucioServer);
}

int XrdOucName2NameDiskCacheProxy4Rucio::lfn2pfn(const char* lfn, char* buff, int blen)
{
    std::string myLfn, myPfn, rucioDID;
    std::string scope, slashScope, file;
    std::size_t i;
    // MD5_CTX c;
    // unsigned char md5digest[MD5_DIGEST_LENGTH];
    // char md5string[MD5_DIGEST_LENGTH*2+1];

    if (isCmsd) // cmsd shouldn't do lfn2pfn()
    {
        blen = strlen(lfn);
        strncpy(buff, lfn, blen);
        return 0;
    }

    myPfn = "";
    myLfn = lfn;
// see comments in pfn2cache()

    // Do not allow users to specify file:// or root://localfile
    if (myLfn.find("/root:/localfile") == 0 || myLfn.find("/file:/") == 0)
    {
        myPfn = "ENOENT"; 
        return EFAULT;
    }

    if (myLfn.find("/root:/") == 0 || myLfn.find("/roots:/") == 0 ||
        myLfn.find("/http:/") == 0 || myLfn.find("/https:/") == 0) 
        myPfn = makeMetaLink(eDest, myName, lfn);  // Assume the client know the data source...
    else if ((i = myLfn.rfind(gLFNprefix)) != 0)
    {
        // client want so access a local file that is permitted by the xcache configuration

        /* Do not allow this kind of access (security)
        myLfn = "/file:/localhost" + myLfn;
        myPfn = makeMetaLink(eDest, myName, myLfn.c_str());
        */
        myPfn = "EFAULT";
    }
    else // gLFN
    {
        // with a leading "/"
        rucioDID = myLfn.substr(i + gLFNprefix.length(), myLfn.length() -i - gLFNprefix.length()); 
        if (rucioDID.rfind("/") < rucioDID.rfind(":") && rucioDID.rfind(":") != string::npos)
        // check if this is /atlas/rucio/scope:file format
            myPfn = getMetaLink(eDest, myName, rucioDID);
        else
        {
        // otherwise, assume this is /atlas/rucio/scope/xx/xx/file format
            rucioDID = rucioDID.replace(rucioDID.rfind("/") -6, 7, ":");
            myPfn = getMetaLink(eDest, myName, rucioDID);           
        }
    }
    if (myPfn == "EFAULT")
        return EFAULT;
    else if (myPfn == "ENOENT") 
        return ENOENT;

    blen = myPfn.length();
    strncpy(buff, myPfn.c_str(), blen);
    buff[blen] = 0;

    return 0;
}

int XrdOucName2NameDiskCacheProxy4Rucio::lfn2rfn(const char* lfn, char* buff, int blen) { return -EOPNOTSUPP; }
int XrdOucName2NameDiskCacheProxy4Rucio::pfn2lfn(const char* pfn, char* buff, int blen) 
{
    std::string cachePath;

    cachePath = pfn2cache(localMetaLinkRootDir, gLFNprefix, pfn);
    blen = cachePath.length();
    strncpy(buff, cachePath.c_str(), blen);
    buff[blen] = '\0';

    return 0;
}
 
XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs)
{
    static XrdOucName2NameDiskCacheProxy4Rucio *inst = NULL;

    if (inst) return (XrdOucName2Name *)inst;

    inst = new XrdOucName2NameDiskCacheProxy4Rucio(eDest, confg, parms);
    if (!inst) return NULL;

    return (XrdOucName2Name *)inst;
}
