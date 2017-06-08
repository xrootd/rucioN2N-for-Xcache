#!/usr/bin/env python
# Original Authors:
# - Vincent Garonne, <vgaronne@gmail.com>, 2016
#
# modified by Wei Yang
# minor change to the orignal lxplus.cern.ch:~vgaronne/public/whistle-blower.py

'''
Script to retrieve cache content to synchronize with a catalog.
'''

import argparse
import json
import logging
import logging.handlers
import os
import os.path
import sys
import traceback

sys.path.append("/u/sf/yangw/local/lib/python2.7/site-packages")
import stomp

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(logging.DEBUG)
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)

logging.getLogger("stomp.py").setLevel(logging.CRITICAL)

class MyListener(stomp.ConnectionListener):
    def on_error(self, headers, message):
        pass
    def on_message(self, headers, message):
        pass

def get_dids(urls):
    '''
    Method to get a list of dict {scope, name} from a list of urls.
    rucio://{hostname}/replicas/{scope}/{name}
    '''
    dids = []
    for url in urls:
        print "url :: %s" % url
        components = url.split('/')
        dids.append({'scope': components[-2], 'name': components[-1]})
    return dids


def chunks(l, n):
    '''
    Yield successive n-sized chunks from l.
    '''
    for i in xrange(0, l and len(l) or 0, n):
        yield l[i:i+n]

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument('--cache-dir', action="store", type=str, required=True, help='Cache directory.')
    parser.add_argument('--rse', action="store", required=True, type=str, help='RSE name.')
    parser.add_argument('--broker', action="store", default='atlas-test-mb.cern.ch', type=str, help='Message broker host.')
    parser.add_argument('--port', dest='port', type=int, default=61013, help='Message broker port.')
    parser.add_argument('--topic', action="store", default='/topic/rucio.cache', type=str, help='Message broker topic.')
    parser.add_argument('--timeout', dest="timeout", type=float, default=None, help="Message broker timeout (seconds).")
    parser.add_argument("--chunk-size", action="store", default=10, type=int, help='Message broker chunk size')
    parser.add_argument("--username", action="store", required=True, type=str, help='Message broker username.')
    parser.add_argument("--password", action="store", required=True, type=str, help='Message broker username.')

    args = parser.parse_args()
    dirpath = args.cache_dir + "/dumps"
    RSE = args.rse
    broker, port, topic = args.broker, args.port, args.topic
    username, password = args.username, args.password
    timeout, chunk_size = args.timeout, args.chunk_size
    versioning_file = dirpath + '/.cache_versioning'

    if os.system("python proxy_caceh_dump.py %s" % args.cache_dir) != 0:
        sys.exit(-1)

    logger.info('Scan dump directory: %s', dirpath)
    entries = sorted(dirpath + '/' + fn for fn in os.listdir(dirpath) if not fn.startswith('.'))

    if len(entries) == 1:
        logger.info('Exit. Only one dump available. No diffs to compute.')
        sys.exit(0)

    try:
        logger.info('Connect to the stomp server %s:%s%s', broker, port, topic)
        conn = stomp.Connection(host_and_ports=[(broker, port)],
#                                user=username,
#                                passcode=password,
                                reconnect_attempts_max=10,
                                timeout=timeout, keepalive=True)
        conn.set_listener('', MyListener())
        conn.start()
        conn.connect('', '', wait=True)
        conn.subscribe(destination='/topic/rucio.cache', id=1, ack='auto')

    except stomp.exception.ConnectFailedException, error:
        logger.error('Failed to connect to the messaging server %s %s', error, traceback.format_exc())
        sys.exit(-1)
    except Exception, error:
        logger.error('%s %s', error, traceback.format_exc())
        sys.exit(-1)

    logger.info('Iterate over the dumps')
    predecessor, last_dump = None, entries[-1]
    for entry in entries:
        logger.info("Processing dump: %s", entry)

        with open(entry, "r") as f:
            urls = set(line.rstrip() for line in f if line.startswith('rucio://'))

        plus, minus = None, None
        if predecessor is not None:
            logger.info('Compute the diffs:')
            plus = get_dids(set(urls) - set(predecessor))
            minus = get_dids(set(predecessor) - set(urls))
            logger.info('%s entries to add', len(plus))
            logger.info('%s entries to remove', len(minus))
        else:
            # Check if the cache content has been already published
            if not os.path.isfile(versioning_file):
                plus = get_dids(urls)
                print plus
                logger.info('%s entries to add (boostrap cache publication)', len(plus))

        try:
            for files, operation in ((plus, 'add_replicas'), (minus, 'delete_replicas')):
                for chunk in chunks(files, chunk_size):

                    payload = json.dumps({'operation': operation,
                                          'rse': RSE,
                                          'files': chunk})

                    logger.info('Send report %s to %s:%s%s', payload, broker, port, topic)

                    if not conn.is_connected():
                        logger.info('reconnect to %s:%s%s', broker, port, topic)
                        conn.start()
                        conn.connect(wait=True)
                        logger.info('connected to %s:%s%s',  broker, port, topic)

                    conn.send(body=payload,
                              headers={'persistent': 'true'},
                              destination=topic)

        except stomp.exception.ConnectFailedException, error:
            logger.error('Failed to connect to the messaging server %s %s', str(error), str(traceback.format_exc()))
            sys.exit(-1)
        except Exception, error:
            logger.error('%s %s', str(error), str(traceback.format_exc()))
            sys.exit(-1)
        finally:
            try:
                conn.disconnect()
            except:
                pass

        # Create the versioning file
        if not os.path.isfile(versioning_file):
            open(versioning_file, 'a').close()

        # If succesful delete the file - not the most recent one
        if entry is not last_dump:
            logger.info('delete processed dump: %s', entry)
            os.remove(entry)
        else:
            logger.info('Keep the most recent dump: %s', entry)

        predecessor = urls

    sys.exit(0)
