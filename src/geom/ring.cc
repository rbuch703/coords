

#include <assert.h>

#include <iostream>

#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateSequenceFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/geom/IntersectionMatrix.h>

#include "ring.h"
#include "misc/escapeSequences.h"
#include "misc/varInt.h"


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

void Ring::deleteRecursive(Ring* ring)
{
    for (Ring *child : ring->children)
        Ring::deleteRecursive(child);
        
    delete ring;
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
    
    MUST(coords.size() >= 4, "ring has less than four vertices");
    varUintToFile(coords.size(), fOut);

    int64_t prevX = 0;
    int64_t prevY = 0;
    if (!reverseVertexOrder)
    {
        for (uint64_t i = 0; i < coords.size(); i++)
        {
            int64_t dX = coords[i].x - prevX;
            int64_t dY = coords[i].y - prevY;
            prevX = coords[i].x;
            prevY = coords[i].y;
            varIntToFile(dX, fOut);
            varIntToFile(dY, fOut);
        }
    } else
    {
        for (int64_t i = coords.size()-1; i >= 0; i--)
        {
            int64_t dX = coords[i].x - prevX;
            int64_t dY = coords[i].y - prevY;
            prevX = coords[i].x;
            prevY = coords[i].y;
            varIntToFile(dX, fOut);
            varIntToFile(dY, fOut);
        }
    }
}

uint64_t Ring::getSerializedSize(bool reverseVertexOrder) const
{
    MUST(this->geosPolygon->getNumInteriorRing() == 0, "Ring cannot have interior rings");
    const std::vector<geos::geom::Coordinate> &coords = 
        *this->geosPolygon->getExteriorRing()->getCoordinatesRO()->toVector();

    uint64_t size = varUintNumBytes( coords.size() ); //size of 'numVertices' field;
    
    int64_t prevX = 0;
    int64_t prevY = 0;
    if (!reverseVertexOrder)
    {
        for (uint64_t i = 0; i < coords.size(); i++)
        {
            int64_t dX = coords[i].x - prevX;
            int64_t dY = coords[i].y - prevY;
            prevX = coords[i].x;
            prevY = coords[i].y;
            size += varIntNumBytes( dX );   //size of the delta-encoded vertices
            size += varIntNumBytes( dY );
        }
    } else
    {
        for (int64_t i = coords.size() - 1; i >= 0; i--)
        {
            int64_t dX = coords[i].x - prevX;
            int64_t dY = coords[i].y - prevY;
            prevX = coords[i].x;
            prevY = coords[i].y;
            size += varIntNumBytes( dX );
            size += varIntNumBytes( dY );
        }
    }
    
    return size;
}

const geos::geom::Polygon* Ring::getPolygon() const 
{ 
    return this->geosPolygon; 
}

static std::ostream& operator<<( std::ostream &os, const std::vector<uint64_t> &nums)
{
    for (uint64_t i = 0; i < nums.size(); i++)
    {
        if (i > 0)
            os << ", ";
        os << nums[i];
    }

    return os;
}

static std::vector<Ring*> getRings( geos::geom::Geometry* geometry, 
                                    std::vector<uint64_t> wayIds, 
                                    uint64_t relId)
{
    std::vector<Ring*> rings;
    //cerr << "#" << relId << " - " << geometry->getGeometryType() << endl;
    MUST( geometry->getGeometryTypeId() == geos::geom::GEOS_POLYGON ||
          geometry->getGeometryTypeId() == geos::geom::GEOS_MULTIPOLYGON,
          "geometric join failed");
          
    if (geometry->getGeometryTypeId() == geos::geom::GEOS_POLYGON)
    {
        for (geos::geom::Polygon *poly : 
                Ring::createRings( dynamic_cast<geos::geom::Polygon*>(geometry), relId))
            rings.push_back( new Ring(poly, wayIds));
    } else
    {
        MUST(geometry->getGeometryTypeId() == geos::geom::GEOS_MULTIPOLYGON, 
             "logic error");

        auto collection = dynamic_cast<geos::geom::GeometryCollection*>(geometry);
        for (uint64_t i = 0; i < collection->getNumGeometries(); i++)
        {
            MUST( collection->getGeometryN(i)->getGeometryTypeId() ==
                  geos::geom::GEOS_POLYGON, "geometric difference failed")
            
            auto poly = dynamic_cast<const geos::geom::Polygon*>(collection->getGeometryN(i));

            for (geos::geom::Polygon *pRing : Ring::createRings( poly, relId))
                rings.push_back( new Ring(pRing, wayIds));
        }
         
    }
    
    return rings;
    
}

