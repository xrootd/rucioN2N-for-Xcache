#!/usr/bin/env python
# Wei Yang
# Dump a list of file in order to update with RUCIO

import sys,os
import time
import fnmatch

cachedir = sys.argv[1]
dumpdir = cachedir + "/dumps"
if not os.path.exists(dumpdir):
    os.mkdir(dumpdir)

if not os.path.isdir(dumpdir):
    print "%s exists but is not a directory" % dumpfile
    sys.exit(-1)

def downloaded_percentage(file):
    '''
    get downloaded percentage from .cinfo
    '''
    fcinfo = os.popen("/afs/slac/package/xrootd/githead/amd64_rhel60/src/xrdpfc_print %s.cinfo" % file)
    for line in fcinfo:
        items = line.split(" ")
        if items[0] == 'fileSize':
            fcinfo.close()
            return int(items[7])*100/int(items[5])
    fcinfo.close()
    return 0

myhost = os.uname()[1]
dumpfile = time.strftime("dump_%Y%m%d-%H%M%S", time.localtime())
dumpfile = dumpdir + '/' + dumpfile
try: 
    f = open(dumpfile, 'w')
except:
    print "can not create dump file %s" % dumpfile
    sys.exit(-1)

for path, dirs, files in os.walk(cachedir):
    for file in fnmatch.filter(files, '*'):
        if fnmatch.fnmatch(file, '*.cinfo'):
            continue

        filepath = os.path.join(path, file)
        print "file: %s " % filepath
        rc = filepath.rfind('/rucio/')
        if rc > -1:
            if downloaded_percentage(filepath) < 50:
                continue
            junk = filepath[rc+7:]
            fncomponents = junk.split('/')
            name = fncomponents[-1]
            scope = fncomponents[0]
            for i in range(1, len(fncomponents) - 3):
                scope += '.' + fncomponents[i]
            did = scope + "/" + name
            f.write("rucio://%s/replicas/%s\n" % (myhost, did))
f.close()

