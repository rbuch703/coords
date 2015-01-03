
#include <iostream>
#include <stdio.h>
#include <unistd.h> // for unlink()
#include "osmMappedTypes.h"

using namespace std;

const uint64_t MAX_NODE_SIZE = 50ll * 1000 * 1000;

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
    StorageNode(const char*fileName, const GeoAABB &bounds): 
        bounds(bounds), fileName(fileName), size(0),
        topLeftChild(NULL), topRightChild(NULL), bottomLeftChild(NULL), bottomRightChild(NULL) 
        {
            fData = fopen(fileName, "w+b");
            if (!fData) {perror("fopen"); exit(0);}
             
        }

    void add(OsmLightweightWay &way, const GeoAABB &wayBounds) 
    {
        if (fData)
        {
            uint64_t posBefore = ftell(fData);
            way.serialize(fData);
            assert( ftell(fData) - posBefore == way.size());
            size += way.size();

            if (size > MAX_NODE_SIZE)
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
private:

    void subdivide() {
        cout << "subdividing node '" << fileName << "' ... " << endl;
//        cout.flush();

        int32_t latMid = (((int64_t)bounds.latMax) + bounds.latMin) / 2;    //would overflow in int32_t
        int32_t lngMid = (((int64_t)bounds.lngMax) + bounds.lngMin) / 2;
        assert(fData && !topLeftChild && !topRightChild && !bottomLeftChild && !bottomRightChild);
        topLeftChild = new StorageNode( (fileName+"0").c_str(),  
                            GeoAABB( latMid, bounds.latMax, bounds.lngMin, lngMid));
        
        topRightChild= new StorageNode( (fileName+"1").c_str(),  
                            GeoAABB( latMid, bounds.latMax, lngMid, bounds.lngMax));
                            
        bottomLeftChild = new StorageNode( (fileName+"2").c_str(),  
                            GeoAABB( bounds.latMin, latMid, bounds.lngMin, lngMid));
        
        bottomRightChild= new StorageNode( (fileName+"3").c_str(),  
                            GeoAABB( bounds.latMin, latMid, lngMid, bounds.lngMax));
        
        //topRightChild
        
        rewind(fData);  //reset file read to beginning of file, clear EOF flag
        assert(ftell(fData) == 0);
        char ch;
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
        
        fData = fopen(fileName.c_str(), "wb"); //delete file contents
        fclose(fData);
        fData = NULL;
        //unlink(fileName.c_str());
        
        cout << "done" << endl;
    }
    GeoAABB bounds;
    FILE* fData;
    string fileName;
    uint64_t size;
    StorageNode *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;

};

int main()
{
    LightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");

    StorageNode storage("nodes/node", GeoAABB::getWorldBounds());
    uint64_t numHighways = 0;
    uint64_t pos = 0;
    for (OsmLightweightWay way: wayStore)
    {
        pos += 1;
        if (pos % 10000 == 0)
            cout << (pos / 1000) << "k ways read" << endl;
        
        if (!way.hasKey("highway"))
            continue;
        

        storage.add(way, getBounds(way) );

        //cout << way << endl; 
        numHighways += 1;
    }
 
    cout << numHighways << " roads" << endl;   
    return EXIT_SUCCESS;
}
