/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2017)
 */

using namespace std;

#include <fcntl.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/md5.h>
#include <string>
#include <thread>
#include "checkPFCcinfo.hh"
#include "pfn2cache.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysError.hh"

/* 
 * From Vincent / Mario:
 * use the following URL to retrieve a metalink file from RUCIO
 * https://rucio-lb-prod.cern.ch/redirect/scope/file/metalink?schemes=root&select=geoip
 * for detail, see Mario's presentation at
 * https://indico.cern.ch/event/591724/contributions/2388081/attachments/1385793/2108722/Rucio_Metalink_Status.pdf
 */

std::string rucioServerUrl = "https://rucio-lb-prod.cern.ch/redirect/";
std::string rucioServerCgi = "/metalink?schemes=root&sort=geoip";

#define MetaLinkLifeTmin 1440 
#define MetaLinkLifeTsec MetaLinkLifeTmin*60

struct rucioMetaLink
{
    char *data;
    size_t size;
};

std::string localMetaLinkRootDir;
std::string ossLocalRoot;

void cleaner()
{
    std::string cmd;
    cmd = "find " + localMetaLinkRootDir + " -type f -amin +" 
        + std::to_string((long long int)MetaLinkLifeTmin) + " -exec rm {} \\; > /dev/null 2>&1; "
        + "find " + localMetaLinkRootDir + " -type d -empty -mmin +" 
        + std::to_string((long long int)MetaLinkLifeTmin) + " -exec rmdir {} \\; > /dev/null 2>&1";
    while (! sleep(600))
    {
        system(cmd.c_str());
    }
}

static int Xcache4RUCIO_DBG = 0;

void rucioGetMetaLinkInit(const std::string dir, const std::string osslocalroot) 
{
     localMetaLinkRootDir = dir;
     ossLocalRoot = osslocalroot;

     std::thread cleanning(cleaner);
     cleanning.detach();
     curl_global_init(CURL_GLOBAL_ALL);

     if (getenv("Xcache4RUCIO_DBG") != NULL) Xcache4RUCIO_DBG = atoi(getenv("Xcache4RUCIO_DBG"));
}

static size_t rucioGetMetaLinkCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct rucioMetaLink *mem = (struct rucioMetaLink *)userp;
 
    mem->data = (char*)realloc(mem->data, mem->size + realsize + 1);
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

int mkdir_p(const std::string dir)
{
    std::string dirs, tmp = "/";
    std::string::iterator it;
    struct stat statBuf;
    int rc;

    dirs = dir;
    for (it=dirs.begin(); it!=dirs.end(); ++it)
    {
        if ( *it != '/')
            tmp += *it;
        else
        {
            if (! (stat(tmp.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode)))
            {
                rc = mkdir(tmp.c_str(), 0755);
                if (rc && errno != EEXIST) return rc;
            }
            tmp += "/";
        }
    }
    rc = mkdir(tmp.c_str(), 0755);
    if (rc && errno != EEXIST) return rc;
    errno = 0;
    return 0;
}

// Both makeMetaLink() and getMetaLink() should return metaLinkFile even if we can't create or download 
// a metalink. A file-not-found error will eventually be gerenated.

