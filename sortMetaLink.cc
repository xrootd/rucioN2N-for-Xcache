#include <iostream>
#include <string>
#include <vector>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlreader.h>
#include <XrdCl/XrdClURL.hh>

using namespace std;

const xmlChar* xmlNodeGetName(xmlNodePtr& node) {
    return (xmlChar*)(node->name);
}

bool urlNodeContentSwap(xmlNodePtr& nodea, 
                        xmlNodePtr& nodeb,
                        vector<string> preferredSiteDomains) {

    const xmlChar* nodeAcontent = xmlNodeGetContent(nodea);
    const xmlChar* nodeBcontent = xmlNodeGetContent(nodeb);

    XrdCl::URL urla((const char*)nodeAcontent);
    XrdCl::URL urlb((const char*)nodeBcontent);
    string nodeAhost = urla.GetHostName();
    string nodeBhost = urlb.GetHostName();

    const xmlChar* nodeAlocation = xmlGetProp(nodea, (const xmlChar*)"location");
    const xmlChar* nodeBlocation = xmlGetProp(nodeb, (const xmlChar*)"location");

    int numPreferredSites = preferredSiteDomains.size();
    int tmpa = numPreferredSites;
    int tmpb = numPreferredSites;
    for (int i=0; i<numPreferredSites; i++) {
        if (tmpa == numPreferredSites && nodeAhost.find(preferredSiteDomains[i]) != std::string::npos) { tmpa = i;}
        if (tmpb == numPreferredSites && nodeBhost.find(preferredSiteDomains[i]) != std::string::npos) { tmpb = i;}
    }

    if (tmpa > tmpb) {
        //printf("swapping %s and %s\n", nodeAhost.c_str(), nodeBhost.c_str());
        xmlNodeSetContent(nodea, nodeBcontent);
        xmlNodeSetContent(nodeb, nodeAcontent);
        xmlSetProp(nodea, (const xmlChar*)"location", nodeBlocation);
        xmlSetProp(nodeb, (const xmlChar*)"location", nodeAlocation);
        return true;
    }
    return false;
}

const char* sortXMLURL(const char* xml_string, std::string domainList) { 

    vector<string> domainVector;

    size_t prev_pos = 0;
    size_t pos = 0;
    while ((pos = domainList.find(' ')) != std::string::npos) {
        domainList.erase(pos, 1);
    }
    
    pos = 0;
    while ((pos = domainList.find(",", prev_pos)) != std::string::npos) {
        domainVector.push_back(domainList.substr(prev_pos, pos-prev_pos));
        prev_pos = pos+1;
    }
    domainVector.push_back(domainList.substr(prev_pos, 999));

    //for (int i=0; i<domainVector.size(); i++) {
    //    printf("+++ domainVector[%d] = %s\n", i, domainVector[i].c_str());
    //}

    xmlKeepBlanksDefault(0);
    xmlDocPtr doc = xmlReadMemory(xml_string, strlen(xml_string), NULL, NULL, 0);
    if (doc == NULL) {
        cerr << "Failed to parse XML." << endl;
        return NULL;
    }

    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    if (root_node == NULL) {
        cerr << "Failed to get root element." << endl;
        xmlFreeDoc(doc);
        return NULL;
    }

    xmlNodePtr file_node = NULL;
    xmlNodePtr current_node = xmlFirstElementChild(root_node);
    while (current_node != NULL) {
        //xmlElemDump(stdout, doc, current_node);
        if (!strcmp((char*)xmlNodeGetName(current_node), "file")) {
            file_node = current_node;
            break;
        }
        current_node = current_node->next;
    }

    if (file_node == NULL) {
        cerr << "File node not found." << endl;
        xmlFreeDoc(doc);
        return NULL;
    }

    xmlNodePtr url_node0 = NULL;
    xmlNodePtr url_nodea = NULL;
    xmlNodePtr url_nodeb = NULL;
    current_node = xmlFirstElementChild(file_node);
    while (current_node != NULL) {
        if (!strcmp((char*)xmlNodeGetName(current_node), "url")) {
            url_node0 = current_node;
            break;
        }
        current_node = current_node->next;
    }
    
    int needsort = 1;
    while (needsort == 1) {
        needsort = 0;
        url_nodea = url_node0;
        while (url_nodea != NULL) {
            url_nodeb = url_nodea->next;
            while (url_nodeb != NULL && strcmp((char*)xmlNodeGetName(url_nodeb), "url")) {
                url_nodeb = url_nodeb->next;
            }
            if (url_nodeb == NULL) break;
            if (urlNodeContentSwap(url_nodea, url_nodeb, domainVector))
                needsort = 1;
            url_nodea = url_nodeb;
        }
    }

    // reset the priority
    url_nodea = url_node0;
    int url_node_priority = 1;
    while (url_nodea != NULL) {
        if (!strcmp((char*)xmlNodeGetName(url_nodea), "url")) {
            char p[4];
            sprintf(p, "%d", url_node_priority);
            xmlSetProp(url_nodea, (const xmlChar*)"priority", (xmlChar*)p);
            url_node_priority++;
        }
        url_nodea = url_nodea->next;
    }

    // Output the modified XML (for demonstration)
    //xmlElemDump(stdout, doc, xmlDocGetRootElement(doc));
    xmlChar *new_xml_string = NULL;
    int format = 1; // 1 for formatted output
    int docSize;
    xmlDocDumpFormatMemory(doc, &new_xml_string, &docSize, format);

    xmlFreeDoc(doc);
    return (const char*)new_xml_string;
}
