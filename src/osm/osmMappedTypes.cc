
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

OsmLightweightWay::OsmLightweightWay( const OSMWay &other)
{
    this->id = other.id;
    this->version = other.version;
    this->isDataMapped = false;
    this->numVertices = other.refs.size();
    this->vertices = new OsmGeoPosition[this->numVertices];
    for (int i = 0; i < this->numVertices; i++)
        this->vertices[i] = other.refs[i];
        
    this->numTags = other.tags.size();
    this->numTagBytes = 0;
    for (uint64_t i = 0; i < other.tags.size(); i++)
    {
        this->numTagBytes += other.tags[i].first.size() + 1;    //  key string length including termination
        this->numTagBytes += other.tags[i].second.size() + 1;   //value string length including termination
    }
    
    this->tagBytes = new uint8_t[this->numTagBytes];
    char *pos = (char*)this->tagBytes;
    for (uint64_t i = 0; i < other.tags.size(); i++)
    {
        strcpy(pos, other.tags[i].first.c_str());
        pos += strlen( pos ) + 1;
        
        strcpy(pos, other.tags[i].second.c_str());
        pos += strlen( pos ) + 1;
    }
    assert( (uint8_t*)pos == this->tagBytes + this->numTagBytes);
    
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

OSMWay OsmLightweightWay::toOsmWay() const {
    
    map<string, string> tags = this->getTagSet();
    return OSMWay( this->id, 
                   this->version, 
                   vector<OsmGeoPosition>( this->vertices, this->vertices + this->numVertices, allocator<map<string, string> >()),
                   vector<OSMKeyValuePair>( tags.begin(), tags.end()));
}

uint64_t OsmLightweightWay::size() const {
    return   sizeof(id)
           + sizeof(version)
           + sizeof(numVertices) + numVertices* sizeof(OsmGeoPosition) 
           + sizeof(numTags) + sizeof(numTagBytes) + numTagBytes;
}


OsmLightweightWay::~OsmLightweightWay()
{
    if (!isDataMapped)  // if 'vectices' and 'tagBytes' are not pointers to pre-allocated storage
    {
        delete [] vertices;
        delete [] tagBytes;
    }
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

LightweightWayStore::LightweightWayStore(const char* indexFileName, const char* dataFileName, bool optimizeForStreaming) {
    mapWayIndex = init_mmap(indexFileName, true, false);
    mapWayData  = init_mmap(dataFileName, true, true);

    if (optimizeForStreaming)
    {
        madvise( mapWayData.ptr, mapWayData.size, MADV_SEQUENTIAL);
        madvise( mapWayIndex.ptr, mapWayIndex.size, MADV_SEQUENTIAL);
    }

}

LightweightWayStore::LightweightWayStore(const std::string baseName, bool optimizeForStreaming):
    LightweightWayStore( (baseName + ".idx").c_str(), (baseName + ".data").c_str(), optimizeForStreaming)
{ }


OsmLightweightWay LightweightWayStore::operator[](uint64_t wayId)
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    assert(wayIndex[wayId] != 0 && "trying to access non-existent way");
    uint64_t wayPos = wayIndex[wayId];
    return OsmLightweightWay((uint8_t*)mapWayData.ptr + wayPos);
}

bool LightweightWayStore::exists(uint64_t wayId) const
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    if (wayId >= getMaxNumWays())
        return false;
        
    return wayIndex[wayId] != 0;
}

void LightweightWayStore::syncRange(uint64_t lowWayId, uint64_t highWayId) const
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    while (!wayIndex[lowWayId] && lowWayId < getMaxNumWays()) lowWayId++;
    while (!wayIndex[highWayId] && highWayId < getMaxNumWays()) highWayId++;

    if (lowWayId > getMaxNumWays() ) return;
    if (highWayId > getMaxNumWays())
        highWayId = getMaxNumWays();
    if (lowWayId >= highWayId) return;
    
    uint64_t lowPos = wayIndex[lowWayId];
    uint64_t highPos= wayIndex[highWayId];
    
    
    int res = sync_file_range(mapWayData.fd, lowPos, highPos - lowPos, 
                              SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER);
    if (res != 0)
        perror("sync_file_range");

    assert(highPos > lowPos);
    //cout << "syncing file range " << lowPos << " -> " << highPos << endl;

    uint64_t addr = ((uint64_t)mapWayData.ptr) + lowPos;
    size_t pageSize = sysconf (_SC_PAGESIZE);

    addr = addr / pageSize * pageSize;

    res = madvise(  (void*)addr, highPos - lowPos, MADV_DONTNEED);
    if (res != 0)
        perror("madvise");

    
}


