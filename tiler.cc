
#include <iostream>
#include <stdio.h>
//#include <unistd.h> // for unlink()
#include <sys/stat.h> //for stat()
#include "osmMappedTypes.h"

using namespace std;

const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
const uint64_t MAX_NODE_SIZE      = 2ll * 1000 * 1000;

static inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b;}
static inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b;}
 
class GeoAABB{
public:
    GeoAABB() {};
    GeoAABB( int32_t lat, int32_t lng): latMin(lat), latMax(lat), lngMin(lng), lngMax(lng) {};
    GeoAABB( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax): 
            latMin(latMin), latMax(latMax), lngMin(lngMin), lngMax(lngMax) {};

    void add (int32_t lat, int32_t lng)
    {
        if (lat > latMax) latMax = lat;
        if (lat < latMin) latMin = lat;
        if (lng > lngMax) lngMax = lng;
        if (lng < lngMin) lngMin = lng;
    }
    

    bool overlapsWith(const GeoAABB &other) const
    {    
        return (max( lngMin, other.lngMin) <= min( lngMax, other.lngMax)) && 
               (max( latMin, other.latMin) <= min( latMax, other.latMax));
    }
    
    int32_t latMin, latMax;
    int32_t lngMin, lngMax;
    
    static const GeoAABB& getWorldBounds() 
    { 
        static GeoAABB world = (GeoAABB){
            .latMin = -900000000, .latMax = 900000000, 
            .lngMin = -1800000000,.lngMin = 1800000000};
        return world;
    }
};

GeoAABB getBounds(const OsmLightweightWay &way)
{
    GeoAABB aabb( way.vertices[0].lat, way.vertices[0].lng);
        
    for (const OsmGeoPosition &pos : way.getVertices())
        aabb.add( pos.lat, pos.lng );

    return aabb;
}

ostream& operator<<(ostream &os, const GeoAABB &aabb)
{
    os << "( lat: " << aabb.latMin << " -> " << aabb.latMax 
       << "; lng: " << aabb.lngMin << " -> " << aabb.lngMax << ")" << endl;
         
    return os;
}

class StorageNode {

public:
    StorageNode(const char*fileName, const GeoAABB &bounds, uint64_t maxNodeSize): 
        bounds(bounds), fileName(fileName), size(0), maxNodeSize(maxNodeSize),
        topLeftChild(NULL), topRightChild(NULL), bottomLeftChild(NULL), bottomRightChild(NULL) 
        {
            fData = fopen(fileName, "wb+"); //open for reading and writing; truncate file
            if (!fData) {perror("fopen"); exit(0);}
             
        }

