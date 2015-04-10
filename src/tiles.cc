
#include "tiles.h"
#include <unistd.h> //for ftruncate()
#include <sys/mman.h>
using std::cout;
using std::endl;

static inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b;}
static inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b;}

static void deleteContentsAndClose(FILE* &file)
{
    MUST(0 == ftruncate(fileno(file), 0), "cannot truncate file");
    fclose(file);
    file = NULL;
}

GeoAABB::GeoAABB( int32_t lat, int32_t lng): latMin(lat), latMax(lat), lngMin(lng), lngMax(lng) {};

GeoAABB::GeoAABB( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax): 
    latMin(latMin), latMax(latMax), lngMin(lngMin), lngMax(lngMax) {};


void GeoAABB::add (int32_t lat, int32_t lng)
{
    if (lat > latMax) latMax = lat;
    if (lat < latMin) latMin = lat;
    if (lng > lngMax) lngMax = lng;
    if (lng < lngMin) lngMin = lng;
}


bool GeoAABB::overlapsWith(const GeoAABB &other) const
{    
    return (max( lngMin, other.lngMin) <= min( lngMax, other.lngMax)) && 
           (max( latMin, other.latMin) <= min( latMax, other.latMax));
}

const GeoAABB& GeoAABB::getWorldBounds() 
{ 
    static GeoAABB world = (GeoAABB){
        .latMin = -900000000, .latMax = 900000000, 
        .lngMin = -1800000000,.lngMin = 1800000000};
    return world;
}

GeoAABB getBounds(const OsmLightweightWay &way)
{
    GeoAABB aabb( way.vertices[0].lat, way.vertices[0].lng);
        
    for (const OsmGeoPosition &pos : way.getVertices())
        aabb.add( pos.lat, pos.lng );

    return aabb;
}

std::ostream& operator<<(std::ostream &os, const GeoAABB &aabb)
{
    os << "( lat: " << aabb.latMin << " -> " << aabb.latMax 
       << "; lng: " << aabb.lngMin << " -> " << aabb.lngMax << ")";
         
    return os;
}


// ============== MEMORY-BACKED STORAGE ==============

MemoryBackedTile::MemoryBackedTile(const char*fileName, const GeoAABB &bounds, uint64_t maxNodeSize):
    topLeftChild(NULL), topRightChild(NULL), bottomLeftChild(NULL), bottomRightChild(NULL),
    bounds(bounds), fileName(fileName), size(0), maxNodeSize(maxNodeSize)
{

}

MemoryBackedTile::~MemoryBackedTile() 
{
    if (topLeftChild) delete topLeftChild;
    if (topRightChild) delete topRightChild;
    if (bottomLeftChild) delete bottomLeftChild;
    if (bottomRightChild) delete bottomRightChild;
    
}

