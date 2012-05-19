#!/usr/bin/python
# -*- coding: utf-8 -*-

first = True

key_map = {}

split_pos = -1;
for line in open("kv_sorted.csv").readlines():
    if first:
        first = False;
        split_pos = line.find(" ")
    
    count = int( line[:split_pos])
    key = line[split_pos+1:].split("§")[0]
    
    if key in key_map: key_map[key]+= count
    else: key_map[key] = count

key_list = []
for key, value in key_map.iteritems():
    key_list.append( (value, key))
    
key_list.sort(reverse=True);

for value, key in key_list:
    print str(value)+"§"+key

