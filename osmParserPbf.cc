
#include "osmParserPbf.h"
#include "fileformat.pb.h"
#include "osmformat.pb.h"

#include <arpa/inet.h>  //for ntohl

#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        cout << "usage: " << argv[0] << " inputfile.pbf" << endl;
    }

    fstream f(argv[1], ios::in | ios::binary);
    assert(f.good());
    
    int32_t size=42;
    f.read((char*)&size, sizeof(size));
    size = ntohl(size);
    /*cout << "char " << f.get() << endl;
    cout << "char " << f.get() << endl;
    cout << "char " << f.get() << endl;
    cout << "char " << f.get() << endl;*/

    //f >> size;
    cout << "size is " << size << endl;
    OSMPBF::BlobHeader header;    
    header.ParseFromIstream(&f);
    cout << "block type is '" << header.type() << "'" << endl;
    if (header.has_indexdata());
        cout << "has index data of size '" << header.indexdata().size() << "'" << endl;

    OSMPBF::HeaderBlock hdr;
    hdr.ParseFromIstream(&f);
    cout << "has_creator: " << hdr.has_writingprogram() << endl;
    
    
    OSMPBF::Blob blob;
    blob.ParseFromIstream(&f);
    if (blob.has_raw())
        cout << "\tcontains raw data" << endl;
    if (blob.has_raw_size())
        cout << "\tcontains compressed data, uncompressed size is " <<  blob.raw_size() << endl;
        
    cout << blob.has_raw() << ", " << blob.has_raw_size() << ", " <<blob.has_zlib_data() << ", " << blob.has_lzma_data() << ", " << endl;
    //if (!) { cerr << "Failed to parse blob header" << endl; exit(1); }
    



}
