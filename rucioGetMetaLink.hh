/*
 * Author: Wei Yang (SLAC National Accelerator Laboratory / Stanford University, 2017)
 */

#include "XrdSys/XrdSysError.hh"

void rucioGetMetaLinkInit(const std::string, const std::string, const std::string, const std::string);
string getMetaLink(XrdSysError*, const std::string, const std::string);
string makeMetaLink(XrdSysError*, const std::string, const std::string);
