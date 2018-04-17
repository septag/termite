import os
import sys
import subprocess
import shutil
import glob
import optparse
import traceback
import timeit
import json
import tempfile

C_EncryptTool = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'encrypt')

def readListFile(listfilepath):
    with open(listfilepath) as f:
        lines = f.readlines()
        f.close()
    return tuple([l.strip() for l in lines])

def main():
    global C_EncryptTool

    cmdParser = optparse.OptionParser();
    cmdParser.add_option('--listfile', action='store', type='string', dest='ARG_ListFile', 
        help = 'Text file which lists input source files', default='')
    cmdParser.add_option('--outdir', action='store', type='string', dest='ARG_OutputDir',
        help = 'Output file(s) directory', default='.')
    cmdParser.add_option('--key', action='store', type='string', dest='ARG_Key',
        help = 'AES-128 key', default='')
    cmdParser.add_option('--iv', action='store', type='string', dest='ARG_Iv',
        help = 'AES-128 Iv', default='')
    cmdParser.add_option('--packjson', action='store_true', dest='ARG_JSON', help='Pack and minify json files')
    (options, args) = cmdParser.parse_args()

    if not options.ARG_ListFile:
        raise Exception('Must provide either --listfile arguments. See --help')
    if not os.path.isdir(options.ARG_OutputDir):
        raise Exception(options.ARG_OutputDir + ' is not a valid directory')
    if not options.ARG_Iv or not options.ARG_Key:
        raise Exception('Must provide Key and Iv parameter')

    files = readListFile(options.ARG_ListFile)
    numErrors = 0
    for f in files:
        if options.ARG_JSON:
            tmpfilepath = os.path.join(tempfile.gettempdir(), os.path.basename(f))
            jdata = json.load(open(f, 'r'))
            open(tmpfilepath, 'w').write(json.dumps(jdata))
            f = tmpfilepath

        outputFilepath = os.path.join(options.ARG_OutputDir, os.path.basename(f)) + '.tenc'
        args = [C_EncryptTool, '-f', f, '-o', outputFilepath, '-k', options.ARG_Key, '-i', options.ARG_Iv]
        r = subprocess.call(args)
        if r != 0:
            numErrors = numErrors + 1

    if numErrors > 0:
        print("%d Errors found" % numErrors)


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
