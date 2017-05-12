using namespace std;

#include <string>
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
    std::size_t i;

    myPfn = pfn;
    i = myPfn.rfind("rucio");
// if pfn doesn't have "rucio", then rucioDID = pfn
//     buff = <cacheDir>/pfn
// if pfn does have "rucio", rucioDID will point to the string after the last "rucio", including the leading "/"
//     buff = <cacheDir>/atlas/rucio<rucioDID>
    if (i == string::npos)
    {
        rucioDID = pfn;
        cachePath = cacheDir + rucioDID;
    }
    else
    {
        rucioDID = myPfn.substr(i + 5, myPfn.length() -i -5);
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

    if (inst) {
        return (XrdOucName2Name *)inst;
    }

    inst = new XrdOucName2NameInvRucio(eDest, parms);
    if (!inst) {
        return NULL;
    }

    return (XrdOucName2Name *)inst;
}
