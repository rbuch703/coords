
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

#include "lod/polygonLodHandler.h"
#include "lod/buildingPolygonLodHandler.h"
#include "lod/landusePolygonLodHandler.h"
#include "lod/waterPolygonLodHandler.h"

using namespace std;

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


static const uint64_t MAP_WIDTH_IN_CM     = 2 * (uint64_t)2003750834;

void simplifyAndAdd(geos::geom::Geometry *geom, const RawTags &tags, uint64_t wayId,
                    FileBackedTile &target, double maxDeviation)
{
    geos::simplify::TopologyPreservingSimplifier simp(geom);
    simp.setDistanceTolerance(maxDeviation);

    GenericGeometry gen = serialize( &(*simp.getResultGeometry()),
                                     wayId | IS_WAY_REFERENCE, tags);
                                     
    target.add( gen, gen.getBounds());
}

void addToTileSet(geos::geom::Geometry* &polygon, 
                   uint64_t entityId,
                   RawTags tags,
                   PolygonLodHandler* handler)
{
    
    const void* const* storeAtLevel = handler->getZoomLevels();
    {
        GenericGeometry gen = serialize( polygon, entityId, tags);
        handler->store(gen, gen.getBounds());
    }


    for (int zoomLevel = PolygonLodHandler::MAX_ZOOM_LEVEL; zoomLevel >= 0; zoomLevel--)
    {
        if (!storeAtLevel[zoomLevel])
            continue;
            
        double pixelWidthInCm = MAP_WIDTH_IN_CM / double(256 * (1ull << zoomLevel));
        double pixelArea = pixelWidthInCm * pixelWidthInCm; // in [cm²]
        if (polygon->getArea() < 4 * pixelArea)
            break;
        
        geos::simplify::TopologyPreservingSimplifier simp(polygon);
        simp.setDistanceTolerance(pixelWidthInCm);
        geos::geom::Geometry *simplifiedPolygon = simp.getResultGeometry()->clone();
        
        GenericGeometry gen = serialize( simplifiedPolygon, entityId, tags);
        handler->store(gen, gen.getBounds(), zoomLevel);
        delete polygon;
        polygon = simplifiedPolygon;    
    }

}

int main(int argc, char** argv)
{
    parseArguments(argc, argv);
    ensureDirectoryExists(tileDirectory);
    

    std::vector<PolygonLodHandler*> polygonLodHandlers;
    polygonLodHandlers.push_back( new BuildingPolygonLodHandler(tileDirectory, "building"));
    polygonLodHandlers.push_back( new LandusePolygonLodHandler(tileDirectory, "landuse"));
    polygonLodHandlers.push_back( new WaterPolygonLodHandler(tileDirectory, "water"));

    for (const PolygonLodHandler* handler : polygonLodHandlers)
        handler->cleanupFiles();
    

    uint64_t numWays = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;
    cout << "stage 1: subdividing dataset to quadtree meta nodes of no more than "
         << (PolygonLodHandler::MAX_META_NODE_SIZE/1000000) << "MB." << endl;

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
        TagDictionary tagsDict = tags.asDictionary();

        for (PolygonLodHandler* handler: polygonLodHandlers)
        {
            if (!handler->applicable(tagsDict, true))
                continue;

            geos::geom::Geometry *geosGeom = createGeosGeometry(geom);
            addToTileSet(geosGeom, geom.getEntityId(), tags, handler);

            delete geosGeom;
        }

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
        

        if (way.isClosed())
        {
            TagDictionary tagsDict = way.getTags().asDictionary();
            for (PolygonLodHandler* handler: polygonLodHandlers)
            {
                if (!handler->applicable(tagsDict, true))
                    continue;

                geos::geom::Geometry *geosGeom = createGeosGeometry(way);
                addToTileSet(geosGeom, way.id | IS_WAY_REFERENCE, way.getTags(), handler);

                delete geosGeom;
            }
        }
        //FIXME: re-add code to store lines
    }
    
    cout << "stage 2: subdividing meta nodes to individual nodes of no more than "
         << (PolygonLodHandler::MAX_NODE_SIZE/1000000) << "MB." << endl;
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on Linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
       
    for (PolygonLodHandler *handler : polygonLodHandlers)
        handler->closeFiles();
    

    for (PolygonLodHandler *handler : polygonLodHandlers)
    {
        handler->subdivide();
        delete handler;
    }
 
    cout << "done." << endl;

    cout << "stats: data set contains " << (numWays     / 1000000) << "M ways "
         << "with " << (numVertices/1000000) << "M vertices " << endl;
    return EXIT_SUCCESS;
}
