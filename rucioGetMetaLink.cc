using namespace std;

#include <fcntl.h>
#include <curl/curl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/md5.h>

/* 
 * From Vincent / Mario:
 * use the following URL to retrieve a metalink file from RUCIO
 * https://rucio-lb-prod.cern.ch/redirect/scope/file/metalink?schemes=root&select=geoip
 * for detail, see Mario's presentation at
 * https://indico.cern.ch/event/591724/contributions/2388081/attachments/1385793/2108722/Rucio_Metalink_Status.pdf
 */

std::string rucioServerUrl = "https://rucio-lb-prod.cern.ch/redirect/";
std::string rucioServerCgi = "/metalink?schemes=root&select=geoip";

#define MetaLinkLifetime 3600*24

struct rucioMetaLink
{
    char *data;
    size_t size;
};

std::string localMetaLinkRootDir;

void rucioGetMetaLinkInit(const std::string dir) 
{
     localMetaLinkRootDir = dir;
//     curl_global_init(CURL_GLOBAL_ALL);
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

std::string makeMetaLink(const std::string pfn)
{
    std::string metaLinkDir, metaLinkFile, myPfn, tmp;
    size_t i;

    // "repair" pfn, from "/root:/atlrdr1:11094/xrootd" to "root://atlrdr1:11094//xrootd"
    myPfn = pfn;
    myPfn.replace(0, 7, "");
    myPfn.replace(myPfn.find("/"), 1, "//");
    myPfn = "root://" + myPfn;

    metaLinkFile = myPfn;
    if (metaLinkFile.find("root://") == 0)
    {
        metaLinkFile = metaLinkFile.replace(0, 7, "");   // remote "root://"
        metaLinkFile = metaLinkFile.replace(0, metaLinkFile.find("/")+2, "");  // remote loginid@hostnaem:port//
    }
    metaLinkFile = localMetaLinkRootDir + "/" + metaLinkFile + ".meta4";
    metaLinkDir = metaLinkFile;
    i = metaLinkDir.rfind("/");
    metaLinkDir.replace(i, metaLinkDir.length() - i+1, "");
    if (mkdir_p(metaLinkDir)) return "";

    FILE *fd = fopen(metaLinkFile.c_str(), "w");
    if (fd == NULL) return "";
    tmp  = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    tmp += "<metalink xmlns=\"urn:ietf:params:xml:ns:metalink\">\n";
    tmp += "  <file name=\"x\">\n";
    tmp += "    <url location=\"LOCAL\" priority=\"1\">" + myPfn + "</url>\n";
    tmp += "  </file>\n";
    tmp += "</metalink>\n";

    fprintf(fd, "%s", tmp.c_str());
    fclose(fd);

    return metaLinkFile; 
}

// return "" if we can't find a metalink for it.
std::string getMetaLink(const std::string DID)
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
    for(i = 0; i < MD5_DIGEST_LENGTH; ++i)
        sprintf(&md5string[i*2], "%02x", (unsigned int)md5digest[i]);
    md5string[MD5_DIGEST_LENGTH*2+1] = '\0';
    tmp = md5string;
    
    metaLinkDir = localMetaLinkRootDir + "/atlas/rucio/" + slashScope + "/" + tmp.substr(0, 2) + "/" + tmp.substr(2, 2);

    if (mkdir_p(metaLinkDir)) return "";

    metaLinkFile = metaLinkDir + "/" + file + ".meta4";
    time_t t_now = time(NULL);
    if (stat(metaLinkFile.c_str(), &statBuf) == 0 && 
       (t_now - statBuf.st_mtim.tv_sec) < MetaLinkLifetime) 
    {
        return metaLinkFile;
    }
     
    rucioMetaLinkURL = rucioServerUrl + scope + "/" + file + rucioServerCgi;

    tmp = "wget -q --no-check-certificate -O " + metaLinkFile + " '" + rucioMetaLinkURL + "'";
    if (system(tmp.c_str()) == 0)
        return metaLinkFile;
    else
        return "" ;

/*  libcurl is not thread safe in RHEL6 !
 *
    struct rucioMetaLink chunk;

    chunk.data = (char*)malloc(1);  // will be grown as needed by the realloc above 
    chunk.size = 0;    // no data at this point  
   
    CURL *curl_handle;
    CURLcode res;
    // curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, rucioMetaLinkURL.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);  // the curl -k option
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, rucioGetMetaLinkCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 60L);
   
    // some servers don't like requests that are made without a user-agent
    // field, so we provide one 
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    res = curl_easy_perform(curl_handle);
   
    // check for errors
    if(res != CURLE_OK) return "";
    curl_easy_cleanup(curl_handle);
    // curl_global_cleanup();

    FILE *fd = fopen(metaLinkFile.c_str(), "w");
    if (fd == NULL) return "";
    fprintf(fd, "%s", chunk.data);
    fclose(fd);
    free(chunk.data);

    return metaLinkFile;
*/
}
