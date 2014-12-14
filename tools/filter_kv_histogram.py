#!/usr/bin/python
# -*- coding: utf-8 -*-

from sys import stdin;

ignore_keys = {}
for line in open("osm_tags.cc"):
    #print line.count('"') 
    if line.count('"') == 2:
        key = line.split('"')[1]
        #print key
        ignore_keys[key] = key

for line in stdin:
    split_pos = line.find(" ", 6);    
    count = int( line[:split_pos])
    key,value = line[split_pos+1:-1].split("§")[0:2]
    if key in ignore_keys: continue;
    print str(count)+"§"+key+"§"+value


