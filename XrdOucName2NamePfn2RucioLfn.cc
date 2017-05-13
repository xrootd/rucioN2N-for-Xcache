using namespace std;

#include <stdio.h>
#include <string>
#include <openssl/md5.h>
#include "XrdVersion.hh"
XrdVERSIONINFO(XrdOucgetName2Name, "Inv-RUCIO-N2N");

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysError.hh"

class XrdOucName2NameInvRucio : public XrdOucName2Name
{
public:
    virtual int lfn2pfn(const char* lfn, char* buff, int blen);
    virtual int lfn2rfn(const char* lfn, char* buff, int blen);
    virtual int pfn2lfn(const char* lfn, char* buff, int blen);

    XrdOucName2NameInvRucio(XrdSysError *erp, const char* parms);
    virtual ~XrdOucName2NameInvRucio() {};

    friend XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs);
private:
    string myName, cacheDir;
    XrdSysError *eDest;
};

XrdOucName2NameInvRucio::XrdOucName2NameInvRucio(XrdSysError* erp, const char* parms)
{
    std::string opts, message, tmp, key, value;
    std::string::iterator it;
    std::size_t i;
    int x;

    myName = "XrdOucN2N-InvRucio";
    eDest = erp;

    x = 0;
    key = "";
    value = "";

    opts = parms;
    opts += " ";
    for (it=opts.begin(); it!=opts.end(); ++it)
    {
        tmp = *it;
        if (tmp == "=") x = 1;
        else if (tmp == " ") 
        { 
            if (key == "cachedir") 
            {
                cacheDir = value;
                message = myName + " Init : cacheDir = " + cacheDir;
                eDest->Say(message.c_str());
            }
            key = "";
            value = "";  
            x = 0;
        }
        else
        {
            if (x == 0) key += tmp;
            if (x == 1) value += tmp;
        }
    }
}

int XrdOucName2NameInvRucio::lfn2pfn(const char* lfn, char* buff, int blen) { return -EOPNOTSUPP; }
int XrdOucName2NameInvRucio::lfn2rfn(const char* lfn, char* buff, int blen) { return -EOPNOTSUPP; }
int XrdOucName2NameInvRucio::pfn2lfn(const char* pfn, char* buff, int blen) 
{
// rucioDID isn't quite ruico DID (scrope:file), rucioDID = scrope/XX/XX/file

    std::string myPfn, rucioDID, cachePath;
    std::string scope, slashScope, file, tmp;
    std::size_t i;
    MD5_CTX c;
    unsigned char md5digest[MD5_DIGEST_LENGTH];
    char md5string[MD5_DIGEST_LENGTH*2+1];

    myPfn = pfn;
    i = myPfn.rfind("rucio");
    // if pfn doesn't have "rucio", then rucioDID = pfn
    //     buff = <cacheDir>/pfn
    // if pfn does have "rucio", rucioDID will point to the string after the last 
    // "rucio", including the leading "/"
    //     buff = <cacheDir>/atlas/rucio<rucioDID>
    if (i == string::npos)
    {
        rucioDID = pfn;
        cachePath = cacheDir + rucioDID;
    }
    else
    {
        rucioDID = myPfn.substr(i + 5, myPfn.length() -i -5);  // with a leading "/"

        // Check if this is a FAX gLFN without the leading "/atlas/rucio". 
        // The gLFN format is /atlas/rucio/scope:file. 
        // In gLFN "scope" can contain "/". In RUCIO the "/" should be converted to "."
        // Also in RUCIO "file" can't have "/", and "scope" and "file" can't have ":".
        //
        // In PFN, "/" is used in scope instead of ".". It is best for the cache to do 
        // this as well for gLFN to improve caching efficiency
        if (rucioDID.rfind("/") < rucioDID.rfind(":") && rucioDID.rfind(":") != string::npos)
        {
           slashScope = rucioDID.substr(1, rucioDID.find(":") -1);
           scope = slashScope;

           while (slashScope.find(".") != string::npos)
               slashScope.replace(slashScope.find("."), 1, "/");
           while (scope.find("/") != string::npos)
               scope.replace(scope.find("/"), 1, ".");

           file = rucioDID.substr(rucioDID.find(":")+1, string::npos);

           MD5_Init(&c);
           rucioDID = scope + ":" + file;
           MD5_Update(&c, rucioDID.c_str(), rucioDID.length());
           MD5_Final(md5digest, &c);
           for(i = 0; i < MD5_DIGEST_LENGTH; ++i)
               sprintf(&md5string[i*2], "%02x", (unsigned int)md5digest[i]);
           md5string[MD5_DIGEST_LENGTH*2+1] = '\0';
           tmp = md5string;
           rucioDID = "/" + slashScope + "/" + tmp.substr(0, 2) + "/" + tmp.substr(2, 2) + "/" + file;
        }
        
        cachePath = cacheDir + "/atlas/rucio" + rucioDID;
    }
    blen = cachePath.length();
    strncpy(buff, cachePath.c_str(), blen);
    buff[blen] = '\0';

    return 0;
}
 
XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs)
{
    static XrdOucName2NameInvRucio *inst = NULL;

    if (inst) return (XrdOucName2Name *)inst;

    inst = new XrdOucName2NameInvRucio(eDest, parms);
    if (!inst) return NULL;

    return (XrdOucName2Name *)inst;
}
