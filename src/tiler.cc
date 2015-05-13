
#include <stdio.h>
#include <getopt.h> //for getopt_long()
//#include <unistd.h> // for unlink()
#include <sys/stat.h> //for stat()
#include <math.h>

#include <vector>
#include <list>
#include <iostream>
#include <set>

#include "tiles.h"
#include "osm/osmTypes.h"
#include "osm/osmMappedTypes.h"
#include "geom/envelope.h"
#include "containers/osmWayStore.h"

using namespace std;

const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
const uint64_t MAX_NODE_SIZE      = 10ll * 1000 * 1000;

std::string storageDirectory;
std::string destinationDirectory;
std::string usageLine;

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

OsmWay getEditableCopy(OsmLightweightWay &src) {
    
    map<string, string> tags = src.getTagSet();
    return OsmWay( src.id, 
                   src.version, 
                   vector<OsmGeoPosition>( src.vertices, src.vertices + src.numVertices),
                   vector<OsmKeyValuePair>( tags.begin(), tags.end()));
}


OsmLightweightWay toLightweightWay( const OsmWay &other)
{
    OsmLightweightWay res;
    res.id = other.id;
    res.version = other.version;
    res.isDataMapped = false;
    res.numVertices = other.refs.size();
    res.vertices = new OsmGeoPosition[res.numVertices];
    for (int i = 0; i < res.numVertices; i++)
        res.vertices[i] = other.refs[i];
        
    res.numTags = other.tags.size();
    res.numTagBytes = 0;
    for (uint64_t i = 0; i < other.tags.size(); i++)
    {
        res.numTagBytes += other.tags[i].first.size() + 1;    //  key string length including termination
        res.numTagBytes += other.tags[i].second.size() + 1;   //value string length including termination
    }
    
    res.tagBytes = new uint8_t[res.numTagBytes];
    char *pos = (char*)res.tagBytes;
    for (uint64_t i = 0; i < other.tags.size(); i++)
    {
        strcpy(pos, other.tags[i].first.c_str());
        pos += strlen( pos ) + 1;
        
        strcpy(pos, other.tags[i].second.c_str());
        pos += strlen( pos ) + 1;
    }
    assert( (uint8_t*)pos == res.tagBytes + res.numTagBytes);
    return res;    
}


static uint64_t frequencies[] = {0,0,0,0,0,0};
#if 0
OsmLightweightWay getLod12Version(OsmLightweightWay &wayIn)
{
    //create a non-mapped copy. This is necessary because the mapped OsmLightweightWay 
    //cannot easily be changed in size
    OsmWay way = getEditableCopy(wayIn);

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


    Envelope bounds = wayIn.getBounds();
    uint64_t solidAngle = abs(bounds.latMax - bounds.latMin);
    solidAngle *= abs(bounds.lngMax - bounds.lngMin);
    /* WARNING!!!: FIXME: these are very rough computations based on a lat/lng grid (plate
     * carree). More accurate computations would need to take into account the coordinates 
     * in web mercator projection (the projection the data will be rendered in) */
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
                for ( const OsmKeyValuePair &kv: way.tags)
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
    
    return toLightweightWay(way);
}
#endif

int parseArguments(int argc, char** argv)
{
    static const struct option long_options[] =
    {
        {"dest",  required_argument, NULL, 'd'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "d:", long_options, &opt_idx)))
    {
        switch(opt) {
            case '?': exit(EXIT_FAILURE); break; //unknown option; getopt_long() already printed an error message
            case 'd': destinationDirectory = optarg; break;
            default: abort(); break;
        }
    }
    
    if (optind == argc)
    {
        std::cerr << "error: missing storage directory argument" << std::endl;
        std::cerr << usageLine << std::endl;
        exit(EXIT_FAILURE);
    }
    MUST(optind < argc, "argv index out of bounds");
    
    storageDirectory = argv[optind++];
    
    return optind;
}


static const std::set<std::string> areaIndicatorTags { 
    "building", "landuse", "natural", "water", "amenity", "leisure", "area"};

template <typename T>
bool hasAreaTag( const T &tags )
{
    for (const Tag &tag : tags)
        if ( areaIndicatorTags.count(tag.first))
            return true;
            
    return false;
}

static const std::set<std::string> lineIndicatorTags { 
    "highway", "waterway", "boundary", "power", "barrier", "railway" };

template <typename T>
bool hasLineTag( const T &tags)
{
    for (const Tag &tag : tags)
        if ( lineIndicatorTags.count(tag.first))
            return true;
            
    return false;
}

static const double INT_TO_LAT_LNG = 1/10000000.0;
static const double INT_TO_MERCATOR_METERS = 1/100.0;

static const double HALF_EARTH_CIRCUMFERENCE = 20037508.34; // ~ 20037km
static const double PI = 3.141592653589793;

static inline void convertWsg84ToWebMercator( int32_t &lat, int32_t &lng)
{
    static const int MAX_MERCATOR_VALUE = HALF_EARTH_CIRCUMFERENCE / INT_TO_MERCATOR_METERS;

    int32_t x =  (lng * INT_TO_LAT_LNG) / 180 * MAX_MERCATOR_VALUE;
    MUST( x >= -MAX_MERCATOR_VALUE && x <= MAX_MERCATOR_VALUE, "projection overflow");
    
    int32_t y =  log(tan((90 + lat * INT_TO_LAT_LNG) * PI / 360)) / PI
    * MAX_MERCATOR_VALUE;
    MUST( y >= -MAX_MERCATOR_VALUE && y <= MAX_MERCATOR_VALUE, "projection overflow");
        
    lat = x;
    lng = y;
}


