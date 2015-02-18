
#include <iostream>
#include <stdio.h>
//#include <unistd.h> // for unlink()
#include <sys/stat.h> //for stat()

#include <vector>
#include <list>

#include "osmMappedTypes.h"
#include "tiles.h"

using namespace std;

const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
const uint64_t MAX_NODE_SIZE      = 10ll * 1000 * 1000;

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

double squaredDistanceToLine(const OsmGeoPosition &P, const OsmGeoPosition &LineA, const OsmGeoPosition &LineB)
{
    assert( LineB!= LineA);
    double num = (LineB.lat - (double)LineA.lat )*(LineA.lng - (double)P.lng );
    num -=       (LineB.lng - (double)LineA.lng )*(LineA.lat - (double)P.lat );
    if (num < 0) num = -num; //distance is an absolute value
    
    double dLat = LineB.lat - (double)LineA.lat;
    double dLng = LineB.lng - (double)LineA.lng;
    
    double denom_sq = dLat*dLat + dLng*dLng;//(B.x-A.x)*(B.x-A.x) + (B.y-A.y)*(B.y-A.y);
    assert(denom_sq != 0);
    return (num*num) / denom_sq;
}


void simplifySection(list<OsmGeoPosition> &container, list<OsmGeoPosition>::iterator segment_first, list<OsmGeoPosition>::iterator segment_last, uint64_t allowedDeviation)
{
    list<OsmGeoPosition>::iterator it_max;
    
    double max_dist_sq = 0;
    OsmGeoPosition LineA = *segment_first;
    OsmGeoPosition LineB = *segment_last;
    list<OsmGeoPosition>::iterator it  = segment_first;
    // make sure that 'it' starts from segment_first+1; segment_first must never be deleted
    for (it++; it != segment_last; it++)
    {
        double dist_sq;
        if (LineA == LineB)
        {
            double dLat =  it->lat - (double)LineA.lat;
            double dLng =  it->lng - (double)LineA.lng;
            dist_sq = dLat*dLat + dLng*dLng;
        } else
            dist_sq = squaredDistanceToLine(*it, LineA, LineB);
        if (dist_sq > max_dist_sq) { it_max = it; max_dist_sq = dist_sq;}
    }
    if (max_dist_sq == 0) return;
    
    if (max_dist_sq < allowedDeviation*allowedDeviation) 
    {
        // no point further than 'allowedDeviation' from line A-B --> can delete all points in between
        segment_first++;
        // erase range includes first iterator but excludes second one
        container.erase( segment_first, segment_last);
        //assert( m_vertices.front() == m_vertices.back());
    } else  //use point with maximum deviation as additional necessary point in the simplified polygon, recurse
    {
        simplifySection(container, segment_first, it_max, allowedDeviation);
        simplifySection(container, it_max, segment_last, allowedDeviation);
    }
}


bool simplifyArea(vector<OsmGeoPosition> &area, double allowedDeviation)
{
    //bool clockwise = isClockwise();
    assert( area.front() == area.back() && "Not a Polygon");
    /** simplifySection() requires the Polygon to consist of at least two vertices,
        so we need to test the number of vertices here. Also, a segment cannot be a polygon
        if its vertex count is less than four (since the polygon of smallest order is a triangle,
        and first and last vertex are identical). Since simplification cannot add vertices,
        we can safely terminate the simplification here, if we have less than four vertices. */
    if ( area.size() < 4) { area.clear(); return false; }


    list<OsmGeoPosition> vertices( area.begin(), area.end() );
    
    list<OsmGeoPosition>::iterator last = vertices.end();
    last--;
    simplifySection( vertices, vertices.begin(), last, allowedDeviation);

    area = vector<OsmGeoPosition>(vertices.begin(), vertices.end() );

    //canonicalize();
    
    /** Need three vertices to form an area; four since first and last are identical
        If the polygon was simplified to degenerate to a line or even a single point,
        This means that the whole polygon would not be visible given the current 
        allowedDeviation. It may thus be omitted completely */
    if (area.size() < 4) { return false; }

    assert( area.front() == area.back());
    //if ( isClockwise() != clockwise) this->reverse();
    return true;
}

/*
AABoundingBox VertexChain::getBoundingBox() const
{
    AABoundingBox box(m_vertices.front());
    for (vector<Vertex>::const_iterator it = m_vertices.begin(); it!= m_vertices.end(); it++)
        box += *it;
        
    return box;
}*/

bool simplifyStroke(vector<OsmGeoPosition> &segment, double allowedDeviation)
{
    
    //TODO: check whether the whole VertexChain would be simplified to a single line (start-end)
    //      if so, do not allocate the list, but set the result right away

    list<OsmGeoPosition> vertices( segment.begin(), segment.end());
    
    list<OsmGeoPosition>::iterator last = vertices.end();
    last--;
    simplifySection( vertices, vertices.begin(), last, allowedDeviation);
    
    /*
    if (vertices.size() == 2)
    {
        BigInt dx = vertices.front().get_x() - vertices.back().get_x();
        BigInt dy = vertices.front().get_y() - vertices.back().get_y();
        dist_sq = asDouble (dx*dx+dy*dy);
        if (dist_sq < allowedDeviation*allowedDeviation) 
            vertices.pop_back();
    }*/
    
    segment = vector<OsmGeoPosition>( vertices.begin(), vertices.end() );    
    //canonicalize();
    if (segment.size() == 2 && segment[0] == segment[1])
        return false;
    
    return segment.size() > 1;
}


