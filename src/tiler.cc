
#include <stdio.h>
#include <getopt.h> //for getopt_long()
//#include <unistd.h> // for unlink()
#include <sys/stat.h> //for stat()
#include <math.h>

#include <vector>
#include <list>
#include <iostream>
#include <set>

#include <geos/simplify/TopologyPreservingSimplifier.h>
#include "tiles.h"
#include "osm/osmTypes.h"
#include "osm/osmMappedTypes.h"
#include "geom/envelope.h"
#include "geom/geomSerializers.h"
#include "containers/osmWayStore.h"
#include "containers/osmRelationStore.h"
#include "containers/reverseIndex.h"
#include "misc/cleanup.h"
#include "misc/escapeSequences.h"
#include "misc/varInt.h"

using namespace std;

const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
const uint64_t MAX_NODE_SIZE      = 10ll * 1000 * 1000;

std::string storageDirectory;
std::string destinationDirectory;
std::string usageLine;

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
    
    map<string, string> tags = src.getTags().asDictionary();

    return OsmWay( src.id, 
                   src.version, 
                   vector<OsmGeoPosition>( src.vertices, src.vertices + src.numVertices),
                   vector<OsmKeyValuePair>( tags.begin(), tags.end()));
}

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

template <typename T>
bool isArea( const T &tags, bool geometryIsClosedArea )
{
    for (const Tag &tag : tags)
    {
        if (tag.first == "building" && (tag.second != "wall" && tag.second != "bridge")) return true;
        if (tag.first == "landuse") return true;
        if (tag.first == "natural")
        {
            if (tag.second != "coastline" && 
                tag.second != "cliff" &&
                tag.second != "hedge" &&
                tag.second != "tree" &&
                tag.second != "ridge" &&
                tag.second != "tree_row")
                return true;
                
            //cliffs in OSM can be linear or areas
            if (tag.second == "cliff" && geometryIsClosedArea) return true;
        }
        
        if (tag.first == "place")
        {
            if (geometryIsClosedArea) return true;
        }
        
        if (tag.first == "water" && tag.second == "riverbank") return true;
        if (tag.first == "waterway" && geometryIsClosedArea) return true;
        if (tag.first == "amenity" && geometryIsClosedArea) return true;
        if (tag.first == "leisure") 
        {
            if (tag.second != "slipway" && 
                tag.second != "track") return true;
        }
        if (tag.first == "area" && tag.second != "no") return true;
    }
            
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

static const double HALF_EARTH_CIRCUMFERENCE = 20037508.3428; // ~ 20037km
static const double PI = 3.141592653589793;

static inline void convertWsg84ToWebMercator( int32_t &lat, int32_t &lng)
{
    MUST( lat >=  -900000000 && lat <=  900000000, "latitude out of range");
    MUST( lng >= -1800000000 && lng <= 1800000000, "longitude out of range");

    static const int MAX_MERCATOR_VALUE = HALF_EARTH_CIRCUMFERENCE / INT_TO_MERCATOR_METERS;

    int32_t x =  (lng * INT_TO_LAT_LNG) / 180 * MAX_MERCATOR_VALUE;
    MUST( x >= -MAX_MERCATOR_VALUE && x <= MAX_MERCATOR_VALUE, "projection overflow");
    
    int32_t y =  log(tan((90 + lat * INT_TO_LAT_LNG) * PI / 360)) / PI * MAX_MERCATOR_VALUE;
    if (y < -MAX_MERCATOR_VALUE) y = -MAX_MERCATOR_VALUE;
    if (y >  MAX_MERCATOR_VALUE) y =  MAX_MERCATOR_VALUE;
    
       
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

    /* conversion changes value in 'geom' and - since those values are delta-encoded varInts - 
     * may change the size of 'geom'. So we need to allocate new storage for the output.
     * Since all 'geom' members compressed may grow at most by a factor of five (one byte before,
     * encoded (u)int32_t -> 5 bytes after), we need to allocate at most 5 times as much memory
     * as the original data structure is big. */
    uint64_t newAllocatedSize = geom.numBytes * 5;
    uint8_t *newData = new uint8_t[newAllocatedSize];
    uint8_t *basePtr = geom.bytes;
    const uint8_t *geoPtr  = geom.getGeometryPtr();
    int64_t baseSize = geoPtr - basePtr;
    MUST( baseSize > 0, "logic error");
    
    // copy that part of 'geom' that won't change through projection (id, type, tags)
    memcpy(newData, basePtr, baseSize);
    
    uint8_t *newGeoPtr = newData + baseSize;
    
    int nRead = 0;
    //int nWritten=0;
    uint64_t numRings = varUintFromBytes(geoPtr, &nRead);
    geoPtr+= nRead;
    newGeoPtr += varUintToBytes(numRings, newGeoPtr);
            
    while (numRings--)
    {
        
        uint64_t numPoints = varUintFromBytes(geoPtr, &nRead);
        geoPtr += nRead;
        newGeoPtr += varUintToBytes(numPoints, newGeoPtr);
        
        assert(numPoints < 10000000 && "overflow"); //sanity check, not an actual limit
        
        int64_t xIn = 0;
        int64_t yIn = 0;
        int64_t prevXOut = 0;
        int64_t prevYOut = 0;
        while (numPoints-- > 0)
        {
            xIn += varIntFromBytes(geoPtr, &nRead); //resolve delta encoding
            geoPtr += nRead;
            yIn += varIntFromBytes(geoPtr, &nRead);
            geoPtr += nRead;
            
            MUST( xIn >= INT32_MIN && xIn <= INT32_MAX, "overflow");
            MUST( yIn >= INT32_MIN && yIn <= INT32_MAX, "overflow");
            
            int32_t xOut = xIn;
            int32_t yOut = yIn;
            convertWsg84ToWebMercator( xOut, yOut);
            
            int64_t dX = xOut - prevXOut;
            int64_t dY = yOut - prevYOut;
            newGeoPtr += varIntToBytes(dX, newGeoPtr);
            newGeoPtr += varIntToBytes(dY, newGeoPtr);

            prevXOut = xOut;
            prevYOut = yOut;   
        }
    }
    uint64_t newSize = newGeoPtr - newData;
    MUST( newSize <= newAllocatedSize, "overflow");
    delete [] geom.bytes;
    geom.bytes = newData;
    geom.numBytes = newSize;
    geom.numBytesAllocated = newAllocatedSize;
}

template <typename T>
std::set<T> getSetFromFileEntries(std::string filename)
{
    std::set<T> res;
    
    FILE* f = fopen( filename.c_str(), "rb");
    T entry;
    while (fread( &entry, sizeof(entry), 1, f))
        res.insert(entry);

    fclose(f);
    return res;
}

uint64_t tryParse(const char* s, uint64_t defaultValue)
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

void addBoundaryTags( TagDictionary &tags, const vector<uint64_t> &referencingRelations, 
                      const map<uint64_t, TagDictionary> &boundaryTags/*, uint64_t wayId*/)
{

    uint64_t adminLevel = tags.count("admin_level") ? tryParse(tags["admin_level"].c_str(), 0xFFFF) : 0xFFFF;
    //cout << "admin level is " << adminLevel << endl;

    for ( uint64_t relId : referencingRelations)
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

map<uint64_t, TagDictionary> getBoundaryRelationTags(const string &storageDirectory)
{
    map<uint64_t, TagDictionary> res;
    
    RelationStore relStore(storageDirectory + "relations");
    for (OsmRelation rel : relStore)
    {
        TagDictionary tags(rel.tags.begin(), rel.tags.end());

        if (tags.count("type") && tags["type"] == "boundary")
        {
            res.insert( make_pair(rel.id, tags) );
            //cerr << ESC_FG_BLUE << "relation " << rel.id 
            ///     << " is a boundary" << ESC_RESET << endl;
        }
    }
    return res;
}

void cleanupTileDirectory(const std::string tileDirectory)
{
    deleteNumberedFiles(tileDirectory, "area", "");
    deleteIfExists(     tileDirectory, "area");
    
    deleteNumberedFiles(tileDirectory, "line", "");
    deleteIfExists(     tileDirectory, "line");
}

static const uint64_t MAP_WIDTH_IN_CM     = 2 * (uint64_t)2003750834;
static const uint64_t L12_MAP_WIDTH_IN_PIXELS = 256 * ( 1 << 12);
static const double   L12_MAP_RESOLUTION_IN_CM_PER_PIXEL = 
                          MAP_WIDTH_IN_CM / (double)L12_MAP_WIDTH_IN_PIXELS;

static const double   L12_MAX_DEVIATION = L12_MAP_RESOLUTION_IN_CM_PER_PIXEL;

static const double   L12_PIXEL_AREA = L12_MAP_RESOLUTION_IN_CM_PER_PIXEL *
                                       L12_MAP_RESOLUTION_IN_CM_PER_PIXEL;
static const double   L12_MIN_POLYGON_AREA = 4 * L12_PIXEL_AREA;


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
    cleanupTileDirectory( destinationDirectory);
    
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
    MUST(f, "cannot open file 'multipolygons.bin'");
    
    int ch;
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);
        GenericGeometry geom(f);
        MUST( geom.getEntityType() == OSM_ENTITY_TYPE::RELATION, "found multipolygon that is not a relation");
        convertWsg84ToWebMercator(geom);

        Envelope bounds = geom.getBounds();
        
        if (! bounds.isValid())
            continue;
        
        RawTags tags = geom.getTags();
        
        // all multipolygons are by definition areas and not lines
        /*if (hasLineTag( tags ))
            lineStorage.add( geom, bounds);*/
            
        if (isArea(tags, true))
        {
            geos::geom::Geometry *geosGeom = createGeosGeometry(geom);
            if (geosGeom->getArea() > L12_MIN_POLYGON_AREA)
            {
                geos::simplify::TopologyPreservingSimplifier simp(geosGeom);
                simp.setDistanceTolerance(L12_MAX_DEVIATION);
                GenericGeometry gen = serialize( &(*simp.getResultGeometry()), geom.getEntityId(), tags);
                areaStorage.add( gen, bounds);
            }

            delete geosGeom;

        }
/*        else
            cerr << "[WARN] multipolygon " << geom.getEntityId() << " has no tag that indicates it should be treated as an area. Skipping." << endl;*/

        if (++pos % 10000 == 0)
            cout << (pos/1000) << "k multipolygons read" << endl;
    }
    
    fclose(f); 

    map< uint64_t, TagDictionary> boundaryRelationTags = getBoundaryRelationTags(storageDirectory);
        
    ReverseIndex wayReverseIndex(storageDirectory + "wayReverse", false);
    cerr << "loaded " << boundaryRelationTags.size() << " boundary relations." << endl;
    
    // the IDs of ways that serve as outer ways of multipolygons.
    std::set<uint64_t> outerWayIds = getSetFromFileEntries<uint64_t>(storageDirectory + "outerWayIds.bin");
    std::cout << (outerWayIds.size()/1000) << "k ways are part of outer ways of multipolygons" << endl;      

    pos = 0;
    for (OsmLightweightWay way: LightweightWayStore(storageDirectory + "ways", true))
    {
        way.unmap();
        convertWsg84ToWebMercator(way);
        
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        TagDictionary tags = way.getTags().asDictionary();
        
        /*cout << "tags before: " << endl;
        for (OsmKeyValuePair kv : tags)
            cout << "\t" << kv.first << " -> " << kv.second << endl;*/
        if (wayReverseIndex.isReferenced(way.id))
        {
            vector<uint64_t> relationIds = wayReverseIndex.getReferencingRelations(way.id);
            uint64_t i = 0;
            while (i < relationIds.size())
            {
                if (! boundaryRelationTags.count(relationIds[i]))
                {
                    relationIds[i] = relationIds.back();
                    relationIds.pop_back();
                } else
                    i++;
            }
            
            if (relationIds.size() > 0)
                addBoundaryTags( tags, relationIds, boundaryRelationTags/*, way.id*/ );
        }
        /*cout << "tags after: " << endl;
        for (OsmKeyValuePair kv : tags)
            cout << "\t" << kv.first << " -> " << kv.second << endl;*/
        
        
        if ( isArea( tags, way.vertices[0] == way.vertices[way.numVertices-1] && way.numVertices > 3) )
        {
            /* Area tags on multipolygons' outer ways are pointless, as is the multipolygon
             * that forms the area and not the way that is part of it. Mostly these area
             * tags are leftovers from an old tagging schema where multipolygons had only a
             * single outer way and that way was tagged instead of the multipolygon. But in any
             * case, the way should not be rendered as an area, as the multipolygon already is.
             */
            if (!(outerWayIds.count(way.id)))
            {
                if (way.vertices[0] != way.vertices[way.numVertices-1] || way.numVertices <= 3)
                    cerr << ESC_FG_YELLOW << "[WARN] way " << way.id << " has area tags, but is "
                         << "not a closed area. Skipping it." << ESC_RESET << endl;
                else
                {
                    /* zoom level 12: 
                     * - map is 256* 1^12 = 1^20 pixels wide (also high)
                     * - map is 2*2003750834cm wide (also high)
                     * - each pixel corresponds to a width of 3821.851413727cm
                     * - each pixel corresponds to an area of 14606548 cm²
                     * - ignore all ways with an area of less than ~ 4px --> 58426192cm²*/
                    //std::cout << way.getArea() << std::endl;
                    geos::geom::Geometry *geosGeom = createGeosGeometry(way);
                    if (geosGeom->getArea() > L12_MIN_POLYGON_AREA)
                    {
                        geos::simplify::TopologyPreservingSimplifier simp(geosGeom);
                        simp.setDistanceTolerance(L12_MAX_DEVIATION);
                        
                        Tags dummy(tags.begin(), tags.end());
                        uint8_t* tagBytes = RawTags::serialize( dummy, (uint64_t*)nullptr);
                        RawTags rawTags(tagBytes);

                        GenericGeometry gen = serialize( &(*simp.getResultGeometry()),
                                                         way.id | IS_WAY_REFERENCE, rawTags);
                                                         
                        areaStorage.add( gen, gen.getBounds());
                        delete [] tagBytes;
                    }
                    delete geosGeom;
                }
            }
        }   
        
        if ( hasLineTag( tags))
            lineStorage.add(way, tags, way.getBounds());
        
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
    return EXIT_SUCCESS;
}
