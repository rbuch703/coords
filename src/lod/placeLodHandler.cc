
#include <math.h>

#include "placeLodHandler.h"
#include "config.h"

PlaceLodHandler::PlaceLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
//    enableLods({10, 9, 6});
}


int PlaceLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/, double) const
{
    if (tags.count("place"))
    {
        tags.insert( make_pair("type", tags["place"]));
        return 0;
    }

    return -1;
}

/* to establish an order of importance for place labels, labels are usually sorted by
 * population. To emulate this behavior using z-ordering, we store the population as
 * a z-value. Since z-values are only 8 bit wide, they cannot hold the actual population value.
 * Instead, we use the logarithm of the z-value, as it maintains he monotonicity of the
 * original population values, and at the same time compresses huge ranges of values.
 * We currently use the logarithm of the population to the base of 1.2, which allows
 * for populations up to 10 billion (10G) to be stored in a value smaller than 127.
 */
int8_t PlaceLodHandler::getZIndex(const TagDictionary &tags) const
{

    int64_t pop = 0;
    if (tags.count("population"))
        pop = atoll( tags.at("population").c_str());
    
    if (pop < 1)
        pop = 1;
        
    uint64_t popLog = - log(pop) / log(1.2);
    
    return popLog > 127 ? 127 : popLog;
    //return 0;
    
}


bool PlaceLodHandler::isArea() const
{
    return false;
}

