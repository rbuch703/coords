
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
#include "geom/envelope.h"
#include "geom/geomSerializers.h"
#include "geom/srsConversion.h"
#include "containers/osmNodeStore.h"
#include "containers/osmRelationStore.h"
#include "containers/reverseIndex.h"
#include "containers/chunkedFile.h"
#include "misc/cleanup.h"
#include "misc/escapeSequences.h"
#include "lod/lodHandler.h"
#include "lod/addressLodHandler.h"
#include "lod/placeLodHandler.h"
#include "lod/buildingPolygonLodHandler.h"
#include "lod/landusePolygonLodHandler.h"
#include "lod/waterPolygonLodHandler.h"
#include "lod/roadLodHandler.h"
#include "lod/boundaryLodHandler.h"
#include "lod/waterwayLodHandler.h"

using namespace std;

std::string storageDirectory;
std::string tileDirectory;


bool parseArguments(int argc, char** argv)
{
    bool createLods = true;
    const std::string usageLine = std::string("usage: ") + argv[0] + " [-l|--no-lod] --dest <tile destination directory> <storage directory>";
    
    static const struct option long_options[] =
    {
        {"dest",   required_argument, NULL, 'd'},
        {"no-lod", no_argument,       NULL, 'l'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "ld:", long_options, &opt_idx)))
    {
        switch(opt) {
            //unknown option; getopt_long() already printed an error message
            case '?': exit(EXIT_FAILURE); break; 
            case 'l': createLods = false; break;
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
    
    return createLods;
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

/* FIXME: this method uses a slow topology-preserving simplifier. This is sensible for polygons,
 *        where a simpler simplifier may cause topological errors (e.g. self-overlaps) that
 *        may break rendering. But for simple line strings, a simpler and much faster algorithm
 *        would be sufficient. */
void addToTileSet(geos::geom::Geometry* &geometry,
                  uint64_t id,
                  GEOMETRY_FLAGS flags,
                  int8_t zIndex,
                  RawTags tags,
                  LodHandler* handler,
                  int coarsestZoomLevel,
                  bool createLods
                  )
{
    if (coarsestZoomLevel < 0)
        return;
    
    if (!createLods)
    {
        GenericGeometry gen = serialize( geometry, id, flags, zIndex, tags);
        handler->store(gen, gen.getBounds(), LodHandler::MAX_ZOOM_LEVEL);
        return;
    }

    const void* const* storeAtLevel = handler->getZoomLevels();
    for (int zoomLevel = LodHandler::MAX_ZOOM_LEVEL; zoomLevel >= coarsestZoomLevel; zoomLevel--)
    {
        if (!storeAtLevel[zoomLevel])
            continue;
            
        double pixelWidthInCm = MAP_WIDTH_IN_CM / double(256 * (1ull << zoomLevel));
        double pixelArea = pixelWidthInCm * pixelWidthInCm; // in [cm²]

        if (handler->isArea() && geometry->getArea() < pixelArea)
            break;
        
        geos::simplify::TopologyPreservingSimplifier simp(geometry);
        simp.setDistanceTolerance(pixelWidthInCm);
        geos::geom::Geometry *simplifiedGeometry = simp.getResultGeometry()->clone();
        
        //is still bigger than a single pixel after the simplification
        if (!handler->isArea() || simplifiedGeometry->getArea() >= pixelArea)
        {
            GenericGeometry gen = serialize( simplifiedGeometry, id, flags, zIndex, tags);
            handler->store(gen, gen.getBounds(), zoomLevel);
        }
        delete geometry;
        geometry = simplifiedGeometry;
    }
}

void parsePolygons(std::vector<LodHandler*> &lodHandlers, std::string storageDirectory, bool createLods)
{
    FILE* f = fopen( (storageDirectory + "multipolygons.bin").c_str(), "rb");
    
    MUST(f, "cannot open file 'multipolygons.bin'. Did you forget to run the multipolygon reconstructor?");
    int pos = 0;
    int ch;
    while ( (ch = fgetc(f)) != EOF)
    {
        ungetc(ch, f);
        GenericGeometry geom(f);
        MUST( geom.getEntityType() == OSM_ENTITY_TYPE::RELATION, 
             "found multipolygon that is not a relation");
        convertWgs84ToWebMercator(geom);

        Envelope bounds = geom.getBounds();
        
        if (! bounds.isValid())
            continue;
        
        RawTags tags = geom.getTags();
        TagDictionary tagsDict = tags.asDictionary();

        for (LodHandler* handler: lodHandlers)
        {
            int level = handler->applicableUpToZoomLevel(tagsDict, true);
            
            if (level < 0) //not applicable at all
                continue;

            geos::geom::Geometry *geosGeom = createGeosGeometry(geom);
            addToTileSet(geosGeom, geom.getEntityId(), geom.getGeometryFlags(), 
                         handler->getZIndex(tagsDict), tags, handler, level, createLods);

            delete geosGeom;
        }

        if (++pos % 10000 == 0)
            cout << "              " << (pos/1000) << "k polygons read" << endl;
    }
    fclose(f);
}

void parseNodes(std::vector<LodHandler*> &lodHandlers, std::string storageDirectory)
{
    ChunkedFile nodes( storageDirectory + "nodes.data");
    uint64_t numNodesRead = 0;
    for (Chunk nodeChunk : nodes)
    {
        if (++numNodesRead % 1000000 == 0)
            cout << "\t      " << (numNodesRead/1000000) << "M nodes read" << endl;

        OsmNode node(nodeChunk.getDataPtr());
        TagDictionary tagsDict( node.tags.begin(), node.tags.end());
        
        convertWgs84ToWebMercator( node.lat, node.lng);
        
        for (LodHandler* handler : lodHandlers)
        {
            int coarsestZoomLevel = handler->applicableUpToZoomLevel(tagsDict, true);
            
            if (coarsestZoomLevel < 0) //not applicable at all
                continue;

            const void* const* storeAtLevel = handler->getZoomLevels();
            for (int zoomLevel = LodHandler::MAX_ZOOM_LEVEL; 
                     zoomLevel >= coarsestZoomLevel; 
                     zoomLevel--)
            {
                if (!storeAtLevel[zoomLevel])
                    continue;
                    
                handler->store(node, zoomLevel, handler->getZIndex(tagsDict));
            }
            //addToTileSet(geosGeom, geom.getEntityId(), tags, handler, level);
            //pointsStorage.add(node);
            
        }
    }
    cerr << "\t          total: " << numNodesRead << " nodes." << endl;
}

void parseWays(std::vector<LodHandler*> &lodHandlers, std::string storageDirectory, bool createLods)
{
    uint64_t numWays = 0;
    uint64_t pos = 0;
    uint64_t numVertices = 0;

    cerr << "\t          loading tags from boundary relations" << endl;
    map<uint64_t,TagDictionary> boundaryRelationTags= loadBoundaryRelationTags(storageDirectory);
        
    ReverseIndex wayReverseIndex(storageDirectory + "wayReverse", false);
    cerr << "\t          loaded " << boundaryRelationTags.size() << " boundary relations." << endl;
    
    // the IDs of ways that serve as outer ways of multipolygons.
    std::set<uint64_t> outerWayIds = 
        getSetFromFileEntries<uint64_t>( storageDirectory + "outerWayIds.bin");
    std::cout << "\t          " << (outerWayIds.size()/1000) 
              << "k ways are part of outer ways of multipolygons" << endl;      


    pos = 0;
    for (const Chunk & chunk : ChunkedFile(storageDirectory + "ways.data"))
    {
        const uint8_t* ptr = chunk.getDataPtr();
        OsmWay way(ptr);
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;

        if (way.refs.size() < 2)
        {
            cout << ESC_FG_YELLOW << "[WARN] way " << way.id 
                 << " has less than two vertices. Skipping." << ESC_RESET << endl;
             continue;
        }

        numWays += 1;
        numVertices += way.refs.size();
        convertWgs84ToWebMercator(way);

        if (wayReverseIndex.isReferenced(way.id))
            way.addTagsFromBoundaryRelations( wayReverseIndex.getReferencingRelations(way.id),
                                              boundaryRelationTags);
        

        TagDictionary tagsDict( way.tags.begin(), way.tags.end());
        for (LodHandler* handler: lodHandlers)
        {
            int level = handler->applicableUpToZoomLevel(tagsDict, way.isClosed());
            if (level < 0)
                continue;
                
            if (handler->isArea() && !way.isClosed())
                continue;
                
            way.tags = vector<OsmKeyValuePair>(tagsDict.begin(), tagsDict.end());
            geos::geom::Geometry *geosGeom = createGeosGeometry(way, handler->isArea());
            uint8_t *tags = RawTags::serialize(way.tags);
            addToTileSet(geosGeom, way.id, 
                         handler->isArea() ? GEOMETRY_FLAGS::WAY_POLYGON : GEOMETRY_FLAGS::LINE,
                         handler->getZIndex(tagsDict), RawTags(tags), handler, level, createLods);
            delete [] tags;

            delete geosGeom;
        }
    }

    cout << "stats: data set contains " << (numWays     / 1000000) << "M ways "
         << "with " << (numVertices/1000000) << "M vertices " << endl;

}


int main(int argc, char** argv)
{
    bool createLods = parseArguments(argc, argv);
    ensureDirectoryExists(tileDirectory);

    std::vector<LodHandler*> lodHandlers;
    lodHandlers.push_back( new BuildingPolygonLodHandler(tileDirectory, "building"));
    lodHandlers.push_back( new LandusePolygonLodHandler(tileDirectory, "landuse"));
    lodHandlers.push_back( new WaterPolygonLodHandler(tileDirectory, "water"));
    lodHandlers.push_back( new RoadLodHandler(tileDirectory, "road"));
    lodHandlers.push_back( new BoundaryLodHandler(tileDirectory, "boundary"));
    lodHandlers.push_back( new WaterwayLodHandler(tileDirectory, "waterway"));
    
    std::vector<LodHandler*> pointLodHandlers;
    pointLodHandlers.push_back( new AddressLodHandler(tileDirectory, "address"));
    pointLodHandlers.push_back( new PlaceLodHandler(tileDirectory, "place"));
    
    if (!createLods)
    {
        std::vector<int> simplifiedLods;
        for (int i = 0; i < LodHandler::MAX_ZOOM_LEVEL; i++)
            simplifiedLods.push_back(i);
            
        for (LodHandler* handler : lodHandlers)
            handler->disableLods( simplifiedLods);
            
        for (LodHandler* handler: pointLodHandlers)
            handler->disableLods( simplifiedLods);
    }

    cout << "stage 1/2: subdividing dataset to quadtree meta nodes of no more than "
         << (LodHandler::MAX_META_NODE_SIZE/1000000) << "MB." << endl;

    cout << "    stage 1a: parsing polygons" << endl;
    parsePolygons(lodHandlers, storageDirectory, createLods);

    cerr << "    stage 1b: parsing points" << endl;
    parseNodes(pointLodHandlers, storageDirectory);

    cerr << "    stage 1c: parsing ways" << endl;
    parseWays(lodHandlers, storageDirectory, createLods);
    
    cout << endl << "stage 2/2: subdividing meta nodes to individual nodes of no more than "
         << (LodHandler::MAX_NODE_SIZE/1000000) << "MB." << endl;
    /* the number of concurrently open files for a given process is limited by the OS (
       e.g. to ~1000 for normal user accounts on Linux. Keeping all node files open
       concurrently may easily exceed that limit. But in stage 2, we only need to keep
       open the file descriptors of the current meta node we are subdividing and its 
       children. So we can close all other file descriptors for now. */
   
    for (LodHandler *handler : lodHandlers)
        handler->closeFiles();

    for (LodHandler *handler : pointLodHandlers)
        handler->closeFiles();

    
    for (LodHandler *handler : lodHandlers)
    {
        handler->subdivide();
        delete handler;
    }

    for (LodHandler *handler : pointLodHandlers)
    {
        handler->subdivide();
        delete handler;
    }
 

    return EXIT_SUCCESS;
}
