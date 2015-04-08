

#include <arpa/inet.h>  //for ntohl

#include <iostream>

#include <stdio.h>
#include <zlib.h>
#include <assert.h>

#include "osm/osmTypes.h"
#include "osm/osmParserPbf.h"
#include "consumers/osmConsumerCounter.h"
//#include "osmConsumerOrderEnsurer.h"

using namespace std;

namespace OSMPBF {
static const int max_uncompressed_blob_size = 32 * (1 << 20);
}

StringTable::StringTable(const google::protobuf::RepeatedPtrField<string>& src)
{
    table.reserve( src.size());
    for ( const string &s : src)
        table.push_back(s);
}
    
const string& StringTable::operator[](uint32_t idx) const
{
    MUST( idx < table.size(), "range overflow");
    return table[idx];
}


OsmParserPbf::OsmParserPbf(FILE * file, OsmBaseConsumer *consumer): 
    f(file), fileSize(0), consumer(consumer)
{
    unpackBuffer = new uint8_t[OSMPBF::max_uncompressed_blob_size];  
    
    fseek(f, 0, SEEK_END);
    fileSize = ftell(f);
    rewind(f);
}

OsmParserPbf::~OsmParserPbf() {
    delete [] unpackBuffer;
    //    google::protobuf::ShutdownProtobufLibrary();  //only necessary to satisfy valgrind

}


void OsmParserPbf::unpackBlob( const OSMPBF::Blob &blob, FILE* fIn, uint8_t *unpackBufferOut, uint32_t &unpackedSizeOut)
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

string OsmParserPbf::prepareBlob(uint8_t *unpackBuffer, uint32_t &dataSizeOut)
{
    OSMPBF::BlobHeader header;    
    OSMPBF::Blob blob;

    uint32_t size;
    MUST(fread(&size, sizeof(size), 1, f) == 1, "could not read size indicator");
    size = ntohl(size);

    MUST(size <= (1 << 15), "blob header too big");
    MUST(fread(unpackBuffer, size, 1, f) == 1, "fread failed");
    MUST( header.ParseFromArray(unpackBuffer, size), "Failed to parse BlobHeader");
    //delete [] headerBuffer;

    size = header.datasize();
    MUST(size <= OSMPBF::max_uncompressed_blob_size, "blob header bigger than allowed 32 MiB" );    
    //cout<< "blob header for block type '" << header.type() << "' with raw size " << size << endl;
    
    MUST(fread(unpackBuffer, size, 1, f) == 1, "fread failed");
    MUST( blob.ParseFromArray(unpackBuffer, size), "could not parse blob");

    /*if (blob.has_raw())
        cout << " contains raw data";
    if (blob.has_raw_size())
        cout << " contains compressed data, " << blob.zlib_data().size() << "-> "<<  blob.raw_size() << " bytes";*/
        
    cout << "\e[K" << endl;

    unpackBlob(/*ref*/blob, f, unpackBuffer, /*ref*/dataSizeOut);
    assert(dataSizeOut == (uint32_t)blob.raw_size());
    return header.type();

}

