
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>

#include "osm/osmMappedTypes.h"
#include "geom/ringSegment.h"
#include "geom/ring.h"

using namespace std;



int main()
{
    RelationStore relStore("intermediate/relations");
    LightweightWayStore wayStore("intermediate/ways");
    
    for (const OsmRelation &rel : relStore)
    //int dummy = 0;
    //for (OsmRelation rel = relStore[60198]; dummy == 0; dummy++)
    {
        //cout << "processing relation " << rel << endl;
        map<string, string> tags(rel.tags.begin(), rel.tags.end());;
        if ((! tags.count("type")) || (tags["type"] != "multipolygon"))
            continue;
        
        /* cannot be a std::vector, because we use pointers into this container,
         * and pointers into a vector are not guaranteed to be stable when adding
         * or removing elements from/to it (causing a resize of the underlying array) */
        list<RingSegment> ringSegments;
        vector<RingSegment*> closedRings;
        
        map<OsmGeoPosition, RingSegment*> openEndPoints;
        
        for ( const OsmRelationMember &mbr : rel.members)
        {
            if (mbr.type != WAY)
            {
                cerr << "[WARN] ref to id=" << mbr.ref << " of invalid type '"<< (mbr.type == NODE ? "node" : mbr.type == RELATION ? "relation" : "<unknown>")<<"' in multipolygon relation " << rel.id << endl;
                continue;
            }
            
            if (! wayStore.exists(mbr.ref))
            {
                cerr << "[WARN] relation " << rel.id << " reference way " << mbr.ref << ", which is not part of the data set" << endl;
                continue;
            }
            
            OsmLightweightWay way = wayStore[mbr.ref];
            if (way.numVertices < 1)
            {
                cerr << "[WARN] way " << way.id << " has no member nodes; ignoring it" << endl;
                continue;
            }
            
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
                 
                 openEndPoints.insert( make_pair( segment->getStartPosition(), segment));
                 openEndPoints.insert( make_pair( segment->getEndPosition(), segment));
                 break;
            }
            
        }

        if (openEndPoints.size())
        {
            cerr << "[WARN] multipolygon relation " << rel.id << " has open way end points. Will ignore all affected open rings: " << endl;
            for ( const pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
            {
                cerr << "\tnode " << kv.first.id << " (" << (kv.first.lat/10000000.0) << "°, " << (kv.first.lng/10000000.0) << "°)" << endl;
            }
        }
        
        for (RingSegment* rootSegment : closedRings)
        {
            Ring ring(rootSegment, wayStore);
            //cout << "flattened " << ring.wayIds.size() << " ways to list of " << ring.vertices.size() << " vertices" << endl;
/*            for (OsmGeoPosition pos : vertices)
            {
                cout << pos.lat << ", " << pos.lng << endl;
            }
            
            exit(0);*/
        }
        //cout << rel.members.size() << ", " << rel << endl;
    }


    return EXIT_SUCCESS;
}
