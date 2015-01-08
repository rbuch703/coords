
#include "osmMappedTypes.h"
#include <string.h> //for strlen
#include <fcntl.h>  //for sync_file_range()
#include <sys/mman.h>   //for madvise()
#include <unistd.h> //for sysconf()

#include <iostream>

using namespace std;
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}

//==================================
OsmLightweightWay::OsmLightweightWay( FILE* src): 
        isDataMapped(false), vertices(NULL), tagBytes(NULL)
{
    MUST( 1 == fread(&this->id, sizeof(this->id), 1, src), "read failure");
    MUST( 1 == fread(&this->numVertices, sizeof(uint16_t), 1, src), "read failure");
        
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
    this->numVertices = *( (uint16_t*)(dataPtr));
    this->vertices = (OsmGeoPosition*)(dataPtr + 2);
    dataPtr += (2 + sizeof(OsmGeoPosition) * this->numVertices);

    this->numTags = *((uint16_t*)dataPtr);
    this->numTagBytes  = *((uint32_t*) (dataPtr + 2));
    this->tagBytes = (dataPtr + 6); 
}

uint64_t OsmLightweightWay::size() const {
    return   sizeof(id) 
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
    assert (id > 0);
    //get offset at which the dumped way *starts*
    //uint64_t offset = index_map ? ftello(dest) : 0;
    MUST(1 == fwrite(&this->id, sizeof(this->id), 1, dest), "write error");
    
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


std::map<std::string, std::string> OsmLightweightWay::getTags() const
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

LightweightWayStore::LightweightWayStore(const char* indexFileName, const char* dataFileName) {
    mapWayIndex = init_mmap(indexFileName, true, false);
    mapWayData  = init_mmap(dataFileName, true, true);

    madvise( mapWayData.ptr, mapWayData.size, MADV_SEQUENTIAL);

}


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
    uint64_t *wayIndex = (uint64_t*)host.mapWayIndex.ptr;
    while (wayIndex[pos] == 0 && pos < endPos)
        pos+=1;            
}        


//======================================================

RelationStore::RelationStore(const char* indexFileName, const char* dataFileName) {
    mapRelationIndex = init_mmap(indexFileName, true, false);
    mapRelationData  = init_mmap(dataFileName, true, true);
}

OsmRelation RelationStore::operator[](uint64_t relationId) const
{
    uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
    assert(relationIndex[relationId] != 0 && "trying to access non-existent relation");
    uint64_t dataOffset = relationIndex[relationId];
    return OsmRelation((uint8_t*)mapRelationData.ptr + dataOffset, relationId);

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