void OsmParserPbf::parseDenseNodes( const OSMPBF::DenseNodes &nodes, const StringTable &stringTable, int32_t granularity, int64_t lat_offset, int64_t lon_offset)
{
    //cout << "\t\t\tcontain " << (nodes.has_denseinfo() ? "":"NO ") << "DenseInfo block" << endl;
    int64_t numNodes = nodes.id().size();
    //cout << "\t\t\tcontain " << numNodes << " nodes" << endl;
    //cout << "\t\t\tgot passed gran/dLat/dLng:" << granularity << "/" << lat_offset << "/" << lon_offset << endl;
    MUST( nodes.lat().size() == numNodes, "property count mismatch"); 
    MUST( nodes.lon().size() == numNodes, "property count mismatch"); 

    bool hasDenseInfo = nodes.has_denseinfo();    
    bool containsVisibilityInformation = nodes.denseinfo().visible().size();
    
    if (nodes.has_denseinfo())
    {
        MUST( nodes.denseinfo().version().size() == numNodes, "property count mismatch"); 
        MUST( nodes.denseinfo().timestamp().size() == numNodes, "property count mismatch"); 
        MUST( nodes.denseinfo().changeset().size() == numNodes, "property count mismatch"); 
        MUST( nodes.denseinfo().uid().size() == numNodes, "property count mismatch"); 
        MUST( nodes.denseinfo().user_sid().size() == numNodes, "property count mismatch"); 
        MUST( nodes.denseinfo().user_sid().size() == numNodes, "property count mismatch"); 
        MUST( (!containsVisibilityInformation) ||
              nodes.denseinfo().visible().size() == numNodes, "property count mismatch"); 
    }
    
    MUST(hasDenseInfo, "parsing under absence of denseinfo not implemented");
    int64_t prevId = 0;
    int64_t prevLat= 0;
    int64_t prevLon= 0;
    int64_t keyValPos = 0;
/*    int64_t prevTimeStamp=0;
    int64_t prevChangeset=0;
    int32_t prevUid = 0;
    int32_t prevUsid = 0;*/
    for (int i = 0; i < numNodes; i++)
    {
        int64_t id =     prevId +  nodes.id().Get(i);
        int64_t latRaw = prevLat + nodes.lat().Get(i);
        int64_t lonRaw = prevLon + nodes.lon().Get(i);
        int32_t version= nodes.denseinfo().version().Get(i);
/*        int64_t timeStamp= prevTimeStamp+ nodes.denseinfo().timestamp().Get(i);
        int64_t changeset= prevChangeset+ nodes.denseinfo().changeset().Get(i);
        int32_t uid      = prevUid      + nodes.denseinfo().uid().Get(i);
        int32_t usid     = prevUsid     + nodes.denseinfo().user_sid().Get(i);
        bool    visible  = (!containsVisibilityInformation) || nodes.denseinfo().visible().Get(i);*/
        
        //cout << "lat/lng:" << latRaw << "/" << lonRaw << endl;
        OsmNode node( (int32_t)(latRaw * granularity/100 + lat_offset), (int32_t)(lonRaw * granularity/100 + lon_offset), id, version);
        
        //FIXME: this check is necessary to parse planet dumps; add it to the specification in the wiki
        if (nodes.keys_vals().size() > 0)   
        {
            while ( nodes.keys_vals().Get(keyValPos) != 0)
            { 
                MUST(keyValPos+1 < nodes.keys_vals().size(), "overflow in key/val id list");
                int32_t sid = nodes.keys_vals().Get(keyValPos);
                const string &key = stringTable[sid];
                sid = nodes.keys_vals().Get(keyValPos+1);
                MUST(sid != 0, "key without value");
                const string &value = stringTable[sid];
                keyValPos += 2;
                node.tags.push_back(make_pair(key, value));
                
            }
            keyValPos ++;   //already observed the "end-of-key-value-list" marker, now step beyond it   
        }        
        consumer->consumeNode(node);
        //cout << "\t\t\t" << node << endl; 
        
        prevId = id, prevLat = latRaw, prevLon = lonRaw/*, prevTimeStamp = timeStamp, 
        prevChangeset = changeset,prevUid = uid, prevUsid = usid*/;
    }
    assert (keyValPos == nodes.keys_vals().size() && "extraneous key/value pair(s)");
}

void OsmParserPbf::parseWays(const google::protobuf::RepeatedPtrField<OSMPBF::Way> &ways, const StringTable &stringTable)
{
//    cout << "\t\tprimitiveGroup contains " << ways.size() << " ways" << endl;    
    for (const OSMPBF::Way &way : ways)
    {
        MUST( way.keys().size()== way.vals().size(), "extraneous key or value");
        int32_t version = way.has_info() && way.info().has_version() ? 
            way.info().version() : 1;
        OsmWay osmWay(way.id(), version);

        //osmWay.tags.reserve( way.keys().size());        
        for (int i = 0; i < way.keys().size(); i++)
            osmWay.tags.push_back( make_pair( stringTable[way.keys().Get(i)], stringTable[way.vals().Get(i)] ));
            

        int64_t nodeId = 0;
        //osmWay.refs.reserve( way.refs().size());
        for (int64_t nodeIdRaw : way.refs())
        {
            nodeId += nodeIdRaw;
            MUST(nodeId > 0, "invalid node id");
            osmWay.refs.push_back( (OsmGeoPosition){.id = (uint64_t)nodeId,
                                                    .lat= INVALID_LAT_LNG,
                                                    .lng= INVALID_LAT_LNG} );
        } 

        consumer->consumeWay(osmWay);

        //cout << osmWay << endl;
    }
    
}

void OsmParserPbf::parseRelations(const google::protobuf::RepeatedPtrField<OSMPBF::Relation> &rels, const StringTable &stringTable)
{
    for (const OSMPBF::Relation &rel : rels)
    {
        int32_t version = rel.has_info() && rel.info().has_version() ? 
            rel.info().version() : 1;

        OsmRelation osmRel(rel.id(), version);
        MUST( rel.keys().size() == rel.vals().size(), "extraneous key or value");
        MUST( rel.roles_sid().size() == rel.memids().size(), "incomplete (type,role,ref) triple");
        MUST( rel.roles_sid().size() == rel.types().size(), "incomplete (type,role,ref) triple");

        //osmRel.tags.reserve( rel.keys().size());        
        for (int i = 0; i < rel.keys().size(); i++)
            osmRel.tags.push_back( make_pair( stringTable[rel.keys().Get(i)], stringTable[rel.vals().Get(i)] ));
            
        int64_t ref = 0;
        //rel.members.reserve( rel.roles_sid().size());
        for (int i = 0; i < rel.roles_sid().size(); i++)
        {
            ref += rel.memids().Get(i);
            const string& role = stringTable[rel.roles_sid().Get(i)];
            OSMPBF::Relation::MemberType iType = (OSMPBF::Relation::MemberType)rel.types().Get(i);
            
            ELEMENT type = (iType == OSMPBF::Relation::NODE) ? NODE : 
                           (iType == OSMPBF::Relation::WAY)  ? WAY :
                           (iType == OSMPBF::Relation::RELATION) ? RELATION: OTHER;
            MUST( type == NODE || type == WAY || type == RELATION, "invalid type");
            osmRel.members.push_back( OsmRelationMember(type, ref, role));
        }
        
        consumer->consumeRelation(osmRel);
    }
}

