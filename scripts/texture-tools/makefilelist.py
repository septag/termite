import os
import sys
import glob
import fnmatch
import optparse
import traceback

def main():
    cmdParser = optparse.OptionParser()
    cmdParser.add_option('--ext', action='store', type='string', dest='ARG_Ext',
        help='File extension(s) to gather, sperated by comma', default='*')
    cmdParser.add_option('--dir', action='store', type='string', dest='ARG_Dir',
        help='Root directory to search for files', default='.')
    cmdParser.add_option('--prefix', action='store', type='string', dest='ARG_Prefix',
        help='Prefix directory that will be appended at the begining of each filepath', default='')
    cmdParser.add_option('--name', action='store', type='string', dest='ARG_NamePattern',
        help='Name pattern, can include wild cards', default='*')
    cmdParser.add_option('--out', action='store', type='string', dest='ARG_OutputFile',
        help='Output file list path', default='')
    (options, args) = cmdParser.parse_args()

    if options.ARG_Ext:
        extensions = options.ARG_Ext.split(',')
    if options.ARG_Dir:
        destDir = os.path.abspath(options.ARG_Dir)
    if not os.path.isdir(destDir):
        raise Exception('--dir argument is not a vaid directory')
    if not options.ARG_OutputFile:
        raise Exception('--out argument is required. see --help')
    destFilepath = os.path.abspath(options.ARG_OutputFile)

    files = []
    numFiles = 0
    search_phrase = options.ARG_NamePattern + '.'
    for root, dirnames, filenames in os.walk(destDir):
        for ext in extensions:
            for filename in fnmatch.filter(filenames, search_phrase + ext):
                files.append(os.path.join(options.ARG_Prefix, os.path.relpath(os.path.join(root, filename), destDir)))
                numFiles = numFiles + 1
    print('Writing to: ' + destFilepath)
    with open(destFilepath, 'w') as lf:
        for f in files:
            lf.write(f + '\n')
        lf.close()

    print('Total %d files found' % numFiles)

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