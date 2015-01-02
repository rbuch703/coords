
#include "osmMappedTypes.h"

#include <iostream>

using namespace std;
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}

//==================================
OsmLightweightWay::OsmLightweightWay( FILE* src, uint64_t way_id): 
        isDataMapped(false), vertices(NULL), tagBytes(NULL), id(way_id)
{
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

OsmLightweightWay::OsmLightweightWay( uint8_t *dataPtr, uint64_t way_id): 
    isDataMapped(true), id(way_id)
{
    this->numVertices = *( (uint16_t*)(dataPtr));
    this->vertices = (OsmGeoPosition*)(dataPtr + 2);
    dataPtr += (2 + sizeof(OsmGeoPosition) * this->numVertices);

    this->numTags = *((uint16_t*)dataPtr);
    this->numTagBytes  = *((uint32_t*) (dataPtr + 2));
    this->tagBytes = (dataPtr + 6); 
}


OsmLightweightWay::~OsmLightweightWay()
{
    if (!isDataMapped)
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

}


OsmLightweightWay LightweightWayStore::operator[](uint64_t wayId)
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    assert(wayIndex[wayId] != 0 && "trying to access non-existent way");
    uint64_t wayPos = wayIndex[wayId];
    return OsmLightweightWay((uint8_t*)mapWayData.ptr + wayPos, wayId);

}

bool LightweightWayStore::exists(uint64_t wayId) const
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    if (wayId >= getMaxNumWays())
        return false;
        
    return wayIndex[wayId] != 0;
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

