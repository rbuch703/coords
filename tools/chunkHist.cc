
#include <stdio.h>

#include <map>
#include <iostream>


using namespace std;

int main()
{
    FILE* f = fopen("chunkSizes.bin", "rb");
    uint32_t chunkSize;
    
    map<uint32_t, uint64_t> chunkSizes;
    uint64_t idx = 0;
    while ( fread(&chunkSize, sizeof(chunkSize), 1, f) == 1)
    {
        idx++;
        if (idx % 1000000 == 0)
            cerr << (idx/1000000) << "M" << endl;
            
        if (!chunkSizes.count(chunkSize))
            chunkSizes.insert( make_pair(chunkSize, 0));
            
        chunkSizes[chunkSize] += 1;
    
    }
    
    for (pair<uint32_t, uint64_t> p: chunkSizes)
        cout << (p.first+1) << "\t" << p.second << endl;
}
