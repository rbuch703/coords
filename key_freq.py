#!/usr/bin/python
# -*- coding: utf-8 -*-

from sys import argv

#first = True

ignore_keys = {}
for line in open("osm_tags.cc"):
    #print line.count('"') 
    if line.count('"') == 2:
        key = line.split('"')[1]
        #print key
        ignore_keys[key] = key

key_map = {}

for line in open(argv[1]):
    split_pos = line.find(" ", 6);    
    count = int( line[:split_pos])
    key = line[split_pos+1:].split("§")[0]
    if key in ignore_keys: continue
    
    if key in key_map: key_map[key]+= count
    else: key_map[key] = count

key_list = []
for key, value in key_map.iteritems():
    key_list.append( (value, key))
    
key_list.sort(reverse=True);

for value, key in key_list:
    print str(value)+"§"+key

