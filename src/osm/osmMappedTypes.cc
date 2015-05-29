
#include <string.h> //for strlen
#include <string.h> //for memcpy
#include <math.h>   //for fabs()

#include <iostream>

#include "osm/osmMappedTypes.h"
#include "misc/varInt.h"

OsmLightweightWay::OsmLightweightWay() : isDataMapped(false), vertices(NULL), numVertices(0), tagBytes(NULL), numTagBytes(0), id(0), version(0)
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
    
    this->numTagBytes = varUintFromFile( src, nullptr);
    assert(this->numTagBytes <= 1<<20);
    
    uint8_t bytes[10];
    int nRead = varUintToBytes(this->numTagBytes, bytes);
    
    this->tagBytes = new uint8_t[this->numTagBytes + nRead];
    for (int i= 0; i < nRead; i++)
        this->tagBytes[i] = bytes[i];
        
   
    if (this->numTagBytes > 0)
    {
        
        MUST( 1 == fread( &this->tagBytes[nRead], this->numTagBytes, 1, src), "read failure");
    }
    
    this->numTagBytes += nRead;
}

OsmLightweightWay::OsmLightweightWay( uint8_t *dataPtr): 
    isDataMapped(true)
{
    this->id = *( (uint64_t*)dataPtr);
    dataPtr += sizeof( uint64_t);
    
    this->version = *( (uint32_t*)dataPtr);
    dataPtr += sizeof( uint32_t);
    
    this->numVertices = *( (uint16_t*)(dataPtr));
    dataPtr += sizeof(uint16_t);
    
    this->vertices = (OsmGeoPosition*)(dataPtr);
    dataPtr +=  (sizeof(OsmGeoPosition) * this->numVertices);

    int nRead = 0;
    
    this->tagBytes = dataPtr; 
    this->numTagBytes  = varUintFromBytes(dataPtr, &nRead);
    this->numTagBytes += nRead; //plus compresses size of the 'numTagBytes' field itself

}


OsmLightweightWay::OsmLightweightWay( const OsmLightweightWay &other): isDataMapped(false), vertices(NULL), numVertices(0), tagBytes(NULL), numTagBytes(0), id(0), version(0)
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
    uint64_t tagsSize = RawTags(this->tagBytes).getSerializedSize();

    return sizeof(id) +
           sizeof(version) +
           sizeof(numVertices) + 
           numVertices* sizeof(OsmGeoPosition) +
           varUintNumBytes(tagsSize) +
           tagsSize;
}

Envelope OsmLightweightWay::getBounds() const
{
    MUST( numVertices > 0, "cannot create envelope for empty way");
    Envelope aabb( vertices[0].lat, vertices[0].lng);
    
    for (uint64_t i = 0; i < numVertices; i++)
        aabb.add( vertices[i].lat, vertices[i].lng );

    return aabb;
}

static double getArea(const OsmGeoPosition* vertices, uint64_t numVertices)
{
    OsmGeoPosition v0 = vertices[0];
    OsmGeoPosition vn = vertices[numVertices-1];
    
    double area = vn.lat * v0.lng - v0.lat * vn.lng;
    for (uint64_t i = 0; i < numVertices-1; i++)
        area += (vertices[i].lat * (double)vertices[i+1].lng - 
                 vertices[i+1].lat * (double)vertices[i].lng);
        
    return fabs(area/2.0);
}

double OsmLightweightWay::getArea() const
{
    return ::getArea( this->vertices, this->numVertices);  
}

bool OsmLightweightWay::isArea() const
{
    OsmGeoPosition v0 = vertices[0];
    OsmGeoPosition vn = vertices[numVertices-1];
    return v0.lat == vn.lat && v0.lng == vn.lng && numVertices > 3;

}

void OsmLightweightWay::unmap()
{
    if (!isDataMapped)
        return;
        
    OsmGeoPosition* verts = new OsmGeoPosition[this->numVertices];
    for (uint64_t i = 0; i < this->numVertices; i++)
        verts[i] = this->vertices[i];
    
    //don't have to delete() 'vertices', as they are memory-mapped and not manually allocated
    this->vertices = verts;
    
    uint8_t *tagData = new uint8_t[numTagBytes];
    for (uint64_t i = 0; i < this->numTagBytes; i++)
        tagData[i] = this->tagBytes[i];

    this->tagBytes = tagData;

    isDataMapped = false;    
    
    
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

    assert(this->numTagBytes < 1000000);
    MUST( 1 == fwrite( this->tagBytes, this->numTagBytes, 1, dest), "write error");
    /*assert(this->numTagBytes <= 1<<17);
    MUST( 1 == fwrite(&this->numTags,     sizeof(uint16_t), 1, dest), "write error");
    MUST( 1 == fwrite(&this->numTagBytes, sizeof(uint32_t), 1, dest), "write error");
    
    if (this->numTagBytes > 0)
        MUST( 1 == fwrite(this->tagBytes, sizeof(uint8_t) * this->numTagBytes, 1, dest), "read failure");*/
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

    assert(this->numTagBytes <= 1<<20);
   
    memcpy( dest, this->tagBytes, sizeof(uint8_t) * this->numTagBytes);
    dest += this->numTagBytes;

    return dest;
}

