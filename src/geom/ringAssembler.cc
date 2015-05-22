#include <math.h>

#include <iostream>
#include <set>

#include "misc/escapeSequences.h"
#include "ringAssembler.h"

void RingAssembler::addWay( const OsmLightweightWay &way) 
{
    ringSegments.push_back( RingSegment(way));
    
    RingSegment *segment = &ringSegments.back();
    while (true)
    {
        if (segment->isClosed())
        {
            closedRings.push_back(segment);
            break;
        }
    
        if (openEndPoints.count( segment->getStartPosition() ))
        {
            RingSegment *sibling = openEndPoints[segment->getStartPosition()];
            assert( openEndPoints.count( sibling->getStartPosition()) &&
                    openEndPoints.count( sibling->getEndPosition()) );
            openEndPoints.erase( sibling->getStartPosition() );
            openEndPoints.erase( sibling->getEndPosition() );
            ringSegments.push_back( RingSegment(segment, sibling));
            segment = &ringSegments.back();
            //could still be closed in itself or connect to another open segment
            // --> loop again
            continue; 
        }

        if (openEndPoints.count( segment->getEndPosition() ))
        {
            RingSegment *sibling = openEndPoints[segment->getEndPosition()];
            assert( openEndPoints.count( sibling->getStartPosition()) &&
                    openEndPoints.count( sibling->getEndPosition()));
            openEndPoints.erase( sibling->getStartPosition() );
            openEndPoints.erase( sibling->getEndPosition() );
            ringSegments.push_back( RingSegment(segment, sibling));
            segment = &ringSegments.back();
            //could still be closed in itself or connect to another open segment
            // --> loop again
            continue; 
        }
        
        /* if this point is reached, then 'segment' is not closed in itself,
         * and cannot be connected directly to any RingSegment in openEndPoints.
         * So register its two endpoints as two additional open endpoints*/
         
         openEndPoints.insert( std::make_pair( segment->getStartPosition(), segment));
         openEndPoints.insert( std::make_pair( segment->getEndPosition(), segment));
         break;
    }
}