void OsmParserPbf::parse()
{    
    assert(f);
    int ch;
    cout << endl << endl << "\e[2A"; //reserve 2 lines of space for message
    cout << "\e[s"; //mark cursor position for future status updates
    
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);

        uint32_t size;
        uint64_t filePos = ftello(f);
        string blobType = prepareBlob(unpackBuffer, size);
        
        if ( blobType == "OSMHeader")
        {    
            //cout << "parsing headerBlock" << endl;
            OSMPBF::HeaderBlock headerBlock;
            MUST( headerBlock.ParseFromArray(unpackBuffer, size), "failed to parse HeaderBlock");
            /*for (string s: headerBlock.required_features())
                cout << "\trequires feature '" << s << "'" << endl;

            for (string s: headerBlock.optional_features())
                cout << "\thas optional feature " << s << "'" << endl;
                
            cout << "\twritten by '" << headerBlock.writingprogram() << "'" << endl;
            cout << endl << endl << "\e[2A";
            cout << "\e[s"; //mark cursor position for future status updates*/
        } else if (blobType == "OSMData")
        {
            cout << "\e[u"; //move cursor to saved position
            cout << "Processing data at file position "<< (filePos / 1000000) << "M (" << (filePos*100/fileSize) << "%)" << endl;
            

            OSMPBF::PrimitiveBlock primBlock;
            MUST( primBlock.ParseFromArray(unpackBuffer, size), "failed to parse PrimBlock");
                
            //cout << "\tstringtable has " << primBlock.stringtable().s().size() << " entries" << endl;            
            StringTable strings(primBlock.stringtable().s());
            //vector<string> strings;

            int32_t granularity = primBlock.has_granularity() ? primBlock.granularity() : 100;
            int64_t lat_offset = primBlock.has_lat_offset() ? primBlock.lat_offset() : 0;
            int64_t lon_offset = primBlock.has_lon_offset() ? primBlock.lon_offset() : 0;
            //int32_t date_granularity = primBlock.has_date_granularity() ? primBlock.date_granularity() : 1000;
            /*cout << "\tgranularity=" << granularity 
                 << " offset=" << lat_offset << "/" << lon_offset 
                 << " date_granularity=" << date_granularity << endl;*/
            //for (const string &s : primBlock.stringtable().s())
            //    strings.push_back(s);
                
            //cout << "\t" << primBlock.primitivegroup().size() << " primitive groups" << endl;
            
            for (const OSMPBF::PrimitiveGroup &primGroup : primBlock.primitivegroup())
            {
                //NOTE: more than one of these may be present in a given primitive group

                //cout << "\t\tgroup contains " << primGroup.nodes().size() << " nodes" << endl;
                if (primGroup.nodes().size())
                    MUST(false, "NOT IMPLEMENTED");
                    
                //cout << "\t\tgroup contains " << (primGroup.has_dense() ? "":"NO ") << "dense nodes" << endl;
                if (primGroup.has_dense())
                    parseDenseNodes( primGroup.dense(), strings, granularity, lat_offset, lon_offset);

                //cout << "\t\tgroup contains " << primGroup.ways().size() << " ways" << endl;
                if (primGroup.ways().size())
                    parseWays( primGroup.ways(), strings);
                
                //cout << "\t\tgroup contains " << primGroup.relations().size() << " relations" << endl;
                if (primGroup.relations().size())
                    parseRelations( primGroup.relations(), strings);
                
                //cout << "\t\tgroup contains " << primGroup.changesets().size() << " changesets" << endl;
                if (primGroup.changesets().size())
                    MUST(false, "NOT IMPLEMENTED");
                
            }
//                cout << "string '" << s << "'" << endl;
            //cout << "\tfirst string is '" << primBlock.stringtable().s().c_str() << "'" << endl;
 //           exit(0);
        
        } else MUST(false, "invalid header type");
    }
    //delete [] unpackBuffer;
    //if (!) { cerr << "Failed to parse blob header" << endl; exit(1); }
}

