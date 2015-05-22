
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <string.h>

using namespace std;

struct Tuple {
    uint64_t oldId;
    uint64_t newId;
};

int main(int argc, const char** argv)
{
    assert(argc == 3);
    assert(strlen(argv[1]) == 1);
    if (argc != 3 || strlen(argv[1]) != 1)
        exit(EXIT_FAILURE);
        
    char ch = argv[1][0];
    
    uint64_t relId = atoll(argv[2]);
    
    string filename;
    switch (ch)
    {
        case 'n' : filename = "intermediate/mapNodes.idx"; break;
        case 'w' : filename = "intermediate/mapWays.idx" ; break;
        case 'r' : filename = "intermediate/mapRelations.idx";break;
    }
    
    static const char* baseUrl = "http://www.openstreetmap.org/";
    //cout << "opening file " << filename << endl;
    FILE* f = fopen( filename.c_str(), "rb");
    uint64_t numItems;
    fread(&numItems, sizeof(numItems), 1, f);
    while (numItems--)
    {
        Tuple tpl;
        fread(&tpl, sizeof(tpl), 1, f);
        //cout << tpl.oldId << ", " << tpl.newId << endl;
        if (tpl.newId == relId)
        {
            cout << tpl.oldId << " - ";
            switch (ch)
            {
                case 'n': cout << baseUrl << "node/" << tpl.oldId << endl; break;
                case 'w': cout << baseUrl << "way/" << tpl.oldId << endl; break;
                case 'r': cout << baseUrl << "relation/" << tpl.oldId << endl; break;
            }
            //cout << tpl.oldId << " -> " << tpl.newId << endl;
            return EXIT_SUCCESS;
        }
    }
    
    cout << "no match found" << endl;
}