std::vector<Ring*> Ring::merge( const Ring* ring1, const Ring* ring2, 
                          const geos::geom::IntersectionMatrix *mat, uint64_t relId)
{
    
    //std::cerr << ;
            
    geos::geom::Geometry* joined;           
    if (mat->isWithin())    
    {
        std::cerr << ESC_FG_YELLOW << "[WARN] relation " << relId << ": merging rings "
         << "(" << ring1->wayIds << ") and (" << ring2->wayIds << ")" 
         << " as 'difference B-A'" << ESC_RESET << std::endl;
        /* if 'ring' lies completely within 'ring2', but they share an outer edge.
         * 'ring' is likely to be supposed to be subtracted */
        joined = ring2->getPolygon()->difference(ring1->getPolygon());
    } else if (mat->isContains()) // 'ring
        /* same with 'ring2' being inside 'ring' */
    {
        std::cerr << ESC_FG_YELLOW << "[WARN] relation " << relId << ": merging rings "
         << "(" << ring1->wayIds << ") and (" << ring2->wayIds << ")" 
         << " as 'difference A-B'" << ESC_RESET << std::endl;
        joined = ring1->getPolygon()->difference(ring2->getPolygon());
    } else
        /* generic overlap: either both rings only share an edge (but no interior),
           or both rings overlap partially, with no ring being completely inside 
           the other.
           In this case, the supposed geometric interpretation is likely a union.*/
    {
        if (mat->isOverlaps(2,2))
            std::cerr << ESC_FG_YELLOW << "[WARN] relation " << relId << ": merging rings "
         << "(" << ring1->wayIds << ") and (" << ring2->wayIds << ")" 
         << " as 'union of overlaps'" << ESC_RESET  << std::endl;
         
        // non-overlapping adjacent (inner) rings are allowed by the OSM multipolygon spec
        //else
        //    std::cerr << "'union'"  << ESC_RESET << std::endl;

        joined = ring1->getPolygon()->Union(ring2->getPolygon());
        MUST( joined->getGeometryTypeId() == geos::geom::GEOS_POLYGON, 
              "geometric join failed");
    }

    std::vector<uint64_t> wayIds = ring1->wayIds;
    wayIds.insert(wayIds.end(), ring2->wayIds.begin(), ring2->wayIds.end());

    std::vector<Ring*> mergeResult = getRings(joined, wayIds, relId);
    delete joined;    
    
    return mergeResult;
}

void Ring::flattenHierarchyToPolygons(std::vector<Ring*> &roots)
{
    
    for (uint64_t i = 0; i < roots.size(); i++)
    {
        Ring *outer = roots[i];
        
        for (Ring* inner : outer->children)
        {
            roots.insert(roots.end(), inner->children.begin(), inner->children.end());
            inner->children.clear();
        }
    }
}

void Ring::insertIntoHierarchy( std::vector<Ring*> &hierarchyRoot, uint64_t relId)
{
    for (uint64_t i = 0; i< hierarchyRoot.size(); i++)
    {
        Ring* root = hierarchyRoot[i];
        
        /* note: contains() only means that no part of 'ring' lies outside of 'root'.
                 This is less strict than that the OGC multipolygon requirements (which
                 demand - among others - that the inner ring touches the outer one 
                 at most at a finite number of points, while contains() allows touching
                 edges). But contains() is good enough for now to establish a hierarchy.
                 The edge cases (like touching edges) need to be removed in a later 
                 post-processing step (e.g. through CSG subtraction).*/
        if ( root->contains(*this) )
            return this->insertIntoHierarchy( root->children, relId);

        if ( root->interiorIntersectsWith(*this))
        {
            std::cerr << ESC_FG_YELLOW << "[WARN] overlapping rings found in relation " 
                      << relId << ", merging them." << ESC_RESET << std::endl;
            std::cerr << "\tRing 1: ways " << root->wayIds << std::endl;
            std::cerr << "\tRing 2: ways " << this->wayIds << std::endl;

            std::vector<uint64_t> wayIds( root->wayIds.begin(), root->wayIds.end());
            wayIds.insert( wayIds.end(), this->wayIds.begin(), this->wayIds.end() );

            std::cerr << ESC_FG_RED << "Merging overlapping rings in relation " << relId 
                      << ESC_RESET << std::endl;
            geos::geom::Geometry *merged = root->getPolygon()->Union(this->getPolygon());
            MUST( merged->getGeometryTypeId() == geos::geom::GEOS_POLYGON, "merge error");

            /* unions cannot reduce the area, and a reduction would void this algorithm, as
             * it assumes that rings are inserted sorted descending by area. We still allow
             * for a very small relative reduction to account for numerical inaccuracies */
            double relativeArea = merged->getArea() / root->getArea();
            MUST( relativeArea >= 0.99999999999999 , "union reduced area");

            Ring *mergedRing = new Ring( dynamic_cast<geos::geom::Polygon*>(merged), wayIds);
            MUST( this->children.size() == 0, "simple ring cannot have children");
            mergedRing->children = root->children;

            /* These assertions should be true geometrically, but can fail in GEOS due to
             * numerical inaccuracies */
            /*MUST( merged->covers(root->geosPolygon), "merge error");
            MUST( merged->covers(ring->geosPolygon), "merge error");*/
            
            hierarchyRoot[i] = mergedRing;

            delete root;
            delete this;
            
            return;
        }
    }

    //no overlap found --> add ring at this level
    hierarchyRoot.push_back(this);
   

}

