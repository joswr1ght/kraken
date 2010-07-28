#!/usr/bin/python

import sys

if len(sys.argv) >= 3:
    if len(sys.argv[1])>=114 and len(sys.argv[2])>=114:
        r = ""
        for i in range(114):
            a = ord(sys.argv[1][i])
            b = ord(sys.argv[2][i])
            r = r+chr(48^a^b)
        print r
