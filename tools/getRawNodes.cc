
#include <stdint.h>
#include "osm_types.h"
#include <stdio.h>
#include <iostream>

#include <assert.h>

#include <fcntl.h>

using namespace std;


int main() {
    FILE* fNodeIdx = fopen("intermediate/nodes.idx", "rb");
    FILE* fNodeData= fopen("intermediate/nodes.data", "rb");
    FILE* fNodesRaw =fopen("intermediate/nodes.raw", "wb");
    int fidNodeIdx =  fileno(fNodeIdx);
    int fidNodeData = fileno(fNodeData);
    int fidNodeRaw =  fileno(fNodesRaw);
    
    posix_fadvise(fidNodeIdx, 0, 0, POSIX_FADV_SEQUENTIAL); //bigger read-ahead window
    posix_fadvise(fidNodeData, 0, 0, POSIX_FADV_SEQUENTIAL); //bigger read-ahead window


     
    fseek(fNodeIdx, 0, SEEK_END);
    uint64_t numNodes = ftell(fNodeIdx) / sizeof(int64_t);
     
    //Nodes nodes("intermediate/nodes.idx", "intermediate/nodes.data");
    
    for (uint64_t idx = 0; idx < numNodes; idx++)
    {
        if (idx % 10000000 == 0) 
        {
            cout << (idx/1000000) << "M nodes read." << endl;
            int posix_fadvise(int fd, off_t offset, off_t len, int advice);
            size_t pos = ftell(fNodeIdx);
            posix_fadvise(fidNodeIdx, 0, pos, POSIX_FADV_DONTNEED); //free cache;

            pos = ftell(fNodeData);
            posix_fadvise(fidNodeData, 0, pos, POSIX_FADV_DONTNEED); //free cache;

            pos = ftell(fNodesRaw);
            //posix_fadvise(fidNodeRaw, 0, pos, POSIX_FADV_DONTNEED); //free cache;

        }

                
        OSMNode node(fNodeIdx, fNodeData, idx);
        if ( node.id == -1)
            continue;

        //cout << node.id << endl;
              
       fwrite( &node.lat, sizeof(node.lat), 1, fNodesRaw );
       fwrite( &node.lon, sizeof(node.lon), 1, fNodesRaw );
        
    }
    
    fclose(fNodesRaw);

    return 0;
}

