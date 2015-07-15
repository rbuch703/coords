#! /usr/bin/python3

idx = 0;

for row in open("names_sorted.csv").readlines():
    #print(row);
    p = row.find(", ")
    if p < 0:
        continue
        
    cnt = int(row[:p])
    name = row[p+2:-1].replace('"', '\\"')
    print ("    \"" + name + "\",")
    
    
    if idx > 255:
        break;
        
    idx += 1;
