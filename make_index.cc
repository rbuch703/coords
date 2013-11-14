
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#include <string>
#include <iostream>

using namespace std;

bool exists(string filename)
{
    struct stat dummy;
    return (stat( filename.c_str(), &dummy ) == 0);
}

void make_index(string filename, FILE* f_out)
{
    cout << "scanning file " << filename << endl;
    assert(exists(filename));
    uint8_t children = 0;
    
    if ( exists( filename+"0")) children += 1;
    if ( exists( filename+"1")) children += 2;
    if ( exists( filename+"2")) children += 4;
    if ( exists( filename+"3")) children += 8;
    
    fwrite(&children, sizeof(children), 1, f_out);

    if ( exists( filename+"0")) make_index(filename+"0", f_out);
    if ( exists( filename+"1")) make_index(filename+"1", f_out);
    if ( exists( filename+"2")) make_index(filename+"2", f_out);
    if ( exists( filename+"3")) make_index(filename+"3", f_out);
    
}

void build_index(string basepath, string filename)
{
    FILE* f = fopen( (filename+".idx").c_str(), "wb");
    make_index(basepath+filename, f);
    fclose(f);
}

int main()
{
    /*build_index("output/coast/", "seg");
    build_index("output/coast/", "building");
    build_index("output/coast/", "country");
    build_index("output/coast/", "state");
    build_index("output/coast/", "residential");*/
    build_index("output/coast/", "water");
/*    FILE* f = fopen("seg.idx", "wb");
    make_index("seg", f);
    fclose(f);*/
    return 0;
}

