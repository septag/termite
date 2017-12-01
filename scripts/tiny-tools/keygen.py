import os
import sys
import traceback

def main():
    key = os.urandom(16).hex()
    iv = os.urandom(16).hex()

    skey = ''
    for i in range(0, len(key), 2):
        skey = skey + '0x' + str(key[i:i+2]) + (', ' if i < len(key)-2 else '')

    siv = ''
    for i in range(0, len(iv), 2):
        siv = siv + '0x' + str(iv[i:i+2]) + (', ' if i < len(iv)-2 else '')        

    print('Key: ' + key)
    print('Iv: ' + iv)

    print('')

    print('Key (CPP): ' + skey)
    print('Iv (CPP): ' + siv)

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