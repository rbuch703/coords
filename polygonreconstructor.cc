
#include "polygonreconstructor.h"

#include <boost/foreach.hpp>

#include "osm_types.h"

#include <set>
#include <queue>

#include <iostream>
#include <assert.h>

typedef pair<Vertex, PolygonSegment*> Vertex2Segment;

PolygonSegment* PolygonReconstructor::add(const PolygonSegment &s)
{
    PolygonSegment *seg = new PolygonSegment(s);

      //if there is another segment that connects seemlessly  to this one
    while ( openEndPoints.count(seg->front()) || openEndPoints.count(seg->back()))
    {
        if (openEndPoints.count(seg->front())) //other polygon connects to 'front' of this one
        {
            PolygonSegment *other = openEndPoints[seg->front()];
            openEndPoints.erase( other->front());
            openEndPoints.erase( other->back());
            
            if (other->front() == seg->front()) 
            {
                seg->reverse(); //have to fit front-to-back
                assert( seg->back() == other->front() );
                seg->append(*other, true);
            } else
            {
                assert( other->back() == seg->front() );
                other->append(*seg, true);
                *seg = *other;
            }
            delete other;
        }
        
        if (openEndPoints.count(seg->back())) //other polygon connects to 'back' of this one
        {
            PolygonSegment *other = openEndPoints[seg->back()];
            openEndPoints.erase( other->front());
            openEndPoints.erase( other->back());
            
            if (other->back() == seg->back()) 
            {
                seg->reverse();
                assert( other->back() == seg->front());
                other->append(*seg, true);
                *seg = *other;
            } else
            {
                assert( seg->back() == other->front() );
                seg->append(*other, true);
            }
            delete other;
        }
    } 

    if ( seg->front() == seg->back())   //if the segment itself is a closed polygon
    { 
        res.push_back(*seg); 
        delete seg; 
        return &res.back();
    } else
    {
        assert( seg->front() != seg->back() );
        
        assert ( openEndPoints.count(seg->front()) == 0);
        assert ( openEndPoints.count(seg->back()) == 0 );
        openEndPoints[seg->front()] = seg;
        openEndPoints[seg->back()] = seg;
    }
    return NULL;
}

#if 0
void PolygonReconstructor::forceClosePolygons()
{
    /** at this point, all polygons that can be closed have been closed.
      * of the remaining segments, we attempt to find segments that are as close together as possible*/
    uint32_t nEndPoints = openEndPoints.size();
    Vertex vEndPoints[nEndPoints];
    uint32_t i = 0;
    //for (map<Vertex, PolygonSegment*>::const_iterator poly = openEndPoints.begin(); poly != openEndPoints.end(); poly++, i++)
    //    vEndPoints[i] = poly->first;
    
    BOOST_FOREACH( Vertex2Segment poly, openEndPoints)
        vEndPoints[i++] = poly.first;
        
    while (nEndPoints)
    {
        uint64_t minDist = 0xFFFFFFFFFFFFFFFFll;
        int min_i=1, min_j=0;
        for (i = 1; i < nEndPoints; i++)
            for (uint32_t j = 0; j < i; j++)
            {
                uint64_t dist = (vEndPoints[i] - vEndPoints[j]).squaredLength( );
                if (dist < minDist) { minDist = dist; min_i = i; min_j = j;}
            }
        assert(minDist != 0xFFFFFFFFFFFFFFFFll);
        //cout << "Minimum gap size is " << (sqrt(minDist)/100) << "m" << endl;
        assert( openEndPoints.count(vEndPoints[min_i]) && openEndPoints.count(vEndPoints[min_j]) );
        PolygonSegment* seg1 = openEndPoints[vEndPoints[min_i]];
        PolygonSegment* seg2 = openEndPoints[vEndPoints[min_j]];
        
        Vertex v1 = vEndPoints[min_i];
        Vertex v2 = vEndPoints[min_j];

        i = 0;
        while (i < nEndPoints)
        {
            if (vEndPoints[i] == seg1->front() || vEndPoints[i] == seg1->back() ||
                vEndPoints[i] == seg2->front() || vEndPoints[i] == seg2->back()   )
                {
                    --nEndPoints;
                    vEndPoints[i] = vEndPoints[nEndPoints];
                }
            else i++;
        }
        
        if (seg1 == seg2) // same polygon segment --> close the loop
        {
            //cout << "closing gap of  " 
            //     << sqrt( (seg1->back() - seg1->front()).squaredLength())/100.0 << "m"<< endl;
        
            openEndPoints.erase( seg1->front());
            openEndPoints.erase( seg1->back());
            seg1->append(seg1->front()); // to really make it closed
            res.push_back(*seg1);
            delete seg1;
        } else
        {
            openEndPoints.erase( seg1->front());
            openEndPoints.erase( seg1->back());
            openEndPoints.erase( seg2->front());
            openEndPoints.erase( seg2->back());
                
            if      (v1 == seg1->front() && v2 == seg2->front()) { seg1->reverse(); seg1->append(*seg2, false); }
            else if (v1 == seg1->back()  && v2 == seg2->front()) {                  seg1->append(*seg2, false); }
            else if (v1 == seg1->front() && v2 == seg2->back() ) { seg2->append(*seg1, false); *seg1 = *seg2;   }
            else if (v1 == seg1->back()  && v2 == seg2->back() ) { seg2->reverse(); seg1->append(*seg2, false); }
            else assert(false);
            
            delete seg2;
            assert ( openEndPoints.count(seg1->front()) == 0);
            assert ( openEndPoints.count(seg1->back() ) == 0);
            openEndPoints[seg1->front()] = seg1;
            openEndPoints[seg1->back()]  = seg1;
            vEndPoints[nEndPoints++] = seg1->front();
            vEndPoints[nEndPoints++] = seg1->back();

        }    
    }
    assert(openEndPoints.size() == 0);
}
#endif
struct EndPointDistance
{
    EndPointDistance(uint64_t i, uint64_t j, BigInt dist): m_i(i), m_j(j), m_dist(dist) {}
    uint64_t m_i,m_j;
    BigInt m_dist;
    