void MemoryBackedTile::add(const OsmLightweightWay &way, const GeoAABB &wayBounds)
{
    assert( (!topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild) ||
            ( topLeftChild &&  topRightChild &&  bottomLeftChild &&  bottomRightChild));

    bool hasChildren = topLeftChild;
    assert(!hasChildren || ways.size() == 0);

    if (!hasChildren)
    {
        //cout << "adding way of size " << way.size() << "to mem-backed node of size " << size << endl;
        ways.push_back(way);
        size += way.size();
        
        if (size > maxNodeSize)
            subdivide();
    } else 
    {
        assert( topLeftChild && topRightChild && bottomLeftChild && bottomRightChild);
        if (wayBounds.overlapsWith(topLeftChild->bounds)) topLeftChild->add(way, wayBounds);
        if (wayBounds.overlapsWith(topRightChild->bounds)) topRightChild->add(way, wayBounds);
        if (wayBounds.overlapsWith(bottomLeftChild->bounds)) bottomLeftChild->add(way, wayBounds);
        if (wayBounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(way, wayBounds);
//            assert(false && "Not implemented");
    }
}

void MemoryBackedTile::writeToDiskRecursive(bool includeSelf = true)
{
    if (topLeftChild)     topLeftChild->    writeToDiskRecursive(true);
    if (topRightChild)    topRightChild->   writeToDiskRecursive(true);
    if (bottomLeftChild)  bottomLeftChild-> writeToDiskRecursive(true);
    if (bottomRightChild) bottomRightChild->writeToDiskRecursive(true);

    if (includeSelf)
    {
        /* Open and close the file in any case to create at least an empty node file.
         * This empty file acts as a marker that there may be child nodes*/
        FILE* fData = fopen(fileName.c_str(), "wb");

        for (const OsmLightweightWay &way : ways)
            way.serialize(fData);

        fclose(fData);
    }    
}
/*
virtual void subdivideAndRelease(uint64_t maxNodeSize) {
    
    subdivide();    
    
    if (topLeftChild)     topLeftChild->    subdivideAndRelease(maxNodeSize);
    if (topRightChild)    topRightChild->   subdivideAndRelease(maxNodeSize);
    if (bottomLeftChild)  bottomLeftChild-> subdivideAndRelease(maxNodeSize);
    if (bottomRightChild) bottomRightChild->subdivideAndRelease(maxNodeSize);
}*/

void MemoryBackedTile::subdivide() {
    //cout << "subdividing mem-backed node '" << fileName << "' ... " << endl;
//        cout.flush();

    int32_t latMid = (((int64_t)bounds.latMax) + bounds.latMin) / 2;    //would overflow in int32_t
    int32_t lngMid = (((int64_t)bounds.lngMax) + bounds.lngMin) / 2;
    assert(!ways.empty() && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
    GeoAABB aabbTopLeft(            latMid, bounds.latMax, bounds.lngMin, lngMid       );
    GeoAABB aabbTopRight(           latMid, bounds.latMax,        lngMid, bounds.lngMax);
    GeoAABB aabbBottomLeft(  bounds.latMin,        latMid, bounds.lngMin, lngMid       );
    GeoAABB aabbBottomRight( bounds.latMin,        latMid,        lngMid, bounds.lngMax);
    
    topLeftChild =    new MemoryBackedTile( (fileName+"0").c_str(), aabbTopLeft,     maxNodeSize);
    topRightChild=    new MemoryBackedTile( (fileName+"1").c_str(), aabbTopRight,    maxNodeSize);
    bottomLeftChild = new MemoryBackedTile( (fileName+"2").c_str(), aabbBottomLeft,  maxNodeSize);
    bottomRightChild= new MemoryBackedTile( (fileName+"3").c_str(), aabbBottomRight, maxNodeSize);
    
    while (!ways.empty())
    {
        const OsmLightweightWay &way = ways.front();

        GeoAABB wayBounds = getBounds(way);

        assert ( wayBounds.overlapsWith(topLeftChild    ->bounds) ||
                 wayBounds.overlapsWith(topRightChild   ->bounds) ||
                 wayBounds.overlapsWith(bottomLeftChild ->bounds) ||
                 wayBounds.overlapsWith(bottomRightChild->bounds) );

        if (wayBounds.overlapsWith(topLeftChild    ->bounds)) topLeftChild->add(    way, wayBounds);
        if (wayBounds.overlapsWith(topRightChild   ->bounds)) topRightChild->add(   way, wayBounds);
        if (wayBounds.overlapsWith(bottomLeftChild ->bounds)) bottomLeftChild->add( way, wayBounds);
        if (wayBounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(way, wayBounds);
        
        ways.pop_front();
    }
    
    size = 0;
}


// ================== FILE-BACKED TILE =================

FileBackedTile::FileBackedTile(const char*fileName, const GeoAABB &bounds, uint64_t maxNodeSize): 
        bounds(bounds), fileName(fileName), size(0), maxNodeSize(maxNodeSize),
        topLeftChild(NULL), topRightChild(NULL), bottomLeftChild(NULL), bottomRightChild(NULL)
        {
            fData = fopen(fileName, "wb+"); //open for reading and writing; truncate file
            if (!fData) {perror("fopen"); abort();}
        }
        
FileBackedTile::~FileBackedTile() 
{
    if (topLeftChild) delete topLeftChild;
    if (topRightChild) delete topRightChild;
    if (bottomLeftChild) delete bottomLeftChild;
    if (bottomRightChild) delete bottomRightChild;
    
    topLeftChild = topRightChild = bottomLeftChild = bottomRightChild = NULL;

    if (fData) fclose(fData);
    fData = NULL;
}

void FileBackedTile::add(OsmLightweightWay &way, const GeoAABB &wayBounds)
{
    if (fData)
    {
        assert( !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
#ifndef NDEBUG
        uint64_t posBefore = ftell(fData);
#endif
        way.serialize(fData);
        assert( ftell(fData) - posBefore == way.size());
        size += way.size();

        if (size > maxNodeSize)
            subdivide();
    } else 
    {
        assert( topLeftChild && topRightChild && bottomLeftChild && bottomRightChild);
        if (wayBounds.overlapsWith(topLeftChild->bounds)) topLeftChild->add(way, wayBounds);
        if (wayBounds.overlapsWith(topRightChild->bounds)) topRightChild->add(way, wayBounds);

        if (wayBounds.overlapsWith(bottomLeftChild->bounds)) bottomLeftChild->add(way, wayBounds);
        if (wayBounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(way, wayBounds);
    }
    
}

void FileBackedTile::releaseMemoryResources()
{
    if (fData)
    {
        fclose(fData);
        fData = NULL;
    }
    
    if (topLeftChild)     topLeftChild->    releaseMemoryResources();
    if (topRightChild)    topRightChild->   releaseMemoryResources();
    if (bottomLeftChild)  bottomLeftChild-> releaseMemoryResources();
    if (bottomRightChild) bottomRightChild->releaseMemoryResources();
}

void FileBackedTile::subdivide(uint64_t maxSubdivisionNodeSize, bool useMemoryBackedStorage) {
    //cout << "size of node " << this << " is " << size 
    this->maxNodeSize = maxSubdivisionNodeSize;
    if (size <= maxSubdivisionNodeSize)
    {
        /* is either 
            1. an empty leaf
            2. an empty inner node
            3. an inner node small enough not to have to be subdivided
         */
        if (fData)
            fclose(fData);

        if (topLeftChild)     topLeftChild->    subdivide(maxSubdivisionNodeSize,useMemoryBackedStorage);
        if (topRightChild)    topRightChild->   subdivide(maxSubdivisionNodeSize,useMemoryBackedStorage);
        if (bottomLeftChild)  bottomLeftChild-> subdivide(maxSubdivisionNodeSize,useMemoryBackedStorage);
        if (bottomRightChild) bottomRightChild->subdivide(maxSubdivisionNodeSize,useMemoryBackedStorage);
            
        return;
    }

    cout << "subdividing node '" << fileName << "' ... (size " << (size/1000000) << "MB)" << endl;

    assert(!topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
    assert( size > maxSubdivisionNodeSize);
    assert( size > 0);

    if (fData == NULL)
        fData = fopen(fileName.c_str(), "ab+"); // open for reading and writing; "append" keeps file contents.

    fseek(fData, 0, SEEK_END);  //should be a noop for opening with mode "a"
    uint64_t sizeFromFile = ftell(fData);
    MUST(sizeFromFile == size, "storage file corrupted.");
    rewind(fData);
    if (useMemoryBackedStorage)
    {
        MemoryBackedTile *shadowNode = new MemoryBackedTile(fileName.c_str(), bounds, maxSubdivisionNodeSize);

        uint8_t* data = (uint8_t*)mmap(NULL, size, PROT_READ, MAP_SHARED, fileno(fData), 0);
        assert(data != MAP_FAILED);
        uint64_t pos = 0;
        while (pos < size)
        {
            OsmLightweightWay way(&data[pos]);
            pos += way.size();
            shadowNode->add(way, getBounds(way));
        }
        /*int ch;
        while ( (ch = fgetc(fData)) != EOF)
        {
            ungetc( ch, fData);

            OsmLightweightWay way(fData);
            shadowNode -> add(way, getBounds(way));
        }*/

        assert(shadowNode->getSize() == 0);
        shadowNode->writeToDiskRecursive(false);
        delete shadowNode;
        /* All contents of this node have been distributed to its four subnodes.
         * So the node itself can now be deleted.
         * Here, we only delete its contents and keep the empty file to act as a marker that
         * subnodes may exist. */
        

        /* must be executed only after writeToDiskRecursive(), because the OsmLightweightWays
         * of child nodes hold pointers to this memory mapping */
        int res = munmap(data, size);
        MUST(res == 0, "cannot unmap file");
        deleteContentsAndClose(fData);
        size = 0;

    } else
    {
        this->subdivide(); //also deletes file contents and frees file pointer
        this->releaseMemoryResources();
    }
     
}

void FileBackedTile::subdivide() 
{
    cout << "subdividing node '" << fileName << "' ... " << endl;

    int32_t latMid = (((int64_t)bounds.latMax) + bounds.latMin) / 2;    //would overflow in int32_t
    int32_t lngMid = (((int64_t)bounds.lngMax) + bounds.lngMin) / 2;
    assert(fData && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
    GeoAABB aabbTopLeft(            latMid, bounds.latMax, bounds.lngMin, lngMid       );
    GeoAABB aabbTopRight(           latMid, bounds.latMax,        lngMid, bounds.lngMax);
    GeoAABB aabbBottomLeft(  bounds.latMin,        latMid, bounds.lngMin, lngMid       );
    GeoAABB aabbBottomRight( bounds.latMin,        latMid,        lngMid, bounds.lngMax);

    topLeftChild =    new FileBackedTile( (fileName+"0").c_str(), aabbTopLeft,     maxNodeSize);
    topRightChild=    new FileBackedTile( (fileName+"1").c_str(), aabbTopRight,    maxNodeSize);
    bottomLeftChild = new FileBackedTile( (fileName+"2").c_str(), aabbBottomLeft,  maxNodeSize);
    bottomRightChild= new FileBackedTile( (fileName+"3").c_str(), aabbBottomRight, maxNodeSize);
    
    rewind(fData);  //reset file read to beginning of file, clear EOF flag
    assert(ftell(fData) == 0);
    int ch;
    while ( (ch = fgetc(fData)) != EOF)
    {
        ungetc( ch, fData);

        OsmLightweightWay way(fData);
        GeoAABB wayBounds = getBounds(way);

        assert ( wayBounds.overlapsWith(topLeftChild    ->bounds) ||
                 wayBounds.overlapsWith(topRightChild   ->bounds) ||
                 wayBounds.overlapsWith(bottomLeftChild ->bounds) ||
                 wayBounds.overlapsWith(bottomRightChild->bounds) );

        if (wayBounds.overlapsWith(topLeftChild    ->bounds)) topLeftChild->add(    way, wayBounds);
        if (wayBounds.overlapsWith(topRightChild   ->bounds)) topRightChild->add(   way, wayBounds);
        if (wayBounds.overlapsWith(bottomLeftChild ->bounds)) bottomLeftChild->add( way, wayBounds);
        if (wayBounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(way, wayBounds);
        
    }
    
    /* All contents of this node have been distributed to its four subnodes.
     * So the node itself can now be deleted.
     * Here, we only delete its contents and keep the empty file to act as a marker that
     * subnodes may exist. */
    deleteContentsAndClose(fData);
    size = 0;
}
