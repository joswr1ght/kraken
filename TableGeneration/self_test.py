#!/usr/bin/python

from ctypes import *
import time
import sys
import struct
import os
import os.path

global a5br
global chainIndex
a5br = None
chainIndex = 0

print "Brook ATI self test."

def GetNext():
    global chainIndex
    start = c_ulonglong( chainIndex )
    chainIndex = chainIndex + 1
    if (chainIndex & 0x3f)==0:
        start = c_ulonglong( 0xa8aa000b635a6fa5 )
    else:
        a5br.ApplyIndexFunc(pointer(start),34)
    return start

while True:
    id = 12123
    total = 0
    a5br = cdll.LoadLibrary("./A5Brook.so")
    print "Table ID is:", id

    ###
    # 
    # Edit: last parameter is number of GPUs to use
    #
    ###
    if not a5br.A5BrookInit(c_int(int(id)), c_int(8), c_int(12), 2):
        print "Could not initialize Streams engine. Quitting."
        sys.exit(-1)

    print "Waiting for engine to warm up and get correct pipeline size."
    pipesize = c_int(0)
    while pipesize.value <= 0:
       time.sleep(1)
       a5br.A5BrookPipelineInfo(pointer(pipesize))
    initfill = 200000
    if pipesize.value > 0:
        initfill = pipesize.value + pipesize.value / 4

    print "Filling pipeline with",initfill,"values."
    for i in range(initfill):
        a5br.A5BrookSubmit( GetNext(),c_int(0));

    begin = c_ulonglong(0);
    result = c_ulonglong(0);
    dummy = c_int()

    print "Entering the grinder"
    speed = 0.0
    time_used = 0.0
    wait = 1.0
    while True:
        wait = 0.9*wait + 0.1*(1.0 - time_used)
        if wait>0.0:
            time.sleep(wait)
        else:
            wait = 0.0
        count = 0

        time_used = time.time()
        while a5br.A5BrookPopResult(pointer(begin),
                                    pointer(result),
                                    pointer(dummy) ):
            if (begin.value==0xa8aa000b635a6fa5) \
                    and (result.value!=0x176adcc0086fa000):
                print "SELF TEST FAILED"
                print "%016x -> %016x" % (begin.value,  result.value)
                sys.exit(-1)
            a5br.A5BrookSubmit(GetNext(),c_int(0));
            count = count + 1
            total = total + 1
        time_used = time.time() - time_used
        if count > 0:
            print "%016x -> %016x" % (begin.value,  result.value)
            inst_speed = float(count)/(time_used+wait)
            speed = 0.99 * speed + 0.01 * inst_speed;
            print count, "chains ", speed, "/ s found:", total


    print "Shutting down stream engine and closing table files."
    a5br.A5BrookShutdown()

