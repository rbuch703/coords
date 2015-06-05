
#include "landusePolygonLodHandler.h"

std::set<std::string> LandusePolygonLodHandler::landuses = {
    "park", "forest", "residential", "retail", "commercial", "industrial", "railway",
    "cemetery", "grass", "farmyard", "farm", "farmland", "wood", "meadow", "village_green",
    "recreation_ground", "allotments", "quarry"
};

std::set<std::string> LandusePolygonLodHandler::leisures = 
{
 "park", "garden", "playground", "golf_course", "sports_centre", "pitch", "stadium", "common",
 "nature_reserve"
};

std::set<std::string> LandusePolygonLodHandler::naturals =
{
    "wood", "land", "island", "scrub", "wetland"
};

std::set<std::string> LandusePolygonLodHandler::amenities =
{
    "university", "school", "college", "library", "fuel", "parking", "cinema", "theatre",
    "place_of_worship", "hospital", "grave_yard"
};

LandusePolygonLodHandler::LandusePolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({12, 9, 7, 5});
}


int LandusePolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const
{
    if (!isClosedRing)
        return -1;
        
    if (tags.count("landuse") && landuses.count(tags["landuse"])) return 0;
    if (tags.count("natural") && naturals.count(tags["natural"])) return 0;
    if (tags.count("leisure") && leisures.count(tags["leisure"])) return 0;
    if (tags.count("amenity") && amenities.count(tags["amenity"])) return 0;

    if (tags.count("place") && (tags["place"] == "island" || tags["place"] == "islet"))
        return 0;
        
    if (tags.count("highway") && tags.count("area") && tags["area"] == "yes" &&
        (tags["highway"] == "pedestrian" || tags["highway"] == "footway"))
        return 0;
        
    if (tags.count("tourism") && tags["tourism"] == "zoo")
        return 0;
        
    if (tags.count("aeroway") && 
        (tags["aeroway"] == "aerodrome" || 
         tags["aeroway"] == "helipad" || 
         tags["aeroway"] == "apron" ))
        return 0;
    return -1;
    
}

bool LandusePolygonLodHandler::isArea() const
{
    return true;
}

