
#include "osmParserPbf.h"
#include "fileformat.pb.h"
#include "osmformat.pb.h"

#include <arpa/inet.h>  //for ntohl

#include <iostream>

#include <stdio.h>
#include <zlib.h>
#include <assert.h>

using namespace std;

namespace OSMPBF {
static const int max_uncompressed_blob_size = 32 * (1 << 20);
}

//a macro similar to assert(), but is not deactivated by NDEBUG
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); exit(EXIT_FAILURE);}}

void unpackBlob( const OSMPBF::Blob &blob, FILE* fIn, uint8_t *unpackBufferOut, uint32_t &unpackedSizeOut)
{
    if (blob.has_raw())
    {
        /*FIXME: according to protobuf-documentation, the blob should already contain the raw data
                 in its blob.raw() property. However, according to the OSM PBF sample code,
                 the blob does not contain this data by itself, and the data needs to be loaded
                 manually from the file.
        */
        MUST(false, "unimplemented");
        fclose(fIn); //just to get rid of compiler warning
        //memcpy(unpackBuffer, blob.raw
        return;
    }
    
    if (blob.has_zlib_data())
    {
            // zlib information
        z_stream z;
        z.next_in = (unsigned char*) blob.zlib_data().c_str(),             // next byte to decompress
        z.avail_in  = blob.zlib_data().size(),  // number of bytes to decompress
        z.next_out  = (unsigned char*) unpackBufferOut, // place of next decompressed byte
        z.avail_out = OSMPBF::max_uncompressed_blob_size,     // space for decompressed data
        z.zalloc    = Z_NULL,
        z.zfree     = Z_NULL,
        z.opaque    = Z_NULL;

        MUST(inflateInit(&z) == Z_OK,  "failed to init zlib stream");
        MUST(inflate(&z, Z_FINISH) == Z_STREAM_END, "failed to inflate zlib stream");
        MUST(inflateEnd(&z) == Z_OK, "failed to deinit zlib stream");

        unpackedSizeOut = z.total_out;
        return;
    };

    assert(false && "Not implemented");

}

string prepareBlob(FILE* f, uint8_t *unpackBuffer, uint32_t &dataSizeOut)
{
    OSMPBF::BlobHeader header;    
    OSMPBF::Blob blob;

    uint32_t size;
    fread(&size, sizeof(size), 1, f);
    size = ntohl(size);

    MUST(size <= (1 << 15), "blob header too big");
    MUST(fread(unpackBuffer, size, 1, f) == 1, "fread failed");
    MUST( header.ParseFromArray(unpackBuffer, size), "Failed to parse BlobHeader");
    //delete [] headerBuffer;

    size = header.datasize();
    MUST(size <= OSMPBF::max_uncompressed_blob_size, "blob header bigger than allowed 32 MiB" );    
    cout<< "blob header for block type '" << header.type() << "' with raw size " << size << endl;
    
    MUST(fread(unpackBuffer, size, 1, f) == 1, "fread failed");

    cout << "blob at file position "<< ftello(f) ;

    MUST( blob.ParseFromArray(unpackBuffer, size), "could not parse blob");;

    if (blob.has_raw())
        cout << " contains raw data" << endl;
    if (blob.has_raw_size())
        cout << " contains compressed data, uncompressed size is " <<  blob.raw_size() << endl;

    unpackBlob(/*ref*/blob, f, unpackBuffer, /*ref*/dataSizeOut);
    assert(dataSizeOut == (uint32_t)blob.raw_size());
    return header.type();

}

int main(int argc, char** argv)
{
    
    if (argc < 2)
    {
        cout << "usage: " << argv[0] << " inputfile.pbf" << endl;
        exit(EXIT_FAILURE);
    }

    uint8_t *unpackBuffer = new uint8_t[OSMPBF::max_uncompressed_blob_size];

    FILE* f = fopen( argv[1], "rb");
    assert(f);
    int ch;

    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);

        uint32_t size;
        string blobType = prepareBlob(f, unpackBuffer, size);
        
        if ( blobType == "OSMHeader")
        {    
            cout << "parsing headerBlock" << endl;
            OSMPBF::HeaderBlock headerBlock;
            MUST( headerBlock.ParseFromArray(unpackBuffer, size), "failed to parse HeaderBlock");
            for (string s: headerBlock.required_features())
                cout << "\t contains required feature " << s << endl;

            for (string s: headerBlock.optional_features())
                cout << "\t contains required feature " << s << endl;
        } else if (blobType == "OSMData")
        {
        } else MUST(false, "invalid header type");
    }
    
    delete [] unpackBuffer;
    fclose(f);
    //if (!) { cerr << "Failed to parse blob header" << endl; exit(1); }

    google::protobuf::ShutdownProtobufLibrary();
}