static uint64_t frequencies[] = {0,0,0,0,0,0};

OsmLightweightWay getLod12Version(OsmLightweightWay &wayIn)
{
    //create a non-mapped copy. This is necessary because the mapped OsmLightweightWay 
    //cannot easily be changed in size
    OSMWay way =wayIn.toOsmWay();

    int keep =  0;
    
    if( way.hasKey("landuse"))
    {
            //we are only interested in "natural" *polygons*
        if (way.refs.front() == way.refs.back())
            keep = 1;

    }
        
    if (way.hasKey("natural"))
    {
        //we are only interested in "natural" *polygons*
        if (way.refs.front() == way.refs.back())
            keep = 2;
    }
        
     if (way.hasKey("highway"))
        if (way["highway"] == "primary" || way["highway"] == "motorway" || way["highway"] == "trunk")
            keep = 3;

   
    if (way.hasKey("boundary") && way["boundary"] == "administrative" && way.hasKey("admin_level"))
        if (way["admin_level"] == "6" || way["admin_level"] == "4" || way["admin_level"] == "2")
            keep = 4;

     if (way.hasKey("railway"))
            keep = 5;


    GeoAABB bounds = getBounds(wayIn);
    uint64_t solidAngle = abs(bounds.latMax - bounds.latMin);
    solidAngle *= abs(bounds.lngMax - bounds.lngMin);
    /* WARNING!!!: FIXME: these are very rough computations based on a lat/lng grid. More accurate computations
     *                    would need to take into account the coordinates after mercator projection */
    double solidAngleDeg = solidAngle / (10000000.0 * 10000000.0);
    // at zoom level 12: a tile is on average 0.0879° wide and 0.0415° high, and has 256x256 pixels
    double solidAnglePixel = (0.0879/256) *(0.0415/256);

    static const double degsPerPixel = ((0.0879 + 0.0415)/2.0)/256.0;
    
    if (keep == 1 || keep == 2) //polygons
    {
        if (solidAngleDeg < solidAnglePixel*64)
            keep = 0;
        else
            
        {
            if ( way.refs.front() != way.refs.back())
            {
                cout << "non-closed polygon " << way << endl;
                for ( const OSMKeyValuePair &kv: way.tags)
                    cout << "\t" << kv.first << " -> " << kv.second << endl;
            }
            // average of horizontal and vertical degrees per tile; 
            if (!simplifyArea( way.refs, degsPerPixel * 4 * 10000000)) // 4 px allowed deviation; 
                keep = 0;
        }
    }
            
    if (keep == 3 || keep == 4 || keep == 5) //line geometry
    {
        double dLat = abs(bounds.latMax - bounds.latMin)/10000000.0;
        double dLng = abs(bounds.lngMax - bounds.lngMin)/10000000.0;
        //cout << dLat << ", " << dLng << endl;
        if (dLat < (0.0415/256*4) && dLng < (0.0879/256*4))
        {
            //cout << "#" << endl;
            keep = 0;
        }
        else
            if (!simplifyStroke( way.refs, degsPerPixel * 4 * 10000000)) // 4 px allowed deviation; 
                keep = 0;

    }
    
    frequencies[keep] += 1;
    
    if (!keep)
        way.id = 0;
    
    return way;
}

int main()
{
    LightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");

    ensureDirectoryExists("nodes");
    FileBackedTile storage("nodes/node", GeoAABB::getWorldBounds(), MAX_META_NODE_SIZE);
    //FileBackedTile storageLod12("nodes/lod12", GeoAABB::getWorldBounds(), MAX_META_NODE_SIZE);

    uint64_t numWays = 0;
    uint64_t numLod12Ways = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;
    uint64_t numTagBytes = 0;
    cout << "stage 1: subdividing dataset to quadtree meta nodes of no more than "
         << (MAX_META_NODE_SIZE/1000000) << "MB." << endl;
    for (OsmLightweightWay way: wayStore)
    {
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        storage.add(way, getBounds(way) );
        numWays += 1;

        way = getLod12Version(way);
        if (!way.id)    //has been invalidated by getLod12Version() --> should not be stored
            continue;
            
        numVertices += way.numVertices;
            
        for (OSMKeyValuePair kv: way.getTags())
        {
            //cout << kv.first << " = " << kv.second << endl;
            numTagBytes += 2 + kv.first.size() + kv.second.size();
        }
            
        //storageLod12.add(way, getBounds(way));
        numLod12Ways += 1;
    }
    cout << "stage 2: subdividing meta nodes to individual nodes of no more than "
         << (MAX_NODE_SIZE/1000000) << "MB." << endl;
//    exit(0);
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on Linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
    storage.releaseMemoryResources();
    storage.subdivide(MAX_NODE_SIZE, true);
 
    //storageLod12.releaseMemoryResources();
    //storageLod12.subdivideMemoryBacked(MAX_NODE_SIZE);
    cout << "done." << endl;

    cout << "stats: data set contains " << (numWays     / 1000) << "k ways." << endl;   
    cout << "stats: data set contains " << (numLod12Ways/ 1000) << "k ways with " << (numVertices/1000) << "k vertices and " << (numTagBytes / 1000) << "kB tags at LOD 12." << endl;   
    cout << "stats: skipped: "  << frequencies[0] << 
                 ", landuse: "  << frequencies[1] << 
                 ", natural: "  << frequencies[2] << 
                 ", highway: "  << frequencies[3] << 
                 ", boundary: " << frequencies[4] << endl;

    return EXIT_SUCCESS;
}
