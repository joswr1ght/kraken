#!/usr/bin/python
import sys
import zlib
import os

#if len(sys.argv)<2:
#    print "usage ", sys.argv[0], " IL file"
#    sys.exit(-1)

def makebrz(dp_bits):
    try:
        os.unlink("a5_slice_dpX_a5slicer.il")
    except OSError:
        pass
    dpdefs=""
    for i in range(dp_bits-11):
        dpdefs = dpdefs + " -D DP_BIT_%i" % (i+12,)
    print "DP_DEFS: ", dpdefs
    os.system("/usr/local/atibrook/sdk/bin/brcc -k -pp %s a5_slice_dpX.br" % (dpdefs,) )
    f = open("a5_slice_dpX_a5slicer.il")
    if f==None:
        print "Could not read ", sys.argv[1]
        sys.exit(-1)

    data = f.read()
    f.close()

    oname = "../my_kernel_dp%i.Z" % (dp_bits,)
    data2 = zlib.compress(data)
    fo = open( oname, "wb" )
    fo.write(data2)
    fo.close()
    #os.system("ld -s -r -o ../%s.o -b binary %s" % (oname[:-2],oname))

makebrz(11)
# makebrz(12)
makebrz(13)
makebrz(14)