LightweightWayStore::LightweightWayIterator LightweightWayStore::begin() 
{ 
    return LightweightWayIterator(*this, 0);
}

LightweightWayStore::LightweightWayIterator LightweightWayStore::end()   
{ 
    return LightweightWayIterator(*this, getMaxNumWays());
}

uint64_t LightweightWayStore::getMaxNumWays() const 
{ 
    return mapWayIndex.size / sizeof(uint64_t); 
}


LightweightWayStore::LightweightWayIterator::LightweightWayIterator( LightweightWayStore &host, uint64_t pos):
    host(host), pos(pos) 
{
    advanceToNextWay();
}

LightweightWayStore::LightweightWayIterator& LightweightWayStore::LightweightWayIterator::operator++() {
    pos++;
    advanceToNextWay();
    return *this;
}

OsmLightweightWay LightweightWayStore::LightweightWayIterator::operator *() {
    return host[pos];
}

bool LightweightWayStore::LightweightWayIterator::operator !=( LightweightWayIterator &other) const
{ 
    return pos != other.pos;
} 

void LightweightWayStore::LightweightWayIterator::advanceToNextWay() {
    uint64_t endPos = host.getMaxNumWays();
    if (pos >= endPos)
        return;
    uint64_t *wayIndex = (uint64_t*)host.mapWayIndex.ptr;
    while (wayIndex[pos] == 0 && pos < endPos)
        pos+=1;            
}        


//======================================================

RelationStore::RelationStore(const char* indexFileName, const char* dataFileName) {
    mapRelationIndex = init_mmap(indexFileName, true, false);
    mapRelationData  = init_mmap(dataFileName, true, true);
}

RelationStore::RelationStore(string baseName): 
    RelationStore( (baseName + ".idx").c_str(), (baseName + ".data").c_str())
{ }


OsmRelation RelationStore::operator[](uint64_t relationId) const
{
    uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
    assert(relationIndex[relationId] != 0 && "trying to access non-existent relation");
    uint64_t dataOffset = relationIndex[relationId];
    return OsmRelation((uint8_t*)mapRelationData.ptr + dataOffset);

}

bool RelationStore::exists(uint64_t relationId) const
{
    uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
    if (relationId >= getMaxNumRelations())
        return false;
        
    return relationIndex[relationId] != 0;
}

RelationStore::RelationIterator RelationStore::begin() 
{ 
    return RelationIterator(*this, 0);
}

RelationStore::RelationIterator RelationStore::end()   
{ 
    return RelationIterator(*this, getMaxNumRelations());
}

uint64_t RelationStore::getMaxNumRelations() const 
{ 
    return mapRelationIndex.size / sizeof(uint64_t); 
}


RelationStore::RelationIterator::RelationIterator( RelationStore &host, uint64_t pos): host(host), pos(pos) 
{
    advanceToNextRelation();
}

RelationStore::RelationIterator& RelationStore::RelationIterator::operator++() 
{
    pos++;
    advanceToNextRelation();
    return *this;
}

OsmRelation RelationStore::RelationIterator::operator *() 
{
    return host[pos];
}

bool RelationStore::RelationIterator::operator !=( RelationIterator &other) const
{ 
    return pos != other.pos;
} 


void RelationStore::RelationIterator::advanceToNextRelation() {
    uint64_t endPos = host.getMaxNumRelations();
    uint64_t *relationIndex = (uint64_t*)host.mapRelationIndex.ptr;
    while (relationIndex[pos] == 0 && pos < endPos)
        pos+=1;            
}        