void RingAssembler::warnUnconnectedNodes(uint64_t relId) const 
{
    if (openEndPoints.size())
    {
        std::cerr << "[WARN] multipolygon relation " << relId << " has unconnected nodes: " << std::endl;
        for ( const std::pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
        {
            std::cerr << ESC_FG_GRAY << "\t- node " << kv.first.id << " at way sequence ";
            for (uint64_t wayId : kv.second->getWayIdsRecursive())
                std::cerr << wayId << ", ";
            std::cerr << ESC_RESET << std::endl;
        }
    }
}

void RingAssembler::tryCloseOpenSegments( double maxConnectionDistance)
{
    while (openEndPoints.size() > 0)
    {
        MUST(openEndPoints.size() % 2 == 0, "geometric bug - odd number of segment end points");
        OsmGeoPosition ep1, ep2;
        double minDist = INFINITY;
        for (const std::pair<OsmGeoPosition, RingSegment*> &p1 : openEndPoints)
            for (const std::pair<OsmGeoPosition, RingSegment*> &p2 : openEndPoints)
            {
                if (p1.first == p2.first) continue;
                double dLat = p1.first.lat - p2.first.lat;
                double dLng = p1.first.lng - p2.first.lng;
                double dist = sqrt( dLat*dLat + dLng*dLng);
                if (dist < minDist)
                {
                    minDist = dist;
                    ep1 = p1.first;
                    ep2 = p2.first;
                }
            }

        if (minDist > maxConnectionDistance)
        {
            /*std::cerr << ESC_FG_RED << "\tclosest connection would be " 
                      << minDist/maxConnectionDistance << " times further apart then allowed"
                      << ESC_FG_RESET << std::endl;*/
            break;  //no more matches possible
        }
            
        std::cerr << ESC_FG_GREEN << "\tfound end point pair with distance " << minDist 
                  << ESC_RESET << std::endl;
        
        MUST( openEndPoints.count(ep1) && openEndPoints.count(ep2), "invalid endpoint");
        
        RingSegment *rs1 = openEndPoints.at(ep1);
        RingSegment *rs2 = openEndPoints.at(ep2);

        openEndPoints.erase( rs1->getStartPosition());
        openEndPoints.erase( rs1->getEndPosition());

        if (rs1 == rs2) //connecting the end poitns will close a ring
        {
            /* target sequence: --- rs1 --- rsArtificial --- rs1 --- */
            /* target hierarchy:
             *
             *  rsMerged   
             *     /\                
             *    /  \               
             *  rs1  rsArtificial
             */

            ringSegments.push_back( RingSegment(ep1, ep2, -2)); //rsArtificial
            ringSegments.push_back( RingSegment(rs1, &ringSegments.back())); //rsMerged
            RingSegment *rsMerged = &ringSegments.back();

            MUST( rsMerged->isClosed(), "geometry bug");
            closedRings.push_back(rsMerged);

        }
        else
        {//connecting the end points will form a larger, still unclosed segment
        
            openEndPoints.erase( rs2->getStartPosition());
            openEndPoints.erase( rs2->getEndPosition());

            /* target sequence: --- rs1 --- rsArtificial --- rs2 --- */
            
            /* target hierarchy
             *        ----rsM2----
             *       /            \
             *      /              \
             *    rsM1              \
             *     /\                \
             *    /  \                \
             *  rs1  rsArtificial     rs2
             */
            //add the 'artificial' segment            
            ringSegments.push_back( RingSegment(ep1, ep2, -2)); //rsArtificial
            ringSegments.push_back( RingSegment(rs1, &ringSegments.back())); //rsM1
            ringSegments.push_back( RingSegment(rs2, &ringSegments.back())); //rsM2
                
            RingSegment *rsM2 = &ringSegments.back();

            MUST( !rsM2->isClosed(), "geometry bug")
            openEndPoints.insert(std::make_pair( rsM2->getStartPosition(), rsM2));
            openEndPoints.insert(std::make_pair( rsM2->getEndPosition(),   rsM2));

        }


    }
}

const std::vector<RingSegment*> &RingAssembler::getClosedRings() const 
{ 
    return this->closedRings;
}

bool RingAssembler::hasOpenRingSegments() const 
{ 
    return openEndPoints.size() > 0; 
}

double RingAssembler::getAABBDiameter( std::map<uint64_t, OsmLightweightWay> wayStore) const 
{
    MUST(ringSegments.size() > 0, "cannot determine diameter of empty RingSegment set");
    
    int32_t latMin = ringSegments.front().getStartPosition().lat;
    int32_t latMax = ringSegments.front().getStartPosition().lat;

    int32_t lngMin = ringSegments.front().getStartPosition().lng;
    int32_t lngMax = ringSegments.front().getStartPosition().lng;
    
    for (const RingSegment &seg : ringSegments)
    {
        int64_t wayId = seg.getWayId();
        if (wayId <= 0) //not a valid way reference
            continue;
            
        if (!wayStore.count(wayId))
            continue;
            
        OsmLightweightWay way = wayStore[wayId];
        for (int i = 0; i < way.numVertices; i++)
        {
            if (way.vertices[i].lat > latMax) latMax = way.vertices[i].lat;
            if (way.vertices[i].lat < latMin) latMin = way.vertices[i].lat;

            if (way.vertices[i].lng > lngMax) lngMax = way.vertices[i].lng;
            if (way.vertices[i].lng < lngMin) lngMin = way.vertices[i].lng;
        }
    }
    
    double dLat = latMax - latMin;
    double dLng = lngMax - lngMin;
    
    return sqrt( dLat*dLat + dLng*dLng);
}

static bool isValidWay(OsmRelationMember mbr, uint64_t relationId, const std::map<uint64_t, OsmLightweightWay> &ways)
{
    if (mbr.type != OSM_ENTITY_TYPE::WAY)
    {
        std::cerr << ESC_FG_GRAY << "[INFO]" << " ref to id=" << mbr.ref << " of invalid type '"
             << (mbr.type == OSM_ENTITY_TYPE::NODE ? "node" : 
                 mbr.type == OSM_ENTITY_TYPE::RELATION ? "relation" : "<unknown>" )
             << "' in multipolygon relation " << relationId << ESC_RESET << std::endl;
        return false;
    }
    
    if (! ways.count(mbr.ref))
    {
        std::cerr << "[WARN] relation " << relationId << " references way " << mbr.ref 
             << ", which is not part of the data set" << std::endl;
        return false;
    }
    

    if (ways.at(mbr.ref).numVertices < 1)
    {
        std::cerr << "[WARN] way " << mbr.ref << " has no member nodes; ignoring it" 
                  << std::endl;
        return false;
    }
    
    return true;
}


RingAssembler RingAssembler::fromRelation( OsmRelation &rel, 
            const std::map<uint64_t, OsmLightweightWay> &ways, 
            std::map<std::string, std::string> &outerTagsOut)
{
    RingAssembler ringAssembler;
    
    std::set<uint64_t> waysAdded;
    uint64_t numOuterWays = 0;

    for ( const OsmRelationMember &mbr : rel.members)
        if (isValidWay( mbr, rel.id, ways))
        {
            if (waysAdded.count(mbr.ref))
            {
                std::cerr << ESC_FG_YELLOW << "[WARN] relation " << rel.id << " contains way "
                     << mbr.ref << " multiple times. Skipping all but the first "
                     << "occurrence." << ESC_RESET << std::endl;
                continue;
            }
            
            OsmLightweightWay way = ways.at(mbr.ref);
            if (mbr.role == "outer")
            {
                outerTagsOut = way.getTags().asDictionary();
                numOuterWays += 1;
            }
            ringAssembler.addWay(way);
            waysAdded.insert(mbr.ref);
        }

    if (numOuterWays > 1)
        outerTagsOut.clear(); // no unique outer way
        
    return ringAssembler;
}

