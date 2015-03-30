
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>

#include <osm/osmMappedTypes.h>


using namespace std;

class RingSegment
{
public:
    RingSegment(const OsmGeoPosition &start, const OsmGeoPosition &end):
        start(start), end(end), child1(nullptr), child2(nullptr), 
        isReversed(false)//, removeEnd1(false)
    {
    }
    
    RingSegment( const OsmLightweightWay &way):
        wayId(way.id), child1(nullptr), child2(nullptr), isReversed(false)
    {
        MUST(way.numVertices > 0, "cannot create ring from way without members");
        start = way.vertices[0];
        end   = way.vertices[way.numVertices - 1];
    }

    RingSegment( RingSegment *pChild1, RingSegment *pChild2):
        wayId(-1), child1(pChild1), child2(pChild2), isReversed(false) 
    {
        
            if (child2->getEndPosition() == child1->getStartPosition() ||
                child2->getEndPosition() == child1->getEndPosition())
                child2->reverse();
            
            /* now (the) one of the end points of child2 that can be connected to
             * child1 is the 'start' position of child2 */
            
            if (child1->getStartPosition() == child2->getStartPosition())
                child1->reverse();
                
            MUST( child1->getEndPosition() == child2->getStartPosition(), "invalid geometry processing");
            
                       
        start = child1->getStartPosition();
        end   = child2->getEndPosition();
    }
    bool isClosed() const { return (start == end); }
    OsmGeoPosition getStartPosition() const { return isReversed ? end    : start; }
    OsmGeoPosition getEndPosition()   const { return isReversed ? start  : end;   }
    RingSegment*   getFirstChild()    const { return isReversed ? child2 : child1;}
    RingSegment*   getSecondChild()   const { return isReversed ? child1 : child2;}

private:
    void reverse() { isReversed = !isReversed; }

private:
    /* id of the OSM way this ring segment is based on (in which case the ring segment must
       not have children); or -1 if the ring segment is not directly based on a single OSM way
       and does have children */
    int64_t wayId; 
    OsmGeoPosition start;
    OsmGeoPosition end;
    
    /* NOTE: RingSegment does *not* take ownership of the child RingSegments it points to.
     *       Those have to be delete'ed separately */
    RingSegment *child1, *child2; 
    bool isReversed;       // reverse node order before connecting to its sibling

    //bool removeEnd;     // remove end node  when connecting to the sibling 
                        // (because it's a duplicate from its sibling node);
                        // 'end' refers to the last node *after* a possible
                        // reversal (see above).

};


int main()
{
    RelationStore relStore("intermediate/relations");
    LightweightWayStore wayStore("intermediate/ways");
    
    for (const OsmRelation &rel : relStore)
    {
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
            cerr << "[WARN] multipolygon relation " << rel.id << " has open way end points: " << endl;
            for ( const pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
            {
                cerr << "\t" << kv.first.id << " (" << kv.first.lat << ", " << kv.first.lng << ")" << endl;
            }
            
        }

        //cout << rel.members.size() << ", " << rel << endl;
    }


    return EXIT_SUCCESS;
}
