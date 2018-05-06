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
import tempfile
from PIL import Image

ARG_InputFile = ''
ARG_ListFile = ''
ARG_OutputDir = '.'
ARG_Encoder = 'etc2_alpha'
ARG_Quality = 'normal'
ARG_FixImageSizeModulo = 4

C_TexturePackerPath = 'TexturePacker'
C_EtcToolPath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'EtcTool')

gFileHashes = {}    # key: filepath, value: sha1    
gProcessedFileCount = 0

def readListFile():
    global ARG_ListFile

    with open(ARG_ListFile) as f:
        lines = f.readlines()
        f.close()
    return tuple([l.strip() for l in lines])

def readHashFile():
    global ARG_ListFile
    global gFileHashes

    hashFilepath = ARG_ListFile + '.sha1'
    if not os.path.isfile(hashFilepath):
        return

    with open(hashFilepath) as f:
        lines = f.readlines()
        stripLines = [l.strip() for l in lines]
        for l in stripLines:
            key, value = l.split(';', 1)
            gFileHashes[key] = value
        f.close()

def writeHashFile():
    global ARG_ListFile
    global gFileHashes

    with open(ARG_ListFile + '.sha1', 'w') as f:
        for key, value in gFileHashes.items():
            f.write(key + ';' + value + '\n')
        f.close()

def compressLz4(filepath):
    with open(filepath, 'rb') as f:
        srcData = f.read()
        srcDataLen = len(srcData)
        f.close()
    compressed = lz4.block.compress(srcData, mode='high_compression', compression=9, store_size=True)
    os.remove(filepath)
    with open(filepath + '.lz4', 'wb') as f:
        f.write(compressed)
        f.close()
    compressedLen = len(compressed)
    print('\tLZ4 compressed (%dkb -> %dkb), Ratio: %.1f' % (srcDataLen/1024, compressedLen/1024, 
        srcDataLen/compressedLen))

def encodeEtc2(filepath):
    global ARG_OutputDir, ARG_Quality, ARG_Encoder, ARG_ListFile, ARG_FixImageSizeModulo
    global C_EtcToolPath
    global gFileHashes, gProcessedFileCount

    if not os.path.isfile(filepath):
        print("Image file '%s' does not exist" % filepath)
        return False
    filedir = os.path.dirname(filepath)
    destdir = os.path.join(ARG_OutputDir, filedir)
    if not os.path.isdir(destdir):
        os.makedirs(destdir, exist_ok=True)

    # Check source file hash with the data we cached 
    # If file didn't change, return immediately
    if ARG_ListFile:
        sha1 = hashlib.sha1()
        sha1.update(open(filepath, 'rb').read())
        hashVal = sha1.hexdigest()
        if filepath in gFileHashes and gFileHashes[filepath] == hashVal:
            return True

    tpFmt = ''
    if ARG_Encoder == 'etc2':
        tpFmt = 'RGB8'
    elif ARG_Encoder == 'etc2_alpha':
        tpFmt = 'RGBA8'

    tpQuality = ''
    if ARG_Quality == 'low':
        tpQuality = ['-effort', '30']
    elif ARG_Quality == 'normal':
        tpQuality = ['-effort', '60']
    elif ARG_Quality == 'high':
        tpQuality = ['-effort', '100']

    filename, fileext = os.path.splitext(filepath)
    outputFilepath = os.path.join(destdir, os.path.basename(filename)) + '.ktx'

    print(filepath + ' -> ' + os.path.relpath(outputFilepath, ARG_OutputDir))
    modifiedFilepath = filepath
    
    # Open the image file, check the size to be a modulo of the argument
    if (ARG_FixImageSizeModulo != 0):
        img = Image.open(filepath)
        width, height = img.size
        if (width % ARG_FixImageSizeModulo != 0 or height % ARG_FixImageSizeModulo != 0):
            prevWidth = width
            prevHeight = height
            if (width % ARG_FixImageSizeModulo != 0):
                width = width + (ARG_FixImageSizeModulo - (width % ARG_FixImageSizeModulo))
            if (height % ARG_FixImageSizeModulo != 0):
                height = height + (ARG_FixImageSizeModulo - (height % ARG_FixImageSizeModulo))
            print('\tFixing size (%d, %d) -> (%d, %d)' % (prevWidth, prevHeight, width, height))
            tmpImageFilepath = os.path.join(tempfile.gettempdir(), os.path.basename(filename)) + fileext
            newImage = Image.new('RGBA', (width, height))
            newImage.paste(img)
            newImage.save(tmpImageFilepath, fileext[1:])
            modifiedFilepath = tmpImageFilepath

    args = [C_EtcToolPath, modifiedFilepath, '-j', '4']
    if tpFmt:
        args.extend(['-format', tpFmt])
    if tpQuality:
        args.extend(tpQuality)
    args.extend(['-errormetric', 'rec709'])
    #args.extend(['-m', '2'])
    args.extend(['-output', outputFilepath])
    r = subprocess.call(args)
    if r == 0:
        compressLz4(outputFilepath)
        if ARG_ListFile:
            gFileHashes[filepath] = hashVal
        gProcessedFileCount = gProcessedFileCount + 1
    if modifiedFilepath != filepath:
        os.remove(modifiedFilepath)
    return (r == 0)

