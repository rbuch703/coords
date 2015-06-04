
#include "lodHandler.h"
#include "misc/cleanup.h"

const Envelope LodHandler::mercatorWorldBounds = { 
    .xMin = -2003750834, .xMax = 2003750834, 
    .yMin = -2003750834, .yMax = 2003750834}; 

LodHandler::LodHandler(std::string tileDirectory, std::string baseName): 
    tileDirectory(tileDirectory), baseName(baseName),
    baseTileSet(tileDirectory + baseName+"_", mercatorWorldBounds , MAX_META_NODE_SIZE)
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        lodTileSets[i] = nullptr;
}


LodHandler::~LodHandler()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        delete lodTileSets[i];
}


void LodHandler::cleanupFiles() const
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

const void* const* LodHandler::getZoomLevels() const
{
    return (const void* const*)lodTileSets;
}

void LodHandler::store (const GenericGeometry &geometry, const Envelope &env, int zoomLevel)
{
    MUST(zoomLevel >= 0 && zoomLevel <= MAX_ZOOM_LEVEL, "out of bounds");
    
    MUST(lodTileSets[zoomLevel], "writing to non-existent level of detail");
    lodTileSets[zoomLevel]->add(geometry, env);
}

void LodHandler::store (const GenericGeometry &geometry, const Envelope &env)
{
    baseTileSet.add(geometry, env);
}

void LodHandler::closeFiles()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        if (lodTileSets[i])
            lodTileSets[i]->closeFiles();
    
    baseTileSet.closeFiles();
}

void LodHandler::subdivide()
{
    for (int i = 0; i <= MAX_ZOOM_LEVEL; i++)
        if (lodTileSets[i])
            lodTileSets[i]->subdivide(MAX_NODE_SIZE);
    
    baseTileSet.subdivide(MAX_NODE_SIZE);
}

