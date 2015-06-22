
#include "tiles.h"
#include "geom/envelope.h"
#include "geom/geomSerializers.h"

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

FileBackedTile::FileBackedTile(const std::string &fileName, const Envelope &bounds, uint64_t maxNodeSize) : FileBackedTile(fileName.c_str(), bounds, maxNodeSize) 
{
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

void FileBackedTile::add(const OsmWay &way, const Envelope &wayBounds, bool asPolygon)
{
    if (fData)
    {
        assert( !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
        serializeWay(way, asPolygon).serialize(fData);
        this->size = ftell(fData);

        if ( this->size > maxNodeSize)
            subdivide();
    } else 
    {
        assert( topLeftChild && topRightChild && bottomLeftChild && bottomRightChild);
        if (wayBounds.overlapsWith(topLeftChild->bounds)) 
            topLeftChild->add(way, wayBounds, asPolygon);
        if (wayBounds.overlapsWith(topRightChild->bounds)) 
            topRightChild->add(way, wayBounds, asPolygon);

        if (wayBounds.overlapsWith(bottomLeftChild->bounds)) 
            bottomLeftChild->add(way,  wayBounds, asPolygon);
        if (wayBounds.overlapsWith(bottomRightChild->bounds)) 
            bottomRightChild->add(way,  wayBounds, asPolygon);
    }
    
}

void FileBackedTile::add(const GenericGeometry &geom, const Envelope &bounds)
{
    if (fData)
    {
        assert( !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
/*#ifndef NDEBUG
        uint64_t posBefore = ftell(fData);
#endif*/
        
        MUST( fwrite(&geom.numBytes, sizeof(geom.numBytes), 1, fData) == 1, "write error");

        MUST( fwrite(geom.bytes, geom.numBytes, 1, fData) == 1, "write error");
        //serializeWay(way, false, fData);
        //way.serialize(fData);
        //assert( ftell(fData) - posBefore == way.size());
        size = ftell(fData);

        if ( this->size > maxNodeSize)
            subdivide();
    } else 
    {
        assert( topLeftChild && topRightChild && bottomLeftChild && bottomRightChild);
        if (bounds.overlapsWith(topLeftChild->bounds)) topLeftChild->add(geom, bounds);
        if (bounds.overlapsWith(topRightChild->bounds)) topRightChild->add(geom, bounds);
        if (bounds.overlapsWith(bottomLeftChild->bounds)) bottomLeftChild->add(geom, bounds);
        if (bounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(geom, bounds);
    }
}


void FileBackedTile::closeFiles()
{
    if (fData)
    {
        fclose(fData);
        fData = NULL;
    }
    
    if (topLeftChild)     topLeftChild->    closeFiles();
    if (topRightChild)    topRightChild->   closeFiles();
    if (bottomLeftChild)  bottomLeftChild-> closeFiles();
    if (bottomRightChild) bottomRightChild->closeFiles();
}

void FileBackedTile::subdivide(uint64_t maxSubdivisionNodeSize) {
    //cout << "size of node " << this << " is " << size << endl;
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

        if (topLeftChild)     topLeftChild->    subdivide(maxSubdivisionNodeSize);
        if (topRightChild)    topRightChild->   subdivide(maxSubdivisionNodeSize);
        if (bottomLeftChild)  bottomLeftChild-> subdivide(maxSubdivisionNodeSize);
        if (bottomRightChild) bottomRightChild->subdivide(maxSubdivisionNodeSize);
            
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
    this->closeFiles();
    
}

void FileBackedTile::subdivide() 
{
    cout << "subdividing node '" << fileName << "' ... " << endl;

    int32_t xMid = (((int64_t)bounds.xMax) + bounds.xMin) / 2;    //would overflow in int32_t
    int32_t yMid = (((int64_t)bounds.yMax) + bounds.yMin) / 2;
    assert(fData && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
    Envelope aabbTopLeft(     bounds.xMin,        xMid, yMid,        bounds.yMax);
    Envelope aabbTopRight(           xMid, bounds.xMax, yMid,        bounds.yMax);
    Envelope aabbBottomLeft(  bounds.xMin,        xMid, bounds.yMin,        yMid);
    Envelope aabbBottomRight(        xMid, bounds.xMax, bounds.yMin,        yMid);

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
        
        GenericGeometry geom(fData);
        //OsmLightweightWay way(fData);
        static uint64_t cntr = 0;
        cntr++;
        
        Envelope bounds = geom.getBounds();

        assert ( bounds.overlapsWith(topLeftChild    ->bounds) ||
                 bounds.overlapsWith(topRightChild   ->bounds) ||
                 bounds.overlapsWith(bottomLeftChild ->bounds) ||
                 bounds.overlapsWith(bottomRightChild->bounds) );

        if (bounds.overlapsWith(topLeftChild    ->bounds)) topLeftChild->add(    geom, bounds);
        if (bounds.overlapsWith(topRightChild   ->bounds)) topRightChild->add(   geom, bounds);
        if (bounds.overlapsWith(bottomLeftChild ->bounds)) bottomLeftChild->add( geom, bounds);
        if (bounds.overlapsWith(bottomRightChild->bounds)) bottomRightChild->add(geom, bounds);
        
    }
    
    /* All contents of this node have been distributed to its four subnodes.
     * So the node itself can now be deleted.
     * Here, we only delete its contents and keep the empty file to act as a marker that
     * subnodes may exist. */
    deleteContentsAndClose(fData);
    size = 0;
}
