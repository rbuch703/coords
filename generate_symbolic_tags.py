#!/usr/bin/python
# -*- coding: utf-8 -*-

keys = []
values = []

n = 0;
for line in open("kv_germany.csv").readlines():
    line = line[:-1].replace("\\", "\\\\") # remove trailing EOL marker and escape backslashes
    parts = line.split("§")
    if len(parts) > 3:
        continue;
    count, key, value = parts
    
    keys.append(key);
    values.append(value);
    n+=1
    if n>= 65536: break;

print "const char* symbolic_tags_keys[] = {"
for key in keys[:-1]:
    print '  "'+key+'",'

print '  "'+keys[-1]+'"'
print '};'
print ''

print "const char* symbolic_tags_values[] = {"
for value in values[:-1]:
    print '  "'+value+'",'

print '  "'+values[-1]+'"'
print '};'
print ''