    void add(OsmLightweightWay &way, const GeoAABB &wayBounds) 
    {
        if (fData)
        {
            assert( !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
            uint64_t posBefore = ftell(fData);
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
//            assert(false && "Not implemented");
        }
        
    }

    void closeFiles()
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
    
    void subdivide(uint64_t maxNodeSize) {
        
        if (fData == NULL)
            fData = fopen(fileName.c_str(), "ab+"); // open for reading and writing; append) keep file contents.
        
        fseek(fData, 0, SEEK_END);  //should be a noop for opening with mode "a"
        uint64_t size = ftell(fData);

//        cout << "testing meta-node " << fileName << " with size " << (size/1000) << "kB for subdivision " << endl;
        
        if (size > maxNodeSize) 
        {
            /* this will also rewind the file before reading,
             * and close the file descriptor afterwards. */
            subdivide();    
        }
        else //no subdivision necessary
            cout << "\tno need to subdivide node " << fileName << endl;

        
        if (fData)
        {
            fclose(fData);
            fData = NULL;
        }
        
        if (topLeftChild)     topLeftChild->    subdivide(maxNodeSize);
        if (topRightChild)    topRightChild->   subdivide(maxNodeSize);
        if (bottomLeftChild)  bottomLeftChild-> subdivide(maxNodeSize);
        if (bottomRightChild) bottomRightChild->subdivide(maxNodeSize);
    }

private:

    void subdivide() {
        cout << "subdividing node '" << fileName << "' ... " << endl;
//        cout.flush();

        int32_t latMid = (((int64_t)bounds.latMax) + bounds.latMin) / 2;    //would overflow in int32_t
        int32_t lngMid = (((int64_t)bounds.lngMax) + bounds.lngMin) / 2;
        assert(fData && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
        GeoAABB aabbTopLeft(            latMid, bounds.latMax, bounds.lngMin, lngMid       );
        GeoAABB aabbTopRight(           latMid, bounds.latMax,        lngMid, bounds.lngMax);
        GeoAABB aabbBottomLeft(  bounds.latMin,        latMid, bounds.lngMin, lngMid       );
        GeoAABB aabbBottomRight( bounds.latMin,        latMid,        lngMid, bounds.lngMax);
        
        topLeftChild =    new StorageNode( (fileName+"0").c_str(), aabbTopLeft,     maxNodeSize);
        topRightChild=    new StorageNode( (fileName+"1").c_str(), aabbTopRight,    maxNodeSize);
        bottomLeftChild = new StorageNode( (fileName+"2").c_str(), aabbBottomLeft,  maxNodeSize);
        bottomRightChild= new StorageNode( (fileName+"3").c_str(), aabbBottomRight, maxNodeSize);
        
        //topRightChild
        
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
        
        fclose(fData);
        /* All contents of this node have been distributed to its four subnodes.
         * So the node itself can now be deleted.
         * Here, we only delete its contents and keep the empty file to act as a marker that
         * subnodes may exist. */
        fData = fopen(fileName.c_str(), "wb"); 
        fclose(fData);
        fData = NULL;
        
    }

private:
    GeoAABB bounds;
    FILE* fData;
    string fileName;
    uint64_t size;
    uint64_t maxNodeSize;
    StorageNode *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;

};

void ensureDirectoryExists(string directory)
{
    struct stat dummy;
    size_t start_pos = 0;
    
    do
    {
        size_t pos = directory.find('/', start_pos);
        string basedir = directory.substr(0, pos);  //works even if no slash is present --> pos == string::npos
        if (0!= stat(basedir.c_str(), &dummy)) //directory does not yet exist
            if (0 != mkdir(basedir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
                { perror(("[ERR] mkdir '" + directory + "'").c_str());}
        if (pos == string::npos) break;
        start_pos = pos+1;
    } while ( true);
}


int main()
{
    LightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");

    ensureDirectoryExists("nodes");
    StorageNode storage("nodes/node", GeoAABB::getWorldBounds(), MAX_META_NODE_SIZE);
    StorageNode storageLod12("nodes/lod12", GeoAABB::getWorldBounds(), MAX_META_NODE_SIZE);

    uint64_t numWays = 0;
    uint64_t numLod12Ways = 0;
    uint64_t pos = 0;

    cout << "stage 1: subdividing dataset to quadtree meta nodes of no more than "
         << (MAX_META_NODE_SIZE/1000000) << "MB." << endl;
    for (OsmLightweightWay way: wayStore)
    {
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        storage.add(way, getBounds(way) );
        numWays += 1;
        
        if (way.hasKey("building")) continue;
        if (way.hasKey("highway"))
        {
            string highway = way.getValue("highway");
            if (highway == "residential" || highway == "service" ||highway == "track" || highway == "unclassified" || highway == "footway" || highway == "path" || highway == "living_street" || highway == "crossing" || highway == "turning_circle")
                continue;
        }
        
        if (way.hasKey("natural"))
        {
            string natural = way.getValue("natural");
            if (natural == "tree") continue;
        }
        
        /*if (way.hasKey("boundary"))
        {
            if (way.getValue("boundary") != "administrative")
                continue;
            
            if (!way.hasKey("admin_level")
        }*/
        
        storageLod12.add(way, getBounds(way));
        numLod12Ways += 1;
    }
    cout << "stats: data set contains " << (numWays/1000) << "k ways." << endl;   
    cout << "stats: data set contains " << (numLod12Ways/1000) << "k ways at LOD 12." << endl;   

    cout << "stage 2: subdividing meta nodes to individual nodes of no more than "
         << (MAX_NODE_SIZE/1000000) << "MB." << endl;
//    exit(0);
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
    storage.closeFiles();
    storage.subdivide(MAX_NODE_SIZE);
 
    storageLod12.closeFiles();
    storageLod12.subdivide(MAX_NODE_SIZE);
    cout << "done." << endl;
    return EXIT_SUCCESS;
}
