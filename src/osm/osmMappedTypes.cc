
#include <string.h> //for strlen
#include <fcntl.h>  //for sync_file_range()
#include <sys/mman.h>   //for madvise()
#include <unistd.h> //for sysconf()
#include <string.h> //for memcpy
#include <iostream>

#include "osm/osmMappedTypes.h"

using namespace std;


OsmLightweightWay::OsmLightweightWay() : isDataMapped(false), vertices(NULL), numVertices(0), tagBytes(NULL), numTagBytes(0), numTags(0), id(0), version(0)
{    
}

OsmLightweightWay::OsmLightweightWay( FILE* src): 
        isDataMapped(false), vertices(NULL), tagBytes(NULL)
{
    MUST( 1 == fread(&this->id,          sizeof(this->id),          1, src), "read failure");
    MUST( 1 == fread(&this->version,     sizeof(this->version),     1, src), "read failure");
    MUST( 1 == fread(&this->numVertices, sizeof(this->numVertices), 1, src), "read failure");
        
    if (this->numVertices)
    {
        this->vertices = new OsmGeoPosition[this->numVertices];
        MUST( 1 == fread(this->vertices, sizeof(OsmGeoPosition) * this->numVertices, 1, src), "read failure");
    }
    
    //lightweight ways don't need the numTags themselves, but they are required to re-serialize the way 
    MUST( 1 == fread(&this->numTags,    sizeof(uint16_t), 1, src), "read failure");
    MUST( 1 == fread(&this->numTagBytes, sizeof(uint32_t), 1, src), "read failure");
    assert(this->numTagBytes <= 1<<17);
    
    if (this->numTagBytes)
    {
        this->tagBytes = new uint8_t[this->numTagBytes];
        MUST( 1 == fread(this->tagBytes, sizeof(uint8_t) * this->numTagBytes, 1, src), "read failure");
    }
}

OsmLightweightWay::OsmLightweightWay( uint8_t *dataPtr): 
    isDataMapped(true)
{
    this->id = *( (uint64_t*)dataPtr);
    dataPtr += sizeof( uint64_t);
    
    this->version = *( (uint32_t*)dataPtr);
    dataPtr += sizeof( uint32_t);
    
    this->numVertices = *( (uint16_t*)(dataPtr));
    this->vertices = (OsmGeoPosition*)(dataPtr + 2);
    dataPtr += (2 + sizeof(OsmGeoPosition) * this->numVertices);

    this->numTags = *((uint16_t*)dataPtr);
    this->numTagBytes  = *((uint32_t*) (dataPtr + 2));
    this->tagBytes = (dataPtr + 6); 
}


OsmLightweightWay::OsmLightweightWay( const OsmLightweightWay &other): isDataMapped(false), vertices(NULL), numVertices(0), tagBytes(NULL), numTagBytes(0), numTags(0), id(0), version(0)
{
    if (this == &other) return;
    
    *this = other;
}


OsmLightweightWay::~OsmLightweightWay()
{
    if (!isDataMapped)  // if 'vectices' and 'tagBytes' are not pointers to pre-allocated storage
    {
        delete [] vertices;
        delete [] tagBytes;
    }
}



OsmLightweightWay& OsmLightweightWay::operator=(const OsmLightweightWay &other)
{
    if (this == &other)
        return (*this);


    if (!this->isDataMapped)
    {
        delete [] this->vertices;
        delete [] this->tagBytes;
    }
    
    
    this->id = other.id;
    this->version = other.version;
    this->numVertices = other.numVertices;
    this->numTags = other.numTags;
    this->numTagBytes = other.numTagBytes;
    this->isDataMapped= other.isDataMapped;
    
    if (this->isDataMapped) //'other' did not have ownership of its 'vertices' and 'tagBytes' anyways, so we can share them as well
    {
        this->vertices = other.vertices;
        this->tagBytes = other.tagBytes;
    } else
    {
        this->vertices = new OsmGeoPosition[this->numVertices];
        memcpy( this->vertices, other.vertices, this->numVertices * sizeof(OsmGeoPosition) );
        
        this->tagBytes = new uint8_t[this->numTagBytes];
        memcpy( this->tagBytes, other.tagBytes, this->numTagBytes * sizeof( uint8_t));
    }
    
    return (*this);
}

uint64_t OsmLightweightWay::size() const {
    return   sizeof(id)
           + sizeof(version)
           + sizeof(numVertices) + numVertices* sizeof(OsmGeoPosition) 
           + sizeof(numTags) + sizeof(numTagBytes) + numTagBytes;
}

Envelope OsmLightweightWay::getBounds() const
{
    MUST( numVertices > 0, "cannot create envelope for empty way");
    Envelope aabb( vertices[0].lat, vertices[0].lng);
    
    for (uint64_t i = 0; i < numVertices; i++)
        aabb.add( vertices[i].lat, vertices[i].lng );

    return aabb;
}



