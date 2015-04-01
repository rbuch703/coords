

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


Ring::Ring(const std::vector<OsmGeoPosition> &vertices, const std::vector<uint64_t> wayIds):
    wayIds(wayIds)
///*RingSegment *rootSegment, LightweightWayStore &ways*/)
{

//    flatten(rootSegment, ways, this->vertices, this->wayIds, false);

    //static geos::geom::PrecisionModel pmFixed(1.0);
    //geoFactory = new geos::geom::GeometryFactory();
    //std::cerr << pmFixed.getScale() << std::endl;

    geos::geom::CoordinateSequence *seq = factory.getCoordinateSequenceFactory()->create( (size_t)0, 2);
    
    for (const OsmGeoPosition &loc : vertices)
        seq->add(geos::geom::Coordinate(loc.lat, loc.lng));
    
    geos::geom::LinearRing *geosRing = factory.createLinearRing( seq );
    
    geos::geom::Geometry *tmp = factory.createPolygon(geosRing, nullptr);;
    /* somewhat dirty hack: a buffer of 0.0 it a no-op for valid rings,
       but fixes many types of invalid ones. */
    this->geosPolygon = tmp->buffer(0.0);
    delete tmp;
    
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

Ring::Ring(geos::geom::Geometry *geosPolygon, const std::vector<uint64_t> wayIds):
    wayIds(wayIds), geosPolygon(geosPolygon)
{
    area = this->geosPolygon->getArea();
    
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

