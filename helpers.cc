
#include "helpers.h"

#include <sys/stat.h>
#include <sys/types.h>  //for mkdir
#include <stdio.h>      //for perror

#include <sys/time.h>
#include <sys/resource.h>


#include <boost/foreach.hpp>


using namespace std;

double getWallTime()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
}


void ensureDirectoryExists(string directory)
{
    struct stat dummy;
    size_t start_pos = 0;
    
    do
    {
        size_t pos = directory.find('/', start_pos);
        string basedir = directory.substr(0, pos);  //works even if no slash is present --> pos == string::npos
        if (0!= stat(basedir.c_str(), &dummy)) //directory does not yet exist
            if (0 != mkdir(basedir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
                { perror("[ERR] mkdir");}
        if (pos == string::npos) break;
        start_pos = pos+1;
    } while ( true);
}

map<string, uint32_t> zone_entries;

void createEmptyFile(string file_base)
{
    size_t pos = file_base.rfind('/');
    string directory = file_base.substr(0, pos);
    ensureDirectoryExists(directory);
    
    FILE* f = fopen(file_base.c_str(), "ab");
    fclose(f);
}


void dumpPolygon(string file_base, const vector<Vertex>& poly)
{
    size_t pos = file_base.rfind('/');
    string directory = file_base.substr(0, pos);
    ensureDirectoryExists(directory);
    
    FILE* f = fopen(file_base.c_str(), "ab");
    uint64_t nVertices = poly.size();
    fwrite( &nVertices, sizeof(nVertices), 1, f);
    
    BOOST_FOREACH( const Vertex vertex, poly)
    {
        fwrite(&vertex.x, sizeof(vertex.x), 1, f);
        fwrite(&vertex.y, sizeof(vertex.y), 1, f);
    }
    
    fclose(f);
}


