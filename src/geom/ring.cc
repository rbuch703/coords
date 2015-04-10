

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

#if 0
Ring::Ring(const std::vector<OsmGeoPosition> &vertices, const std::vector<uint64_t> wayIds):
    wayIds(wayIds)
///*RingSegment *rootSegment, LightweightWayStore &ways*/)
{

//    flatten(rootSegment, ways, this->vertices, this->wayIds, false);

    //static geos::geom::PrecisionModel pmFixed(1.0);
    //geoFactory = new geos::geom::GeometryFactory();
    //std::cerr << pmFixed.getScale() << std::endl;

    
    /*area = 0.0;
    for (uint64_t i = 0; i < vertices.size()-1; i++)
    {
            area += 0.5 * ( vertices[i].lat * vertices[i+1].lng -
                            vertices[i].lng * vertices[i+1].lat );
    }
    area += 0.5 * (vertices[vertices.size()-1].lat * vertices[0].lng -
                   vertices[vertices.size()-1].lng * vertices[0].lat );*/
    
    area = this->geosPolygon->getArea();
    //std::cout << "area: " << area << std::endl;
}
#endif
Ring::Ring(geos::geom::Polygon *geosPolygon, const std::vector<uint64_t> wayIds):
    wayIds(wayIds), geosPolygon(geosPolygon)
{
    assert(geosPolygon->getNumInteriorRing() == 0 || "Not a simple polygon/ring");
    area = this->geosPolygon->getArea();
    
}

std::vector<geos::geom::Polygon*> Ring::createSimplePolygons(const std::vector<OsmGeoPosition> &vertices, uint64_t relId)
{
    geos::geom::CoordinateSequence *seq = factory.getCoordinateSequenceFactory()->create( (size_t)0, 2);    //start with 0 members; each member will have 2 dimensions
    
    for (const OsmGeoPosition &loc : vertices)
        seq->add(geos::geom::Coordinate(loc.lat, loc.lng));
    
    geos::geom::LinearRing *geosRing = factory.createLinearRing( seq );
    
    geos::geom::Geometry *tmp = factory.createPolygon(geosRing, nullptr);;
    /* somewhat dirty hack: a buffer of 0.0 is a no-op for valid rings,
       but fixes many types of invalid ones. Unfortunately, it does not
       guarantee that the result is a *single* polygon.*/
    
    geos::geom::Geometry *healed = tmp->buffer(0.0);
    delete tmp;

    if (healed->getGeometryTypeId() == geos::geom::GEOS_POLYGON)
    {
        std::vector<geos::geom::Polygon*> res;
        res.push_back(dynamic_cast<geos::geom::Polygon*>(healed));
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
                    res.push_back(dynamic_cast<geos::geom::Polygon*>(part->clone()));
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
    
    return res;
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

bool Ring::contains(const Ring &other) const
{
    return this->geosPolygon->contains(other.geosPolygon);
}


bool Ring::interiorIntersectsWith(const Ring &other) const
{
    // cf. http://en.wikipedia.org/wiki/DE-9IM */
    static const std::string interiorsIntersectMatrix = "2********";
    return geosPolygon->relate( other.geosPolygon, interiorsIntersectMatrix);
}