// Use .meta4 for files that have not been fully cached. 
// Use .metalink for files that have been completely cached. In this case, a .metalink contains only one
// data source at file://localhost//... so that we don't need to Open() with a remote data source.
std::string makeMetaLink(XrdSysError* eDest, const std::string myName, const std::string pfn)
{
    std::string metaLinkDir, metaLinkFile, myPfn, tmp;
    size_t i;

    // "repair" pfn, from "/root:/atlrdr1:11094/xrootd" to "root://atlrdr1:11094//xrootd"
    myPfn = pfn;
    myPfn.replace(0, 7, "");
    myPfn.replace(myPfn.find("/"), 1, "//");
    myPfn = "root://" + myPfn;

    XrdCl::URL rootURL = myPfn;
    if (!rootURL.IsValid()) return "EFAULT"; 

    metaLinkFile = myPfn;
    if (metaLinkFile.find("root://") == 0)
    {
        metaLinkFile = metaLinkFile.replace(0, 7, "");   // remote "root://"
        metaLinkFile = metaLinkFile.replace(0, metaLinkFile.find("/")+2, "");  // remote loginid@hostnaem:port//
    }

    std::string cinfofile = "/" + pfn2cache("", metaLinkFile.c_str()) + ".cinfo";
    metaLinkFile = localMetaLinkRootDir + "/" + metaLinkFile + ".metalink";

    metaLinkDir = metaLinkFile;
    i = metaLinkDir.rfind("/");
    metaLinkDir.replace(i, metaLinkDir.length() - i+1, "");
    if (mkdir_p(metaLinkDir)) 
    {
        eDest->Say((myName + ": Fail to create metalink dir " + metaLinkDir).c_str());
        return metaLinkFile;
    }

    struct stat statBuf;
    if (! stat(metaLinkFile.c_str(), &statBuf))
        return metaLinkFile;

    if (checkPFCcinfoIsComplete(cinfofile))
    {
        FILE *fd = fopen(metaLinkFile.c_str(), "w");
        if (fd != NULL)
        {
            tmp  = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            tmp += "<metalink xmlns=\"urn:ietf:params:xml:ns:metalink\">\n";
            tmp += "  <file name=\"x\">\n";
            tmp += "    <url location=\"LOCAL\" priority=\"1\">file://localhost/" + ossLocalRoot
                 + cinfofile.substr(0, cinfofile.rfind(".cinfo"))+ "</url>\n";
            tmp += "  </file>\n";
            tmp += "</metalink>\n";

            fprintf(fd, "%s", tmp.c_str());
            fclose(fd);
        }
        return metaLinkFile;
    }
    else
        metaLinkFile = metaLinkFile.replace(metaLinkFile.rfind(".metalink"), 9, ".meta4");

    FILE *fd = fopen(metaLinkFile.c_str(), "w");
    if (fd != NULL) 
    {
        tmp  = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        tmp += "<metalink xmlns=\"urn:ietf:params:xml:ns:metalink\">\n";
        tmp += "  <file name=\"x\">\n";
        tmp += "    <url location=\"REMOTE\" priority=\"1\">" + myPfn + "</url>\n";
        tmp += "  </file>\n";
        tmp += "</metalink>\n";

        fprintf(fd, "%s", tmp.c_str());
        fclose(fd);
    }
    return metaLinkFile; 
}

