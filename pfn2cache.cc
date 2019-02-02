/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2017)
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <openssl/md5.h>

std::string pfn2cache(const std::string localMetaLinkRootDir, const std::string gLFNprefix, const char* pfn) 
{
// rucioDID isn't quite ruico DID (scrope:file), rucioDID = scrope/XX/XX/file

    std::string myPfn, rucioDID, cachePath;
    std::string scope, slashScope, file, tmp;
    std::size_t i;
    MD5_CTX c;
    unsigned char md5digest[MD5_DIGEST_LENGTH];
    char md5string[MD5_DIGEST_LENGTH*2+1];

    myPfn = pfn;
    // if myPfn starts with "localMetaLinkRootDir" and ends with ".meta4" or ".metalink", this function is
    // called after lfn2pfn and we need to remove the starting "localMetaLinkRootDir" and tailing ".meta4"
    if (myPfn.find(localMetaLinkRootDir) == 0 &&
        myPfn.rfind(".meta4") == (myPfn.length() - 6))
    {
        myPfn.replace(0, localMetaLinkRootDir.length(), "");
        myPfn.replace(myPfn.rfind(".meta4"), 6, "");
    }

    if (myPfn.find(localMetaLinkRootDir) == 0 &&
        myPfn.rfind(".metalink") == (myPfn.length() - 9))
    {
        myPfn.replace(0, localMetaLinkRootDir.length(), "");
        myPfn.replace(myPfn.rfind(".metalink"), 9, "");
    }

    i = myPfn.rfind("/rucio/");
    // if pfn doesn't have "/rucio/", then rucioDID = pfn
    //     buff = pfn
    // if pfn does have "/rucio/", rucioDID will point to the string after the last 
    // "rucio", including the leading "/"
    //     buff = /atlas/rucio<rucioDID>
    //
    // It is risky to only depend on "Is "/rucio/" in path" to determine if the path is a RUCIO path.
    // It is better to check the string after "/rucio/" to see whether it is in the form of scope:file 
    // or scope/xx/xx/file (hash "/xx/xx/" could be checked)
    if (i == string::npos)
        cachePath = myPfn;
    else
    {
        rucioDID = myPfn.substr(i + 6, myPfn.length() -i -6);  // with a leading "/"

        // Check if this is a FAX gLFN without the leading "/atlas/rucio". 
        // The gLFN format is /atlas/rucio/scope:file. 
        // In gLFN "scope" can contain "/". In RUCIO the "/" should be converted to "."
        // Also in RUCIO "file" can't have "/", and "scope" and "file" can't have ":".
        //
        // In PFN, "/" is used in scope instead of ".". It is best for the cache to do 
        // this as well for gLFN to improve caching efficiency
        if (rucioDID.rfind("/") < rucioDID.rfind(":") && rucioDID.rfind(":") != string::npos)
        { // likely be called by XrdOssStatInfo
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
        
        cachePath = gLFNprefix + rucioDID;
    }
    return cachePath;
}
