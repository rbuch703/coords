

#include <assert.h>

#include <iostream>

#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateSequenceFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/geom/IntersectionMatrix.h>

#include "ring.h"


geos::geom::GeometryFactory Ring::factory;  

Ring::Ring(geos::geom::Polygon *geosPolygon, const std::vector<uint64_t> wayIds):
    wayIds(wayIds), geosPolygon(geosPolygon)
{
    MUST(geosPolygon->getNumInteriorRing() == 0, "Not a simple polygon/ring");
    area = this->geosPolygon->getArea();
    MUST( area > 0, "ring without area");
    
}

double Ring::getArea() const
{
    return this->area;
}

void toSimplePolygons( geos::geom::Polygon *poly, std::vector<geos::geom::Polygon*> &polysOut)
{
    if (poly->getNumInteriorRing() == 0)
    {
        if (poly->getExteriorRing()->getNumPoints() >= 4) //only add non-degenerate polygons
            polysOut.push_back(poly);
        else
            delete poly;
            
        return;
    }
    
    polysOut.push_back( Ring::factory.createPolygon( 
        Ring::factory.createLinearRing( poly->getExteriorRing()->getCoordinates() ), nullptr));
        
    
    for (uint64_t i = 0; i < poly->getNumInteriorRing(); i++)
    {
        const geos::geom::LineString *line = poly->getInteriorRingN(i);
        if (line->getNumPoints() < 4) // degenerate case, not a ring
            continue;
            
        polysOut.push_back( Ring::factory.createPolygon( 
            Ring::factory.createLinearRing(line->getCoordinates() ), nullptr));

    }
    
    delete poly;
    
}

std::vector<geos::geom::Polygon*> Ring::createSimplePolygons(const std::vector<OsmGeoPosition> &vertices, uint64_t relId)
{
    geos::geom::CoordinateSequence *seq = factory.getCoordinateSequenceFactory()->create( (size_t)0, 2);    //start with 0 members; each member will have 2 dimensions
    
    for (const OsmGeoPosition &loc : vertices)
        seq->add(geos::geom::Coordinate(loc.lat, loc.lng));
    
    geos::geom::LinearRing *geosRing = factory.createLinearRing( seq );
    
    geos::geom::Geometry *tmp = factory.createPolygon(geosRing, nullptr);
    /* somewhat dirty hack: a buffer of 0.0 is a no-op for valid rings,
       but fixes many types of invalid ones. Unfortunately, it does not
       guarantee that the result is a *single* polygon.*/
    
    geos::geom::Geometry *healed = tmp->buffer(0.0);
    delete tmp;

    if (healed->getGeometryTypeId() == geos::geom::GEOS_POLYGON)
    {
        std::vector<geos::geom::Polygon*> res;
        //toSimplePolygons() takes ownership of 'healed'
        toSimplePolygons(dynamic_cast<geos::geom::Polygon*>(healed), res);
        return res;
    }

    std::cerr << "[WARN] fixing geometry of relation " << relId << " created geometry of type " << healed->getGeometryType() << std::endl;

    std::vector<geos::geom::Polygon*> res;
    
    switch (healed->getGeometryTypeId())
    {
        case geos::geom::GEOS_MULTIPOLYGON: 
            std::cerr << "\tmultipolygon consists of" << std::endl;
            for (uint64_t i = 0; i < healed->getNumGeometries(); i++)
            {
                const geos::geom::Geometry *part = healed->getGeometryN(i);
                std::cerr << "\t\t" << part->getGeometryType();
                if (part->getGeometryTypeId() == geos::geom::GEOS_POLYGON)
                    toSimplePolygons(dynamic_cast<geos::geom::Polygon*>(part->clone()), res);
                else
                    std::cerr << " (skipping)";
                std::cerr << std::endl;
            }
            delete healed;
            return res;
            break;
        default: 
            std::cerr << "\tdon't know how to handle geometry type. Skipping." << std::endl;
            break;
    }    
    
    delete healed;
    return res;
}

