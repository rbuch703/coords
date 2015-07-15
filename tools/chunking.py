#! /usr/bin/python3


#chunkSizes = [2**x for x in range(32)][5:]
#chunkSizes = [ int(27* (1.335**x)) for x in range(64)]
#chunkSizes = [ int(27* (1.1551**x)) for x in range(64)]
#chunkSizes = [0] + [ int(27* (1.1551**x)) for x in range(55)] + [2**(2*x+17) for x in range(8)]

#chunkSizes = [0] + [ int(27* (2**x)) for x in range(55)] + [2**(2*x+17) for x in range(8)]

#chunkSizes = [0, 27, 34, 51, 60, 69, 77, 91, 104, 112, 124, 138, 152, 168, 185, 204, 225, 245, 267, 297, 332, 371, 413, 465, 523, 592, 670, 761, 868, 985, 1128, 1298, 1493, 1721, 2002, 2337, 2748, 3254, 3882, 4681, 5702, 7005, 8579, 10881, 14016, 18188, 24089, 32051, 1018737, 7599824371187712, 15199648742375424, 30399297484750848, 60798594969501696, 121597189939003392, 243194379878006784, 486388759756013568, 131072, 524288, 2097152, 8388608, 33554432, 134217728, 536870912, 2147483648];

#chunkSizes = [0] + ;

chunkSizes = [ 0 ];
chunkSizes += [ 1<<17 for x in range(55)] # dummy values to be optimized
chunkSizes += [ 1<<17, 1<<19, 1<<21, 1<<23, 1<<25, 1<<27, 1<<29, 1<<31] # powers of two up 2GB

#best chunk sizes: [0, 27, 33, 46, 57, 64, 72, 81, 92, 104, 110, 122, 136, 146, 156, 169, 184, 201, 219, 238, 259, 279, 299, 329, 362, 400, 440, 489, 543, 602, 671, 746, 834, 927, 1035, 1162, 1306, 1471, 1659, 1881, 2140, 2444, 2807, 3233, 3746, 4371, 5131, 6083, 7251, 8547, 10433, 12858, 16030, 20062, 25435, 32174, 131072, 524288, 2097152, 8388608, 33554432, 134217728, 536870912, 2147483648] --> 3.063 GB slack

print (chunkSizes)
print(len(chunkSizes))
     
#print (chunkSizes);
#print(chunkSizes[63]/1000000000);
#print();


def getSlacks( chunkSizes, dataSizes):
    
    slacks = [0 for x in chunkSizes];
    csPos = 0;
    for size, count in dataSizes:
        while chunkSizes[csPos] < size:
            csPos+= 1;
            
        #print(size, "->", chunkSizes[i]);
        slacks[csPos] += ( chunkSizes[csPos] - size) * count;
    return slacks;

dataSizes = [];

def getMaxPos( lst):
    maxPos = 0;
    maxVal = lst[0]
    for i in range( len(lst)): 
        if lst[i] > maxVal:
            maxVal = lst[i];
            maxPos = i;
    return maxPos;

for row in open("chunkSizes.txt").readlines():
    parts = row[:-1].split("\t");
    
    tpl = ( int(parts[0]), int(parts[1]));
    dataSizes.append(tpl);
#    print (tpl);

def approach2(dataSizes):
    memUsage = [ (x[0], (x[0]*x[1])/1000000) for x in dataSizes]
    chunkSizes2 = [];
    #print(memUsage);
    dataLeft = (sum( x[1] for x in memUsage))
    pos = 0;
    for i in range(50):
        dataSum = 0;
        blocksLeft = 50 - i;
        dataForBlock = dataLeft/blocksLeft;
        print("have to fill block with at least", dataForBlock, "MB of data");
        
        
        while dataSum < dataForBlock and pos < len(memUsage):
            dataSum += memUsage[pos][1];
            pos += 1;
        
        print( "found", dataSum, "MB of data till block size", memUsage[pos-1][0])
        chunkSizes2.append( memUsage[pos-1][0]);
        dataLeft -= dataSum;

    print(chunkSizes2);
    print( sum(getSlacks(chunkSizes2, dataSizes)));

def approach1(chunkSizes, dataSizes):

    minSlack = 1E100;
    minSlackChunkSizes = chunkSizes;
    
    #for i in range(100000):
    i = 0;
    while True:
        slacks = [x/1000000000 for x in getSlacks( chunkSizes, dataSizes)];
        
        if sum(slacks) < minSlack:
            minSlack = sum(slacks)
            minSlackChunkSizes = list(chunkSizes);
        i+= 1;  
        if i%1000 == 0:
            print( "current:",sum(slacks), "GB, best:", minSlack, "GB");
            print( "best chunk sizes:", minSlackChunkSizes);
        # keep last ten entries fixed. Their purpose is to provide over-sizes chunk sizes
        # in case those occur (even if they are extremely unlikely)
        maxPos = getMaxPos(slacks[:-8]);   

        diff = chunkSizes[maxPos] - chunkSizes[maxPos-1];
        chunkSizes[maxPos] -= (int(diff/100)+1);

    print(slacks);
    print(chunkSizes);
    
approach1(chunkSizes, dataSizes);