std::string getMetaLink(XrdSysError* eDest, const std::string myName, const std::string DID)
{
    std::string rucioDID, scope, slashScope, file, metaLinkDir, metaLinkFile, rucioMetaLinkURL;
    std::string tmp;
    std::string::iterator it;
    size_t i;
    int rc;
    MD5_CTX c;
    unsigned char md5digest[MD5_DIGEST_LENGTH];
    char md5string[MD5_DIGEST_LENGTH*2+1];
    struct stat statBuf;

    if (localMetaLinkRootDir == "") return "";

    rucioDID = DID;
    slashScope = rucioDID.substr(1, rucioDID.find(":") -1);
    scope = slashScope;

    while (slashScope.find(".") != std::string::npos)
        slashScope.replace(slashScope.find("."), 1, "/");
    while (scope.find("/") != std::string::npos)
        scope.replace(scope.find("/"), 1, ".");

    file = rucioDID.substr(rucioDID.find(":")+1, std::string::npos);

    // check if a valid metalink file already exist. "Valid" means mtime < 7d. 
    MD5_Init(&c);
    rucioDID = scope + ":" + file;
    MD5_Update(&c, rucioDID.c_str(), rucioDID.length());
    MD5_Final(md5digest, &c);
//    for(i = 0; i < MD5_DIGEST_LENGTH; ++i)
    for(i = 0; i < 2; ++i)
        sprintf(&md5string[i*2], "%02x", (unsigned int)md5digest[i]);
//    md5string[MD5_DIGEST_LENGTH*2+1] = '\0';
    md5string[2*2+1] = '\0';
    tmp = md5string;

    std::string cinfofile;

    cinfofile = "/atlas/rucio/" + slashScope + "/"
                + tmp.substr(0, 2) + "/" + tmp.substr(2, 2) + "/"
                + file + ".cinfo";
    
    metaLinkDir = localMetaLinkRootDir + "/atlas/rucio/" + slashScope + "/" 
                + tmp.substr(0, 2) + "/" + tmp.substr(2, 2);
    if (mkdir_p(metaLinkDir)) 
    {
        eDest->Say((myName + ": Fail to create metalink dir " + metaLinkDir).c_str());
        return metaLinkFile;
    }
    metaLinkFile = metaLinkDir + "/" + file + ".metalink";
 
    if (! stat(metaLinkFile.c_str(), &statBuf))
        return metaLinkFile;

    if (checkPFCcinfoIsComplete(cinfofile))
    {
        FILE *fd = fopen(metaLinkFile.c_str(), "w");
        if (fd != NULL) 
        {
            tmp  = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            tmp += "<metalink xmlns=\"urn:ietf:params:xml:ns:metalink\">\n";
            tmp += "  <file name=\"x\">\n";
            tmp += "    <url location=\"LOCAL\" priority=\"1\">file://localhost/" + ossLocalRoot 
                 + cinfofile.substr(0, cinfofile.rfind(".cinfo"))+ "</url>\n";
            tmp += "  </file>\n";
            tmp += "</metalink>\n";

            fprintf(fd, "%s", tmp.c_str());
            fclose(fd);
        }
        return metaLinkFile;
    }
    else
        metaLinkFile = metaLinkFile.replace(metaLinkFile.rfind(".metalink"), 9, ".meta4");
//        metaLinkFile = metaLinkDir + "/" + file + ".meta4";

    time_t t_now = time(NULL);
    if (stat(metaLinkFile.c_str(), &statBuf) == 0 && 
        (t_now - statBuf.st_mtim.tv_sec) < MetaLinkLifeTsec) 
    {
        return metaLinkFile;
    }
     
    rucioMetaLinkURL = rucioServerUrl + scope + "/" + file + rucioServerCgi;

    // -f prevent an output to be created if DID doesn't exist
//    tmp = "curl -s -k -f -o " + metaLinkFile + " '" + rucioMetaLinkURL + "'" + " 2>/dev/null";
//    system(tmp.c_str());
//    return metaLinkFile;

    struct rucioMetaLink chunk;
    CURL *curl_handle;
    CURLcode res;

    for (int ii = 0; ii < 3; ii++)
    {
        chunk.data = (char*)malloc(1);  // will be grown as needed by the realloc above 
        chunk.size = 0;    // no data at this point  
       
        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1); // to make it thread-safe?
        curl_easy_setopt(curl_handle, CURLOPT_URL, rucioMetaLinkURL.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);  // the curl -k option
        curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1);  // http header
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, rucioGetMetaLinkCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 180L);
       
        // some servers don't like requests that are made without a user-agent
        // field, so we provide one 
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        res = curl_easy_perform(curl_handle);
        curl_easy_cleanup(curl_handle);
       
        // check for errors
        if(res == CURLE_OK)
        {
            if (! strncmp(chunk.data, "HTTP/1.1 200 OK", 15) &&
                (! strncmp(chunk.data + chunk.size - 11, "</metalink>", 11) || // simple sanity check
                 ! strncmp(chunk.data + chunk.size - 12, "</metalink>", 11)))  // maybe a "\n" at the end?
            {
                FILE *fd = fopen(metaLinkFile.c_str(), "w");
                if (fd != NULL) 
                {
                    fprintf(fd, "%s", strstr(chunk.data, "<?xml"));
                    fclose(fd);
                    if (Xcache4RUCIO_DBG > 0) 
                        eDest->Say((myName + ": Successfully download metalink for " 
                                    + DID.substr(1, string::npos)).c_str());
                }
                else
                    eDest->Say((myName + ": Fail to write metalink for " 
                                       + DID.substr(1, string::npos)).c_str());
                free(chunk.data);
                break;
            }
            else if (! strncmp(chunk.data, "HTTP/1.1 404 Not Found", 22)) 
            {
                eDest->Say((myName + ": Err RUCIO does not know " + DID.substr(1, string::npos)).c_str());
                break;
            }
            else if (strncmp(chunk.data, "HTTP/1.1 200 OK", 15))
                eDest->Say((myName + ": Err fetching metalink for " + DID.substr(1, string::npos) 
                            + " . Try #" + std::to_string((long long int)ii) 
                            + ": " + std::string(chunk.data, 30)).c_str());
            else 
                eDest->Say((myName + ": Err fetching metalink for " + DID.substr(1, string::npos)
                            + " . Try #" + std::to_string((long long int)ii) 
                            + ": " + std::string(chunk.data + chunk.size - 30)).c_str());
        }
        else 
            eDest->Say((myName + ": Err downloading metalink for " + DID.substr(1, string::npos)
                        + " . Try #" + std::to_string((long long int)ii)).c_str());
        free(chunk.data);
        sleep(5);
    }
// always return a metaLinkFile path. If the actual file does not exist (because RUCIO 
// does not know the file), then this is the same as returning an ENOENT
    return metaLinkFile;  
}