def encodeWithTexturePacker(filepath):
    global ARG_OutputDir
    global C_TexturePackerPath

    filename, fileext = os.path.splitext(filepath)
    outputFilepath = os.path.join(ARG_OutputDir, os.path.basename(filename)) + '.json'

    args = [C_TexturePackerPath, '--data', outputFilepath, filepath]

    r = subprocess.call(args)
    if r == 0:
        # read json and extract output file
        jdata = json.load(open(outputFilepath))
        imgfile = jdata['meta']['image']
        imgdir = os.path.dirname(outputFilepath)
        imgFilepath = os.path.join(imgdir, imgfile)
        res = encodeEtc2(imgFilepath)
        os.remove(imgFilepath)
        return res
    else:
        return False

def encodeFile(filepath):
    # determine the file type (TexturePacker or plain image)
    filename, fileext = os.path.splitext(filepath)
    if fileext == '.tps':
        return encodeWithTexturePacker(filepath)
    if fileext == '.png' or fileext == '.jpg':
        return encodeEtc2(filepath)
    else:
        return False

def main():
    global ARG_ListFile, ARG_Quality, ARG_Encoder, ARG_OutputDir, ARG_InputFile, ARG_FixImageSizeModulo
    global gProcessedFileCount

    cmdParser = optparse.OptionParser()
    cmdParser.add_option('--file', action='store', type='string', dest='ARG_InputFile',
        help = 'Input image file', default=ARG_InputFile)
    cmdParser.add_option('--listfile', action='store', type='string', dest='ARG_ListFile', 
        help = 'Text file which lists input image files', default=ARG_ListFile)
    cmdParser.add_option('--outdir', action='store', type='string', dest='ARG_OutputDir',
        help = 'Output file(s) directory', default=ARG_OutputDir)
    cmdParser.add_option('--enc', action='store', type='choice', dest='ARG_Encoder', 
        choices=['etc2', 'etc2_alpha'], help = 'Choose encoder', default=ARG_Encoder)
    cmdParser.add_option('--quality', action='store', type='choice', dest='ARG_Quality',
        choices = ['low', 'normal', 'high'], help = '', default=ARG_Quality)
    cmdParser.add_option('--msize', action='store', type='int', dest='ARG_FixImageSizeModulo',
        default=4, help='Fix output image size to be a multiply of specified argument')

    (options, args) = cmdParser.parse_args()
    if options.ARG_InputFile:
        ARG_InputFile = os.path.abspath(options.ARG_InputFile)
    if options.ARG_ListFile:
        ARG_ListFile = os.path.abspath(options.ARG_ListFile)
    ARG_OutputDir = os.path.abspath(options.ARG_OutputDir)
    ARG_Encoder = options.ARG_Encoder
    ARG_Quality = options.ARG_Quality
    ARG_FixImageSizeModulo = options.ARG_FixImageSizeModulo
    if not ARG_InputFile and not ARG_ListFile:
        raise Exception('Must provide either --file or --listfile arguments. See --help')
    if not os.path.isdir(ARG_OutputDir):
        raise Exception(ARG_OutputDir + ' is not a valid directory')

    startTm = timeit.default_timer()

    if ARG_ListFile:
        readHashFile()
        files = readListFile()
        for f in files:
            encodeFile(os.path.normpath(f))
        writeHashFile()
    elif ARG_InputFile:
        encodeFile(ARG_InputFile)

    print('Total %d file(s) processed' % gProcessedFileCount)
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



