import sys
import string
import struct
import os

class MultiFileWriter:
    def __init__(self, path):
        self.path = path
        self.files = [None]*256
        self.buffers = [[]]*256
        self.open = False

    def Open(self, name):
        if self.open:
            self.Close()
        outdir = self.path+"/"+name
        if not os.path.isdir(outdir):
            os.makedirs(outdir)
        for i in range(256):
            fname = outdir +"/"+("%02x"%(i,))+".tbl"
            f = open( fname, "a" )
            if f == None:
                print "Could not open file ", fname
                sys.exit(-1)
            size = f.tell()
            if size%11:
                # Recover from partially written chain
                size = size - (size%11)
                f.truncate(size)
            self.files[i] = f
        for i in range(256):
            self.buffers[i] = []
        self.open = True

    def Close(self):
        if not self.open:
            return
        for i in range(256):
            outdata = string.join(self.buffers[i],"")
            self.files[i].write(outdata)
            self.files[i].close()
            self.files[i]=0
        self.open = False
        
    def Write(self, endpoint, index):
        fi = endpoint >> 56
        dat = struct.pack("<Q", endpoint )[1:-1] + \
            struct.pack("<Q", index )[:-3]
        
        self.buffers[fi].append(dat)
        # print self.buffers[fi]
        if len(self.buffers[fi])>256:
            #Flush write buffer
            outdata = string.join(self.buffers[fi],"")
            self.files[fi].write(outdata)
            self.buffers[fi] = []

    def FindHighIndex(self, name):
        if self.open:
            return -1

        high = 0

        for i in range(256):
            fname = self.path+"/"+name+"/"+("%02x"%(i,))+".tbl"
            f = open( fname, "r" )
            if f == None:
                print "Could not open file ", fname
                sys.exit(-1)
            f.seek(0, os.SEEK_END)
            size = f.tell()
            # Recover from partially written chain
            size = size - (size%11)
            pos = size - 1000*11
            if pos<0:
                pos = 0
            f.seek(pos, os.SEEK_SET )
            for j in range((size-pos)/11):
                dat1 = f.read(11)
                dat = dat1[6:]+"\000\000\000"
                val = struct.unpack("<Q", dat)[0]
                if val > high:
                    high = val
            f.close()

        return high
        