void OsmLightweightWay::serialize( FILE* dest/*, mmap_t *index_map*/) const
{
//    cout << "id: " << this->id << ", numVertices " << this->numVertices <<", numTagBytes " << this->numTagBytes<< endl;
    assert (id > 0);
    //get offset at which the dumped way *starts*
    //uint64_t offset = index_map ? ftello(dest) : 0;
    MUST(1 == fwrite(&this->id,      sizeof(this->id),      1, dest), "write error");
    MUST(1 == fwrite(&this->version, sizeof(this->version), 1, dest), "write error");
    
    MUST(this->numVertices <= 2000, "#refs in way beyond what's allowed by spec");
    MUST(1 == fwrite(&this->numVertices, sizeof(this->numVertices), 1, dest), "write error");

    if(this->numVertices > 0)
        MUST(1 == fwrite( this->vertices, sizeof(OsmGeoPosition) * this->numVertices, 1, dest), "write error");

    assert(this->numTagBytes <= 1<<17);
    MUST( 1 == fwrite(&this->numTags,     sizeof(uint16_t), 1, dest), "write error");
    MUST( 1 == fwrite(&this->numTagBytes, sizeof(uint32_t), 1, dest), "write error");
    
    if (this->numTagBytes > 0)
        MUST( 1 == fwrite(this->tagBytes, sizeof(uint8_t) * this->numTagBytes, 1, dest), "read failure");
}

uint8_t* OsmLightweightWay::serialize( uint8_t* dest) const
{
    assert(false && "UNTESTED CODE!!!");
    assert (id > 0);
    
    *((uint64_t*)dest) = id;
    dest += sizeof(uint64_t);

    *((uint32_t*)dest) = version;
    dest += sizeof(uint32_t);
    
    MUST(this->numVertices <= 2000, "#refs in way beyond what's allowed by spec");
    
    *((uint16_t*)dest) = this->numVertices;
    dest += sizeof(uint16_t);
    
    
    if(this->numVertices > 0)
    {
        memcpy(dest, this->vertices, sizeof(OsmGeoPosition) * this->numVertices);
        dest +=                      sizeof(OsmGeoPosition) * this->numVertices;
    }

    assert(this->numTagBytes <= 1<<17);
    
    *((uint16_t*)dest) = this->numTags;
    dest += sizeof(uint16_t);
    
    *((uint32_t*)dest) = this->numTagBytes;
    dest += sizeof( uint32_t);
    
    if (this->numTagBytes > 0)
    {
        memcpy( dest, this->tagBytes, sizeof(uint8_t) * this->numTagBytes);
        dest                       += sizeof(uint8_t) * this->numTagBytes;
    }
    return dest;
}


std::map<std::string, std::string> OsmLightweightWay::getTagSet() const
{
    map<string, string> tags;
    const char *tagPos = (const char*)this->tagBytes;
    const char *tagEnd = (const char*)(this->tagBytes + numTagBytes);
    while (tagPos < tagEnd)
    {
        const char* key = tagPos;
        tagPos += (strlen(tagPos) + 1);
        const char* value=tagPos;
        tagPos += (strlen(tagPos) + 1);
        tags.insert( make_pair( string(key), string(value) ) );
    }
    
    assert(tags.size() == this->numTags);
    return tags;
}

bool OsmLightweightWay::hasKey( const char* key) const
{
    const char *tagPos = (const char*)this->tagBytes;
    const char *tagEnd = (const char*)(this->tagBytes + numTagBytes);
    while (tagPos < tagEnd)
    {
        //const char* key = tagPos;
        if (strcmp(tagPos, key) == 0) return true;
        tagPos += (strlen(tagPos) + 1); //skip beyond the key
        tagPos += (strlen(tagPos) + 1); //skip beyond the corresponding value
    }
    return false;
}

string OsmLightweightWay::getValue( const char* key) const
{
    const char *tagPos = (const char*)this->tagBytes;
    const char *tagEnd = (const char*)(this->tagBytes + numTagBytes);
    while (tagPos < tagEnd)
    {
        //const char* key = tagPos;
        if (strcmp(tagPos, key) == 0)   //found matching key; corresponding value is stored right behind it 
        {
            tagPos += (strlen(tagPos) + 1); //skip beyond the key
            return tagPos;
        }
        tagPos += (strlen(tagPos) + 1); //skip beyond the key
        tagPos += (strlen(tagPos) + 1); //skip beyond the corresponding value
    }
    return "";
}


ostream& operator<<(ostream &out, const OsmLightweightWay &way)
{
    out << "Way " << way.id << " (";
    for ( int i = 0; i < way.numVertices; i++)
        out << way.vertices[i].id << ", ";

    out << ") - " << way.numTagBytes << " tag bytes";
    return out;

}


//======================================================



