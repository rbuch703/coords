
#include "polygonLodHandler.h"
#include "misc/cleanup.h"
#include "config.h"

const Envelope PolygonLodHandler::mercatorWorldBounds = { 
    .xMin = -2003750834, .xMax = 2003750834, 
    .yMin = -2003750834, .yMax = 2003750834}; 

PolygonLodHandler::PolygonLodHandler(std::string tileDirectory, std::string baseName): 
    tileDirectory(tileDirectory), baseName(baseName),
    baseTileSet(tileDirectory + baseName+"_", mercatorWorldBounds , MAX_META_NODE_SIZE)
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        lodTileSets[i] = nullptr;
}

PolygonLodHandler::~PolygonLodHandler()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        delete lodTileSets[i];
    
}


void PolygonLodHandler::cleanupFiles() const
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
    {
        char num[4];
        
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        
        deleteNumberedFiles(tileDirectory, baseName + "_" + num + "_", "");
        //deleteIfExists(tileDirectory, baseName + "_" + num + "_");
    }

    deleteNumberedFiles(tileDirectory, baseName + "_", "");
    //deleteIfExists(tileDirectory, baseName + "_");

}

const void* const* PolygonLodHandler::getZoomLevels() const
{
    return (const void* const*)lodTileSets;
}

void PolygonLodHandler::store (const GenericGeometry &geometry, const Envelope &env, int zoomLevel)
{
    MUST(zoomLevel >= 0 && zoomLevel <= MAX_ZOOM_LEVEL, "out of bounds");
    
    MUST(lodTileSets[zoomLevel], "writing to non-existent level of detail");
    lodTileSets[zoomLevel]->add(geometry, env);
}

void PolygonLodHandler::store (const GenericGeometry &geometry, const Envelope &env)
{
    baseTileSet.add(geometry, env);
}

void PolygonLodHandler::closeFiles()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        if (lodTileSets[i])
            lodTileSets[i]->closeFiles();
    
    baseTileSet.closeFiles();
}

void PolygonLodHandler::subdivide()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        if (lodTileSets[i])
            lodTileSets[i]->subdivide(MAX_NODE_SIZE);
    
    baseTileSet.subdivide(MAX_NODE_SIZE);
}