static uint64_t tryParse(const char* s, uint64_t defaultValue)
{
    uint64_t res = 0;
    for ( ; *s != '\0'; s++)
    {
        if ( *s < '0' || *s > '9') return defaultValue;
        res = (res * 10) + ( *s - '0');
        
    }
    return res;
}

/* addBoundaryTags() adds tags from boundary relations that refer to a way to said way.
   Since a way may be part of multiple boundary relations (e.g. country boundaries are usually
   also the boundaries of a state inside that country), care must be taken when multiple referring
   relations are about to add the same tags to a way.
   
   Algorithm: the admin_level of the way is determined (if it has one itself, otherwise
              the dummy value 0xFFFF is used). Afterwards,
              the tags from all boundary relations are added as follows: when a relation
              has a numerically smaller (or equal) admin level to that of the way
              (i.e. it is a more important boundary), all of its tags are added to the way
              (even if that means overwriting existing tags) and the way's admin level is
              updated. If the relation has a less important admin level, only those of its
              tags are added to the way that do not overwrite existing tags.

  */

static void addBoundaryTags( TagDictionary &tags, 
                             const std::vector<uint64_t> &referringRelations, 
                             const std::map<uint64_t, TagDictionary> &boundaryTags)
{

    uint64_t adminLevel = tags.count("admin_level") ? tryParse(tags["admin_level"].c_str(), 0xFFFF) : 0xFFFF;
    //cout << "admin level is " << adminLevel << endl;

    for ( uint64_t relId : referringRelations)
    {
        if (!boundaryTags.count(relId))
            continue;
            
        const TagDictionary &relationTags = boundaryTags.at(relId);
        uint64_t relationAdminLevel = relationTags.count("admin_level") ?
                                      tryParse(relationTags.at("admin_level").c_str(), 0xFFFF) :
                                      0xFFFF;
        
        for ( const OsmKeyValuePair &kv : relationTags)
        {
            /* is only the relation type marking the relation as a boundary. This was already
             * processed when creating the boundaryTags map, and need not be considered further*/
            if (kv.first == "type")
                continue;
                
            if ( ! tags.count(kv.first))    //add if not already exists
                tags.insert( kv);
            else if (relationAdminLevel <= adminLevel)  //overwrite only if from a more important boundary
                tags[kv.first] = kv.second;
            
        }

        if (relationAdminLevel < adminLevel)
        {
            adminLevel = relationAdminLevel;
            //cout << "\tadmin level is now " << adminLevel << endl;
            //assert(adminLevel != 1024);
        }
    }
}


void OsmLightweightWay::addTagsFromBoundaryRelations(
    std::vector<uint64_t> referringRelationIds,
    const std::map<uint64_t, TagDictionary> &boundaryRelationTags)
{
    uint64_t i = 0;
    while (i < referringRelationIds.size())
    {
        if (! boundaryRelationTags.count(referringRelationIds[i]))
        {
            referringRelationIds[i] = referringRelationIds.back();
            referringRelationIds.pop_back();
        } else
            i++;
    }
    
    if (referringRelationIds.size() == 0)
        return;

    TagDictionary tagDict = this->getTags().asDictionary();
    addBoundaryTags( tagDict, referringRelationIds, boundaryRelationTags );
    
    Tags tags(tagDict.begin(), tagDict.end());

    /* we are going to modify the way (or rather its tags), so make sure that this change
     * is not reflected in the underlying data store where the way was read from */
    if (isDataMapped)
        this->unmap();

    uint64_t numBytes = 0;
    uint8_t *newTagBytes = RawTags::serialize(tags, &numBytes);
    delete [] this->tagBytes;
    this->tagBytes = newTagBytes;
    this->numTagBytes = numBytes;
}


std::ostream& operator<<(std::ostream &out, const OsmLightweightWay &way)
{
    out << "Way " << way.id << " (";
    for ( int i = 0; i < way.numVertices; i++)
        out << way.vertices[i].id << ", ";

    out << ") - " << way.numTagBytes << " tag bytes";
    return out;

}


//======================================================
