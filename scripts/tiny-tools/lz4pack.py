import os
import sys
import subprocess
import shutil
import glob
import optparse
import lz4.block
import json
import hashlib
import traceback
import timeit

def compressLz4(filepath):
    with open(filepath, 'rb') as f:
        srcData = f.read()
        srcDataLen = len(srcData)
        f.close()
    compressed = lz4.block.compress(srcData, mode='high_compression', compression=9, store_size=True)
    with open(filepath + '.lz4', 'wb') as f:
        f.write(compressed)
        f.close()
    compressedLen = len(compressed)
    print('LZ4 compressed (%dkb -> %dkb)' % (srcDataLen/1024, compressedLen/1024))

def main():
    cmdParser = optparse.OptionParser()
    cmdParser.add_option('--file', action='store', type='string', dest='ARG_InputFile',
        help = 'Input file')

    (options, args) = cmdParser.parse_args()
    if options.ARG_InputFile:
        options.ARG_InputFile = os.path.abspath(options.ARG_InputFile)

    if not options.ARG_InputFile:
        raise Exception('Must provide --file argument. See --help')
    if not os.path.isfile(options.ARG_InputFile):
        raise Exception(options.ARG_InputFile + ' is not a valid directory')

    startTm = timeit.default_timer()

    compressLz4(options.ARG_InputFile)

    print('Took %.3f secs' % (timeit.default_timer() - startTm))

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('Error:')
        print(e)
        print('CallStack:')       
        traceback.print_exc(file=sys.stdout)
    except:
        raise

