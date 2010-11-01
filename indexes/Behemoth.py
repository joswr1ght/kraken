#!/usr/bin/python
import os
import sys
import re

def ReadConfig(path):
    config = {}
    f = open(path,"r")
    lines = f.readlines()
    f.close()
    num = 0
    for l in lines:
        l = l.strip()
        v = l.split(" ")
        if v[0]=="Device:" and len(v)>2:
            config[num] = (v[1],int(v[2]),{})
            num = num + 1
        elif v[0]=="Table:" and len(v)>3:
            dev = int(v[1])
            if config.has_key(dev):
                name,max,tables = config[dev]
                tables[int(v[2])] = int(v[3])
                config[dev]=(name,max,tables)
            else:
                print "No device configured: ", dev
    return config        

def WriteConfig(config, path):
    f = open(path,"w")
    f.write("#Devices:  dev/node max_tables\n")
    k = config.keys()
    k.sort()
    for i in k:
        name,max,tables = config[i]
        f.write("Device: %s %i\n" %(name,max))
    f.write("\n#Tables: dev id(advance) offset\n")
    for i in k:
        name,max,tables = config[i]
        for id in tables.keys():
            offset = tables[id]
            f.write("Table: %i %i %i\n"%(i,id,offset))
    f.write("\n")
    f.close()


def FindNextFree(config):
    k = config.keys()
    k.sort()
    for i in k:
        name,max,tables = config[i]
        if len(tables.keys())<max:
            maxid = -1
            maxoffset = -1
            for id in tables.keys():
                offset = tables[id]
                if offset>maxoffset:
                    maxoffset = offset
                    maxid = id
            if maxoffset>=0:
                s = os.lstat("%i.idx"%(maxid,))
                size = (s.st_size/8)-1
                return (i,name,maxoffset+size)
            else:
                return (i,name,0L)
    return None
            
def AddSsdTable(path,id,config):
    k = config.keys()
    k.sort()
    for i in k:
        name,max,tables = config[i]
        ids = tables.keys()
        for j in ids:
            if j==id:
                print "Not adding ", id
                return config
    print "Adding table: ", path,id
    free = FindNextFree(config)
    if free==None:
        print "No free space"
        return config
    dev,name,offset = free
    ids = str(id)
    os.system("./TableConvert si %s/blockdevicedata.bin %s/index.dat %s %s" % (path,path,name+":"+str(offset),ids+".idx"))
    name,max,tables = config[dev]
    tables[id] = offset
    config[dev] = name,max,tables
    return config

def AddDeltaTable(path,id,config):
    k = config.keys()
    k.sort()
    for i in k:
        name,max,tables = config[i]
        ids = tables.keys()
        for j in ids:
            if j==id:
                print "Not adding ", id
                return config
    print "Adding table: ", path,id
    free = FindNextFree(config)
    if free==None:
        print "No free space"
        return config
    dev,name,offset = free
    ids = str(id)
    #os.system("./TableConvert di %s %s %s" % (path,name+":"+str(offset),ids+".idx"))
    print("./TableConvert di %s %s %s" % (path,name+":"+str(offset),ids+".idx"))
    name,max,tables = config[dev]
    tables[id] = offset
    config[dev] = name,max,tables
    return config


def AddRecurse(aRoot,path,config):
    try:
        ents = os.listdir(aRoot+path)
    except OSError:
        return config
    for e in ents:
        src = aRoot+path+"/"+e
        if os.path.isdir(src):
            name = src[-3:]
            if os.path.isfile(src+"/blockdevicedata.bin") and \
                    os.path.isfile(src+"/index.dat"):
                config = AddSsdTable(src,int(e),config)
            else:
                config = AddRecurse(aRoot,path+"/"+e,config)
	rematch = re.search('(?:a51_table_)?([\d]+)\.dlt', e)
	if rematch:
            config = AddDeltaTable(src,int(rematch.group(1)),config)
    return config

c = ReadConfig("tables.conf")
#print FindNextFree(c)
c = AddRecurse(sys.argv[1],"",c)
WriteConfig(c,"tables.conf")
