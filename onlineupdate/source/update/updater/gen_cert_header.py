#!/usr/bin/env python3

import sys
import binascii
import configparser

def file_byte_generator(filename):
    with open(filename, "rb") as f:
        block = f.read()
        return block

def create_header(array_name, in_filename):
    hexified = ["0x" + binascii.hexlify(bytes([inp])).decode('ascii') for inp in file_byte_generator(in_filename)]
    print("const uint8_t " + array_name + "[] = {")
    print(", ".join(hexified))
    print("};")
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('ERROR: usage: gen_cert_header.py array_name update_config_file')
        sys.exit(1);
    config = configparser.ConfigParser()
    config.read(sys.argv[2])
    sys.exit(create_header(sys.argv[1], config['Updater']['certificate-der']))
