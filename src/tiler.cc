
#include <stdio.h>
#include <getopt.h> //for getopt_long()
//#include <unistd.h> // for unlink()
//#include <sys/stat.h> //for stat()
//#include <math.h>

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
#include "geom/srsConversion.h"
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
std::string tileDirectory;

int parseArguments(int argc, char** argv)
{
    const std::string usageLine = std::string("usage: ") + argv[0] + " --dest <tile destination directory> <storage directory>";
    
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
            //unknown option; getopt_long() already printed an error message
            case '?': exit(EXIT_FAILURE); break; 
            case 'd': tileDirectory = optarg; break;
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

    MUST(storageDirectory.length() > 0, "empty storage location given")
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";
    
    if (tileDirectory.length() == 0)
    {
        std::cerr << "error: missing required parameter '--dest'" << std::endl;
        std::cerr << usageLine << endl;
        exit(EXIT_FAILURE);
    }
    if (tileDirectory.back() != '/' && tileDirectory.back() != '\\')
        tileDirectory += "/";
    
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

static const std::set<std::string> l12Highways = {
    "motorway", "motorway_link",
    "trunk",    "trunk_link",
    "primary",  "primary_link",
    "secondary","secondary_link",
    "tertiary", "tertiary_link"};

uint64_t getPositiveIntIfOnlyDigits(const char* s)
{
    bool onlyDigits = true;
    int numDigits = 0;
    int num = 0;
    
    for ( const char* pos = s; *pos != '\0'; pos++)
    {
        numDigits += 1;
        onlyDigits &= (*pos >= '0' && *pos <= '9');
        num = (num * 10) + (*pos - '0');
    }
    
    return onlyDigits ? num : 0;
}

template <typename T>
bool hasL12LineTag( const T &tags)
{
    bool isAdminBoundary = false;
    int adminLevel = 0;
    for (const Tag &tag : tags)
    {
        if (tag.first == "highway")
            if ( l12Highways.count(tag.second)) return true;
        if (tag.first == "boundary" && tag.second == "administrative")
            isAdminBoundary = true;
        if (tag.first == "admin_level")
            adminLevel = getPositiveIntIfOnlyDigits(tag.second.c_str());
        if (tag.first == "railway" && (tag.second == "rail" || tag.second == "narrow_gauge"))
            return true;
        if (tag.first == "waterway" && (tag.second == "river" || tag.second== "canal"))
            return true;
        
    }

    if (isAdminBoundary && adminLevel > 0 && adminLevel <= 6) 
        return true;
        
    return false;
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

map<uint64_t, TagDictionary> loadBoundaryRelationTags(const string &storageDirectory)
{
    map<uint64_t, TagDictionary> res;
    
    RelationStore relStore(storageDirectory + "relations");
    for (OsmRelation rel : relStore)
    {
        TagDictionary tags(rel.tags.begin(), rel.tags.end());

        if (tags.count("type") && tags["type"] == "boundary")
            res.insert( make_pair(rel.id, tags) );
    }
    return res;
}

void cleanupTileDirectory(const std::string tileDirectory)
{
    deleteNumberedFiles(tileDirectory, "area_", "");
    deleteIfExists(     tileDirectory, "area_");

    deleteNumberedFiles(tileDirectory, "area_l12_", "");
    deleteIfExists(     tileDirectory, "area_l12_");

    deleteNumberedFiles(tileDirectory, "line_", "");
    deleteIfExists(     tileDirectory, "line_");

    deleteNumberedFiles(tileDirectory, "line_l12_", "");
    deleteIfExists(     tileDirectory, "line_l12_");

}

static const uint64_t MAP_WIDTH_IN_CM     = 2 * (uint64_t)2003750834;
static const uint64_t L12_MAP_WIDTH_IN_PIXELS = 256 * ( 1 << 12);
static const double   L12_MAP_RESOLUTION_IN_CM_PER_PIXEL = 
                          MAP_WIDTH_IN_CM / (double)L12_MAP_WIDTH_IN_PIXELS;

static const double   L12_MAX_DEVIATION = L12_MAP_RESOLUTION_IN_CM_PER_PIXEL;

static const double   L12_PIXEL_AREA = L12_MAP_RESOLUTION_IN_CM_PER_PIXEL *
                                       L12_MAP_RESOLUTION_IN_CM_PER_PIXEL;
static const double   L12_MIN_POLYGON_AREA = 4 * L12_PIXEL_AREA;

void simplifyAndAdd(geos::geom::Geometry *geom, const RawTags &tags, uint64_t wayId,
                    FileBackedTile &target, double maxDeviation)
{
    geos::simplify::TopologyPreservingSimplifier simp(geom);
    simp.setDistanceTolerance(maxDeviation);

    GenericGeometry gen = serialize( &(*simp.getResultGeometry()),
                                     wayId | IS_WAY_REFERENCE, tags);
                                     
    target.add( gen, gen.getBounds());
}

int main(int argc, char** argv)
{
    parseArguments(argc, argv);
    ensureDirectoryExists(tileDirectory);
    cleanupTileDirectory( tileDirectory);
    
    const Envelope mercatorWorldBounds = { .xMin = -2003750834, .xMax = 2003750834, 
                                           .yMin = -2003750834, .yMax = 2003750834}; 
    
    FileBackedTile areaStorage( tileDirectory + "area_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    FileBackedTile areaL12Storage( tileDirectory + "area_l12_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    FileBackedTile lineStorage( tileDirectory + "line_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    FileBackedTile lineL12Storage( tileDirectory + "line_l12_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    //FileBackedTile storageLod12( (tileDirectory + "lod12").c_str(), worldBounds, MAX_META_NODE_SIZE);

    uint64_t numWays = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;
    cout << "stage 1: subdividing dataset to quadtree meta nodes of no more than "
         << (MAX_META_NODE_SIZE/1000000) << "MB." << endl;

    FILE* f = fopen( (storageDirectory + "multipolygons.bin").c_str(), "rb");
    MUST(f, "cannot open file 'multipolygons.bin'. Did you forget to run the multipolygon reconstructor?");
    
    int ch;
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);
        GenericGeometry geom(f);
        MUST( geom.getEntityType() == OSM_ENTITY_TYPE::RELATION, "found multipolygon that is not a relation");
        convertWgs84ToWebMercator(geom);

        Envelope bounds = geom.getBounds();
        
        if (! bounds.isValid())
            continue;
        
        RawTags tags = geom.getTags();
        
        /* all multipolygons are by definition areas and not lines, so we don't have to check 
           for line tags */
        if (isArea(tags, true))
        {
            areaStorage.add( geom, bounds);
            geos::geom::Geometry *geosGeom = createGeosGeometry(geom);
            if (geosGeom->getArea() > L12_MIN_POLYGON_AREA)
            {
                geos::simplify::TopologyPreservingSimplifier simp(geosGeom);
                simp.setDistanceTolerance(L12_MAX_DEVIATION);
                GenericGeometry gen = serialize( &(*simp.getResultGeometry()), geom.getEntityId(), tags);
                areaL12Storage.add( gen, bounds);
            }

            delete geosGeom;

        }
/*        else
            cerr << "[WARN] multipolygon " << geom.getEntityId() << " has no tag that indicates it should be treated as an area. Skipping." << endl;*/

        if (++pos % 10000 == 0)
            cout << (pos/1000) << "k multipolygons read" << endl;
    }
    
    fclose(f);

    map<uint64_t,TagDictionary> boundaryRelationTags= loadBoundaryRelationTags(storageDirectory);
        
    ReverseIndex wayReverseIndex(storageDirectory + "wayReverse", false);
    cerr << "loaded " << boundaryRelationTags.size() << " boundary relations." << endl;
    
    // the IDs of ways that serve as outer ways of multipolygons.
    std::set<uint64_t> outerWayIds = getSetFromFileEntries<uint64_t>(storageDirectory + "outerWayIds.bin");
    std::cout << (outerWayIds.size()/1000) << "k ways are part of outer ways of multipolygons" << endl;      

    pos = 0;
    for (OsmLightweightWay way: LightweightWayStore(storageDirectory + "ways", true))
    {
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        way.unmap();
        numWays += 1;
        numVertices += way.numVertices;
        convertWgs84ToWebMercator(way);

        if (wayReverseIndex.isReferenced(way.id))
            way.addTagsFromBoundaryRelations( wayReverseIndex.getReferencingRelations(way.id),
                                              boundaryRelationTags);
        
        /* note: do this only after way.addTagsFromBoundaryRelations() !!!
         * RawTags do not allocate memory themselves, but are just an overlay to the
         * tag memory of the underlying OsmLightweightWay. And addTagsFromBoundaryRelations()
         * re-allocates the memory for the way's tags, invalidating said overlay. */
        
        if ( isArea( way.getTags(), way.isArea()) )
        {
            if (!(outerWayIds.count(way.id)))
            {
                if (!way.isArea())
                    cerr << ESC_FG_YELLOW << "[WARN] way " << way.id << " has area tags, but is "
                         << "not a closed area. Skipping it." << ESC_RESET << endl;
                else
                {
                    areaStorage.add(way, way.getBounds(), true);
                
                    geos::geom::Geometry *geosGeom = createGeosGeometry(way);
                    if (geosGeom->getArea() > L12_MIN_POLYGON_AREA)
                        simplifyAndAdd(geosGeom, way.getTags(), way.id, areaL12Storage,
                                       L12_MAX_DEVIATION);

                    delete geosGeom;
                }
            }
        }   
        
        if (hasLineTag(way.getTags()))
        {
            lineStorage.add(way, way.getBounds(), false);
            if ( hasL12LineTag(way.getTags()))
            {
                geos::geom::Geometry *geosGeom = createGeosGeometry(way);
                simplifyAndAdd(geosGeom, way.getTags(), way.id, lineL12Storage,
                               1.5*L12_MAX_DEVIATION);
                delete geosGeom;
            }
        }
    }
    
    
    cout << "stage 2: subdividing meta nodes to individual nodes of no more than "
         << (MAX_NODE_SIZE/1000000) << "MB." << endl;
//    exit(0);
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on Linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
    areaStorage.closeFiles();
    lineStorage.closeFiles();
    areaL12Storage.closeFiles();
    lineL12Storage.closeFiles();
    
    areaStorage.subdivide(MAX_NODE_SIZE);
    lineStorage.subdivide(MAX_NODE_SIZE);
    areaL12Storage.subdivide(MAX_NODE_SIZE);
    lineL12Storage.subdivide(MAX_NODE_SIZE);
 
    cout << "done." << endl;

    cout << "stats: data set contains " << (numWays     / 1000000) << "M ways "
         << "with " << (numVertices/1000000) << "M vertices " << endl;
    return EXIT_SUCCESS;
}