    bool operator<(const EndPointDistance &other) const { return m_dist > other.m_dist; }
};


static void closeSomePolygons( map<Vertex, PolygonSegment*> &openEndPoints, list<PolygonSegment> &res, const uint64_t max_dist_sq)
{
    uint64_t nEndPoints = openEndPoints.size();

    //for each segment end point, stores whether the end point is still free to be connected (=true)
    //(or whether it has already been connected to another end point (=false)
    bool vEndPointAccessible[nEndPoints];
    for (uint64_t i = 0; i < nEndPoints; i++) 
        vEndPointAccessible[i] = true;

    Vertex vEndPoints[nEndPoints];
    uint64_t i = 0;
    BOOST_FOREACH( Vertex2Segment v1, openEndPoints)
        vEndPoints[i++] = v1.first;

    priority_queue<EndPointDistance> queue;
        
    for (uint64_t i = 1; i < nEndPoints; i++)
    {
        if (!vEndPointAccessible[i]) continue;
        //cout << "queue size: " << queue.size() << endl;
        for (uint64_t j = 0; j < i; j++)
        {
            //Vertex vDist(.x, vEndPoints[i].y - vEndPoints[j].y);
            uint64_t dist_sq = (uint64_t)(vEndPoints[i] - vEndPoints[j]).squaredLength();
            if (vEndPointAccessible[j] && dist_sq <= max_dist_sq)
                queue.push( EndPointDistance(i, j, dist_sq ));
        }
    }
    //cout << queue.size() << " candidates" << endl;


    while (queue.size())
    {
        EndPointDistance nextPair = queue.top();
        queue.pop();
        //if one of the end points has already been connected, this pair is obsolete
        if (! (vEndPointAccessible[nextPair.m_i] && vEndPointAccessible[nextPair.m_j])) continue;
        
        assert( openEndPoints.count(vEndPoints[nextPair.m_i]) && openEndPoints.count(vEndPoints[nextPair.m_j]));

        Vertex v1 = vEndPoints[nextPair.m_i];
        Vertex v2 = vEndPoints[nextPair.m_j];

        PolygonSegment* seg1 = openEndPoints[v1];
        PolygonSegment* seg2 = openEndPoints[v2];
        
        //we now proceed to connect the two end points, so they are no longer accessible
        vEndPointAccessible[nextPair.m_i] = vEndPointAccessible[nextPair.m_j] = false;
        
        if (seg1 == seg2) // same polygon segment --> close the loop
        {
            //cout << "closing gap of  " 
            //     << sqrt( (seg1->back() - seg1->front()).squaredLength())/100.0 << "m"<< endl;
        
            openEndPoints.erase( seg1->front());
            openEndPoints.erase( seg1->back());
            seg1->append(seg1->front()); // to really be closed, it has to start and end with the same vertex
            res.push_back(*seg1);
            delete seg1;
            continue;
        }

        openEndPoints.erase( seg1->front());
        openEndPoints.erase( seg1->back());
        openEndPoints.erase( seg2->front());
        openEndPoints.erase( seg2->back());
            
        if      (v1 == seg1->front() && v2 == seg2->front()) { seg1->reverse(); seg1->append(*seg2, false); }
        else if (v1 == seg1->back()  && v2 == seg2->front()) {                  seg1->append(*seg2, false); }
        else if (v1 == seg1->front() && v2 == seg2->back() ) { seg2->append(*seg1, false); *seg1 = *seg2;   }
        else if (v1 == seg1->back()  && v2 == seg2->back() ) { seg2->reverse(); seg1->append(*seg2, false); }
        else assert(false);
        
        delete seg2;
        assert ( openEndPoints.count(seg1->front()) == 0);
        assert ( openEndPoints.count(seg1->back() ) == 0);
        openEndPoints[seg1->front()] = seg1;
        openEndPoints[seg1->back()]  = seg1;
    }

}

void PolygonReconstructor::forceClosePolygons()
{
   
    /** at this point, all polygons that can be closed have been closed.
      * of the remaining segments, we attempt to find segments that are as close together as possible*/
        
    for (uint64_t max_dist = 1; openEndPoints.size() > 0; max_dist*=10)
    {
        //std::cout << "connecting polygon segments that are at most " << (max_dist/100) << "m apart" << endl;
        closeSomePolygons( openEndPoints, res, max_dist * max_dist);
        //std::cout << (openEndPoints.size()/2) << " open segments left" << endl;
        
    }
    assert(openEndPoints.size() == 0);
}

void PolygonReconstructor::clear() {
    set<PolygonSegment*> segs;
    /** we need to delete all remaining manually allocated polygon segments
      * These are all registered in openEndPoints, but each polygon segment appears twice in this list.
      * So, we first have to obtain a list of unique pointers to these segments*/
      
    BOOST_FOREACH( Vertex2Segment v2s, openEndPoints)
        if (!segs.count(v2s.second)) segs.insert(v2s.second);
    //for (map<Vertex, PolygonSegment*>::iterator it = openEndPoints.begin(); it != openEndPoints.end(); it++)
    //    if (!segs.count(it->second)) segs.insert(it->second);
     //delete remaining PolygonSegment

    for (set<PolygonSegment*>::iterator seg = segs.begin(); seg != segs.end(); seg++)
        delete *seg;
        
    openEndPoints.clear();
    res.clear();
        
}

