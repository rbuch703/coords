
#include "tiles.h"
#include "geom/envelope.h"
#include <unistd.h> //for ftruncate()
#include <sys/mman.h>
using std::cout;
using std::endl;

static void deleteContentsAndClose(FILE* &file)
{
    MUST(0 == ftruncate(fileno(file), 0), "cannot truncate file");
    fclose(file);
    file = NULL;
}

// ================== FILE-BACKED TILE =================

FileBackedTile::FileBackedTile(const char*fileName, const Envelope &bounds, uint64_t maxNodeSize): 
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

void FileBackedTile::add(OsmLightweightWay &way, const Envelope &wayBounds)
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

    this->subdivide(); //also deletes file contents and frees file pointer
    this->releaseMemoryResources();
    
}

void FileBackedTile::subdivide() 
{
    cout << "subdividing node '" << fileName << "' ... " << endl;

    int32_t latMid = (((int64_t)bounds.latMax) + bounds.latMin) / 2;    //would overflow in int32_t
    int32_t lngMid = (((int64_t)bounds.lngMax) + bounds.lngMin) / 2;
    assert(fData && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
    Envelope aabbTopLeft(            latMid, bounds.latMax, bounds.lngMin, lngMid       );
    Envelope aabbTopRight(           latMid, bounds.latMax,        lngMid, bounds.lngMax);
    Envelope aabbBottomLeft(  bounds.latMin,        latMid, bounds.lngMin, lngMid       );
    Envelope aabbBottomRight( bounds.latMin,        latMid,        lngMid, bounds.lngMax);

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
        Envelope wayBounds = way.getBounds();

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