void convertWsg84ToWebMercator( OsmLightweightWay &way)
{
    for (uint64_t i = 0; i < way.numVertices; i++)
        convertWsg84ToWebMercator( way.vertices[i].lat, way.vertices[i].lng);
}

void convertWsg84ToWebMercator( GenericGeometry &geom)
{
    MUST( geom.getFeatureType() == FEATURE_TYPE::POLYGON, "not implemented");

    int32_t* geoPtr = (int32_t*)geom.getGeometryPtr();
    
    uint32_t numRings =  *(geoPtr++);
            
    while (numRings--)
    {
        int32_t numPoints = *(geoPtr++);
        assert(numPoints >= 0 && numPoints < 10000000 && "overflow");
        geoPtr += 1;
        
        for (int32_t i = 0; i < numPoints; i++, geoPtr+=2 )
            convertWsg84ToWebMercator( geoPtr[0], geoPtr[1]);
    }
}

int main(int argc, char** argv)
{
    usageLine = std::string("usage: ") + argv[0] + " --dest <tile destination directory> <storage directory>";

    parseArguments(argc, argv);
    
    MUST(storageDirectory.length() > 0, "empty storage location given")
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";
    
    if (destinationDirectory.length() == 0)
    {
        std::cerr << "error: missing required parameter '--dest'" << std::endl;
        std::cerr << usageLine << endl;
        exit(EXIT_FAILURE);
    }
    if (destinationDirectory.back() != '/' && destinationDirectory.back() != '\\')
        destinationDirectory += "/";

    ensureDirectoryExists(destinationDirectory);
    
    /*Envelope worldBounds = { .latMin = -900000000, .latMax = 900000000, 
                             .lngMin = -1800000000,.lngMin = 1800000000};*/
               
    Envelope webMercatorWorldBounds = { .xMin = -2003750834, .xMax = 2003750834, 
                                        .yMin = -2003750834, .yMax = 2003750834}; 
    
    FileBackedTile areaStorage( (destinationDirectory + "area").c_str(), webMercatorWorldBounds, MAX_META_NODE_SIZE);
    
    FileBackedTile lineStorage( (destinationDirectory + "line").c_str(), webMercatorWorldBounds, MAX_META_NODE_SIZE);
    //FileBackedTile storageLod12( (destinationDirectory + "lod12").c_str(), worldBounds, MAX_META_NODE_SIZE);

    uint64_t numWays = 0;
    //uint64_t numLod12Ways = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;
    uint64_t numTagBytes = 0;
    cout << "stage 1: subdividing dataset to quadtree meta nodes of no more than "
         << (MAX_META_NODE_SIZE/1000000) << "MB." << endl;

    FILE* f = fopen( (storageDirectory + "multipolygons.bin").c_str(), "rb");
    MUST(f, "cannot open file");
    
    int ch;
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);
        GenericGeometry geom(f);
        convertWsg84ToWebMercator(geom);

        Envelope bounds = geom.getBounds();
        
        if (! bounds.isValid())
            continue;
        
        vector<Tag> tags = geom.getTags();
        
        // all multipolygons are by definition areas and not lines
        /*if (hasLineTag( tags ))
            lineStorage.add( geom, bounds);*/
            
        if (hasAreaTag(tags))
            areaStorage.add(geom, bounds);
/*        else
            cerr << "[WARN] multipolygon " << geom.getEntityId() << " has no tag that indicates it should be treated as an area. Skipping." << endl;*/

        if (++pos % 10000 == 0)
            cout << (pos/1000) << "k multipolygons read" << endl;
        
    }
    
    fclose(f); 

    pos = 0;
    for (OsmLightweightWay way: LightweightWayStore(storageDirectory + "ways", true))
    {
        way.unmap();
        convertWsg84ToWebMercator(way);
        
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        Tags tags = way.getTags();
        if ( hasAreaTag( tags))
            areaStorage.add(way, way.getBounds());
            
        if ( hasLineTag( tags))
            lineStorage.add(way, way.getBounds());
        
        numWays += 1;

        numVertices += way.numVertices;
            
        for (Tag kv: way.getTags())
        {
            //cout << kv.first << " = " << kv.second << endl;
            numTagBytes += 2 + kv.first.size() + kv.second.size();
        }
        
            

        /*way = getLod12Version(way);
        if (!way.id)    //has been invalidated by getLod12Version() --> should not be stored
            continue;
            
        storageLod12.add(way, way.getBounds());
        numLod12Ways += 1;*/
    }
    
    
    cout << "stage 2: subdividing meta nodes to individual nodes of no more than "
         << (MAX_NODE_SIZE/1000000) << "MB." << endl;
//    exit(0);
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on Linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
    lineStorage.closeFiles();
    areaStorage.closeFiles();
    
    lineStorage.subdivide(MAX_NODE_SIZE);
    areaStorage.subdivide(MAX_NODE_SIZE);
 
    /*storageLod12.closeFiles();
    storageLod12.subdivide(MAX_NODE_SIZE, true);*/
    cout << "done." << endl;

    cout << "stats: data set contains " << (numWays     / 1000000) << "M ways "
         << "with " << (numVertices/1000000) << "M vertices and " << (numTagBytes / 1000000) 
         << "MB tags" << endl;
    //cout << "stats: data set contains " << (numLod12Ways/ 1000) << "k ways with " << (numVertices/1000) << "k vertices and " << (numTagBytes / 1000) << "kB tags at LOD 12." << endl;   
    cout << "stats: skipped: "  << frequencies[0] << 
                 ", landuse: "  << frequencies[1] << 
                 ", natural: "  << frequencies[2] << 
                 ", highway: "  << frequencies[3] << 
                 ", boundary: " << frequencies[4] << endl;

    return EXIT_SUCCESS;
}