/* takes a polygon that potentially has holes, and converts it to a vector of polygon rings */
std::vector<geos::geom::Polygon*> Ring::createRings(const geos::geom::Polygon *poly, uint64_t /*relId*/)
{
    std::vector<geos::geom::Polygon*> rings;
    
    rings.push_back( factory.createPolygon(
                       factory.createLinearRing(
                         poly->getExteriorRing()->getCoordinates()), nullptr));
                           
    for (uint64_t i = 0; i < poly->getNumInteriorRing(); i++)
    {
        rings.push_back( factory.createPolygon(
                           factory.createLinearRing(
                             poly->getInteriorRingN(i)->getCoordinates()), nullptr));
    }
    
    return rings;
}


Ring::~Ring()
{
    /* Note: the geosPolygon is the only GEOS object that needs to be destroyed explicitly.
     *       The created CoordinateSequence is owned by the created LinearRing, which
     *       in turn is owned by the Polygon - and thus all are destroyed along with the Polygon
     */
    delete this->geosPolygon;
}

/*
bool Ring::overlapsWith(const Ring &other) const
{
    
#warning TODO: implement!
    return true;
}*/

#if 0
bool Ring::containsInInterior(const Ring &other) const
{
    /* containsInInterior: 'this' and 'other' must overlap in their
     * interior, but the boundary of 'this' must overlap with
     * neither the interior nor the boundary of 'other'.
     * cf. http://en.wikipedia.org/wiki/DE-9IM */
    static const std::string containsInInteriorMatrix = "T**FF****";

    return geosPolygon->relate( other.geosPolygon, containsInInteriorMatrix);
}

bool Ring::containsAsInner(const Ring &other) const
{
    //std::cout << "beep" << std::endl;
    geos::geom::IntersectionMatrix *mat = geosPolygon->relate( other.geosPolygon );
    //std::string res = mat->toString();
    
    /* 'this' contains 'other', and their boundary overlaps are at most
     * zero-dimensional (i.e. points) */
    bool res = mat->isContains() && (mat->get(1, 1) < 1);
    delete mat;
    //std::cout << "# " << res << std::endl;
    return res;
}
#endif

bool Ring::contains(const Ring &other) const
{
    return this->geosPolygon->contains(other.geosPolygon);
}

bool Ring::boundariesTouch(const Ring &a, const Ring &b)
{
    // cf. http://en.wikipedia.org/wiki/DE-9IM */
    static const std::string touchesMatrix = "****T****";
    return a.geosPolygon->relate( b.geosPolygon, touchesMatrix );
}

bool Ring::interiorIntersectsWith(const Ring &other) const
{
    // cf. http://en.wikipedia.org/wiki/DE-9IM */
    static const std::string interiorsIntersectMatrix = "2********";
    return geosPolygon->relate( other.geosPolygon, interiorsIntersectMatrix);
}


void Ring::serialize(FILE* fOut, bool reverseVertexOrder) const
{
    MUST(this->geosPolygon->getNumInteriorRing() == 0, "Ring cannot have interior rings");
    const geos::geom::LineString* boundary = this->geosPolygon->getExteriorRing();
    const std::vector<geos::geom::Coordinate>& coords = *boundary->getCoordinatesRO()->toVector();
    
    uint32_t numVertices = coords.size();
    MUST(numVertices >= 4, "ring has less than four vertices");
    MUST( fwrite(&numVertices, sizeof(uint32_t), 1, fOut) == 1, "write error");
    
    if (!reverseVertexOrder)
    {
        for (uint64_t i = 0; i < coords.size(); i++)
        {
            int32_t lat = coords[i].x;
            int32_t lng = coords[i].y;
            MUST( fwrite(&lat, sizeof(lat), 1, fOut) == 1, "write error");
            MUST( fwrite(&lng, sizeof(lng), 1, fOut) == 1, "write error");
        }
    } else
    {
        for (int64_t i = coords.size()-1; i >= 0 ; i--)
        {
            int32_t lat = coords[i].x;
            int32_t lng = coords[i].y;
            MUST( fwrite(&lat, sizeof(lat), 1, fOut) == 1, "write error");
            MUST( fwrite(&lng, sizeof(lng), 1, fOut) == 1, "write error");
        }
    }
}

uint64_t Ring::getSerializedSize() const
{
    MUST(this->geosPolygon->getNumInteriorRing() == 0, "Ring cannot have interior rings");
    return sizeof(uint32_t) +   // uint32_t numVertices
           sizeof( int32_t) * 2 * // two int32_t's per vertex (lat, lng)
           this->geosPolygon->getExteriorRing()->getCoordinatesRO()->toVector()->size();
    
}


