#!/usr/bin/python

from ctypes import *
import time
import sys
import struct
import os
import os.path
import MultiFileWriter

global a5br
global chainIndex
a5br = None
running = True

print "ATI CAL A5/1 chain generator."

def GetNext():
    global chainIndex
    start = c_ulonglong( chainIndex )
    chainIndex = chainIndex + 1
    a5br.ApplyIndexFunc(pointer(start),34)
    return start

while running:
    chainIndex = 4294967296
    # Read our advance (id) parameter
    try:
        f = open("a51id.cgi","r")
        id = f.readline().strip()
        id = id[id.find(":")+2:]
        f.close()
    except IOError:
        print "Table ID file not found."
        sys.exit(-1)

    total = chainIndex
    if a5br == None:
        a5br = cdll.LoadLibrary("./A5Brook.so")
    print "Table ID is:", id

    mfwr = MultiFileWriter.MultiFileWriter( "tables/" )

    if os.path.isdir( "tables/"+id ):
        print "Searching for continuation"
        highpoint = mfwr.FindHighIndex( id )
        print "Continue at ", highpoint, " chains."
        total = highpoint
        chainIndex = highpoint - 1000000
        if chainIndex < 0:
            chainIndex = 0    

    mfwr.Open(id)

    ###
    # 
    # EDIT: last parameter is number of GPUs to use
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
        if os.path.isfile("/tmp/a5.pause"):
            time_used = -4.0
            print "Paused"
        else:
            time_used = time.time()
            while a5br.A5BrookPopResult(pointer(begin),
                                        pointer(result),
                                        pointer(dummy) ):
                if (result.value & 0xff)==0:
                    # Don't write internal merges to file.
                    # these have end points 0xffffffffffffffff
                    bindex = c_ulonglong(begin.value)
                    a5br.ExtractIndex(pointer(bindex), 34)
                    mfwr.Write(result.value, bindex.value)
                else:
                    print "Merge ", result.value

                a5br.A5BrookSubmit(GetNext(),c_int(0));
                count = count + 1
                total = total + 1
            time_used = time.time() - time_used
            if count > 0:
                print "%016x -> %016x" % (begin.value,  result.value)
                inst_speed = float(count)/(time_used+wait)
                speed = 0.99 * speed + 0.01 * inst_speed;
                print count, "chains ", speed, "/ s found:", total
            if total>8662000000:
                print "Table completed. - deleting id file"
                idx = int(id)+8
                os.unlink("a51id.cgi")
                # EDIT: max table id for your run
                if idx < 148:
                    fi = open("a51id.cgi","w")
                    fi.write("Your id is: %i\n"%(idx,))
                    fi.close()
                #running = False
                break

    print "Shutting down stream engine and closing table files."
    a5br.A5BrookShutdown()
    mfwr.Close()


