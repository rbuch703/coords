
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
std::string destinationDirectory;
std::string usageLine;

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
            //unknown option; getopt_long() already printed an error message
            case '?': exit(EXIT_FAILURE); break; 
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
            res.insert( make_pair(rel.id, tags) );
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
    
    Envelope webMercatorWorldBounds = { .xMin = -2003750834, .xMax = 2003750834, 
                                        .yMin = -2003750834, .yMax = 2003750834}; 
    
    FileBackedTile areaStorage( (destinationDirectory + "area").c_str(), webMercatorWorldBounds, MAX_META_NODE_SIZE);
    FileBackedTile lineStorage( (destinationDirectory + "line").c_str(), webMercatorWorldBounds, MAX_META_NODE_SIZE);
    //FileBackedTile storageLod12( (destinationDirectory + "lod12").c_str(), worldBounds, MAX_META_NODE_SIZE);

    uint64_t numWays = 0;
    //uint64_t numLod12Ways = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;
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
        convertWgs84ToWebMercator(geom);

        Envelope bounds = geom.getBounds();
        
        if (! bounds.isValid())
            continue;
        
        RawTags tags = geom.getTags();
        
        /* all multipolygons are by definition areas and not lines, so we don't have to check 
           for line tags */
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
        convertWgs84ToWebMercator(way);
        
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        TagDictionary tags = way.getTags().asDictionary();
        
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
        
        if ( hasL12LineTag( tags))
        {
            geos::geom::Geometry *geosGeom = createGeosGeometry(way);
            geos::simplify::TopologyPreservingSimplifier simp(geosGeom);
            simp.setDistanceTolerance(L12_MAX_DEVIATION);
            
            Tags dummy(tags.begin(), tags.end());
            uint8_t* tagBytes = RawTags::serialize( dummy, (uint64_t*)nullptr);
            RawTags rawTags(tagBytes);

            GenericGeometry gen = serialize( &(*simp.getResultGeometry()),
                                             way.id | IS_WAY_REFERENCE, rawTags);
                                             
            lineStorage.add( gen, gen.getBounds());
            delete [] tagBytes; //need to delete manual, RawTags does not take ownership
            delete geosGeom;
            
        }
        
        numWays += 1;
        numVertices += way.numVertices;
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
 
    cout << "done." << endl;

    cout << "stats: data set contains " << (numWays     / 1000000) << "M ways "
         << "with " << (numVertices/1000000) << "M vertices " << endl;
    return EXIT_SUCCESS;
}
