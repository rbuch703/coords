
#include "geometric_types.h"

#include <queue>
#include <iostream>

#include <boost/foreach.hpp>

#include <assert.h>
#include <stdlib.h> //for llabs
#include <math.h>



Vertex::Vertex() :x(0), y(0) {}
Vertex::Vertex(BigInt v_x, BigInt v_y): x(v_x), y(v_y) {}

double Vertex::squaredDistanceToLine(const Vertex &A, const Vertex &B) const
{
    assert( B!= A);
    BigInt num = (B.x-A.x)*(A.y-y) - (B.y-A.y)*(A.x-x);
    if (num < 0) num = -num; //distance is an absolute value
    BigInt denom_sq = (B-A).squaredLength();//(B.x-A.x)*(B.x-A.x) + (B.y-A.y)*(B.y-A.y);
    assert(denom_sq != 0);
    return (num*num).toDouble() / denom_sq.toDouble();
}


/*
double Vertex::distanceToLine(const Vertex &A, const Vertex &B) const
{
    assert( B!= A);
    double num = fabs( ((double)B.x-A.x)*((double)A.y-y) - ((double)B.y-A.y)*((double)A.x-x));
    double denom = sqrt( ((double)B.x-A.x)*((double)B.x-A.x) + ((double)B.y-A.y)*((double)B.y-A.y));
    assert(denom != 0);
    return num / denom;
}*/

BigInt Vertex::pseudoDistanceToLine(const Vertex &start, const Vertex &end) const
{
    return (end.x-start.x)*(start.y-y) - (end.y-start.y)*(start.x-x);
}

BigInt Vertex::squaredLength() const { return (x*x)+(y*y);}
bool Vertex::operator==(const Vertex &other) const { return x==other.x && y == other.y;}
bool Vertex::operator!=(const Vertex &other) const { return x!=other.x || y != other.y;}
bool Vertex::operator< (const Vertex &other) const { return (x< other.x) || ((x == other.x) && (y < other.y));}

Vertex Vertex::operator+(const Vertex &a) const { return Vertex(x+a.x, y+a.y);}
Vertex Vertex::operator-(const Vertex &a) const { return Vertex(x-a.x, y-a.y);}

//Vertex operator*(const int64_t a, const Vertex b) { return Vertex(a*b.x, a*b.y);}
//Vertex operator*(const BigInt &a, const Vertex &b) { return Vertex(a*b.x, a*b.y);}


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}

/** =================================================*/

PolygonSegment::PolygonSegment(const int32_t * vertices, int64_t num_vertices)
{
    while (num_vertices--)
    {
        m_vertices.push_back( Vertex( vertices[0], vertices[1]));
        vertices+=2;
    }
}

void PolygonSegment::append(const PolygonSegment &other, bool shareEndpoint)
{
    if (shareEndpoint) 
    {
        assert(back() == other.front());
        m_vertices.insert( m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
    } else
    {
        assert(back() != other.front()); //otherwise we would have two consecutive identical vertices
        //cout << "concatenating line segments that are " 
        //     << sqrt( (back() -other.front()).squaredLength() )/100.0 << "m apart"<< endl;
        m_vertices.insert( m_vertices.end(),   other.m_vertices.begin(), other.m_vertices.end());
    }
    
    //std::cout << "Merged: " << res
    //return res;
}


PolygonSegment::PolygonSegment( const list<OSMVertex> &vertices)
{
    BOOST_FOREACH( OSMVertex v, vertices)
        append( Vertex( v.x, v.y));
}


bool PolygonSegment::simplifyArea(double allowedDeviation)
{
    assert( m_vertices.front() == m_vertices.back() && "Not a Polygon");
    /** simplifySection() requires the Polygon to consist of at least two vertices,
        so we need to test the number of vertices here. Also, a segment cannot be a polygon
        if its vertex count is less than four (since the polygon of smallest order is a triangle,
        and first and last vertex are identical. Since simplification cannot add vertices,
        We can safely terminate the simplification here, if we are short of four vertices. */
    if ( m_vertices.size() < 4) { m_vertices.clear(); return false; }

    list<Vertex>::iterator last = m_vertices.end();
    last--;
    simplifySection( m_vertices.begin(), last, allowedDeviation);
    
    /** Need three vertices to form an area; four since first and last are identical
        If the polygon was simplified to degenerate to a line or even a single point,
        This means that the whole polygon would not be visible given the current 
        allowedDeviation. It may thus be omitted completely */
    if (m_vertices.size() < 4) { m_vertices.clear(); return false; }

    assert( m_vertices.front() == m_vertices.back());
    return true;
}

void PolygonSegment::simplifyStroke(double allowedDeviation)
{
    list<Vertex>::iterator last = m_vertices.end();
    last--;
    simplifySection( m_vertices.begin(), last, allowedDeviation);
}

void PolygonSegment::simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation)
{
    list<Vertex>::iterator it_max;
    
    double max_dist_sq = 0;
    Vertex A = *segment_first;
    Vertex B = *segment_last;
    list<Vertex>::iterator it  = segment_first;
    // make sure that 'it' starts from segment_first+1; segment_first must never be deleted
    for (it++; it != segment_last; it++)
    {
        double dist_sq = (A == B) ? 
            (*it-A).squaredLength().toDouble() : 
            it->squaredDistanceToLine(A, B);
        if (dist_sq > max_dist_sq) { it_max = it; max_dist_sq = dist_sq;}
    }
    if (max_dist_sq == 0) return;
    
    if (max_dist_sq < allowedDeviation*allowedDeviation) 
    {
        // no point further than 'allowedDeviation' from line A-B --> can delete all points in between
        segment_first++;
        // erase range includes first iterator but excludes second one
        m_vertices.erase( segment_first, segment_last);
        assert( m_vertices.front() == m_vertices.back());
    } else  //use point with maximum deviation as additional necessary point in the simplified polygon, recurse
    {
        simplifySection(segment_first, it_max, allowedDeviation);
        simplifySection(it_max, segment_last, allowedDeviation);
    }
}

static Vertex getSuccessor(list<Vertex>::const_iterator it, const list<Vertex> &lst)
{
    bool closed = lst.front() == lst.back();

    it++;
    if (it != lst.end()) 
        return *it;
    
    it = lst.begin();
    if (closed) it++;
    return *it;
}

static Vertex getPredecessor(list<Vertex>::const_iterator it, const list<Vertex> &lst)
{
    bool closed = lst.front() == lst.back();
    
    list<Vertex>::const_iterator pred = (it == lst.begin()) ? lst.end(): it;
    if (pred == lst.end() && closed)
        pred--; // move from end() to the actual last element (which equals the first element);
    return *(--pred);
}


BigInt getXCoordinate(const Vertex &v) { return v.x;}
BigInt getYCoordinate(const Vertex &v) { return v.y;}

/** 
    @brief: BASIC ALGORITHM (for clipping along the x-axis):
        Sort all polygon segments by endpoint with biggest y coordinate
        while list of segments is not empty:
            take segment with biggest endpoint y coordinate
            if this is the last remaining open polygon segment --> close it by connecting its end points, remove from list
            if the y coordinate of the other end point of this segment is also bigger than the y coordinates
                of all other polygon segment end points --> close this segment by connecting its end points, remove from list
            otherwise:
                also take the segment with the next-highest endpoint y coordinate
                connect both segments at their endpoints with highest y coordinate
                put the connected segment by into the queue; its position will change, since its endpoint with
                    highest y coordinate has been connected and thus is no longer an endpoint
*/
typedef BigInt (*VertexCoordinate)(const Vertex &v);


template<VertexCoordinate significantCoordinate, VertexCoordinate otherCoordinate> 
static void connectClippedSegments( BigInt clip_pos, list<PolygonSegment*> lst, list<PolygonSegment> &out)
{


    assert (lst.size() > 0);
    
    if (lst.size() ==1)
    {
        PolygonSegment *seg = lst.front();
        lst.pop_front();
        
        if (seg->front() != seg->back()) seg->append(seg->front());
        
        out.push_back(*seg);
        delete seg;
        return;
    }
    
    /** This connecting algorithm assumes that all segments start and end at the clipping line 
      * (since they were created by splitting the original polygon along that line).
      * However, the first and last segment in the list may be the first and last line segment from the original polygon
      * and start/end anywhere (because the original polygon may have started at any given point).
      * Thus, in order for the connecting algorithm to work, the first segment has to be joined with the last
      * segment, if the corresponding vertices do not lie on the clipping line. */

    if ( significantCoordinate(lst.front()->vertices().front() ) != clip_pos && 
         significantCoordinate(lst.back()->vertices().back()   ) != clip_pos)
    {
        assert(lst.front()->vertices().front() == lst.back()->vertices().back() );
        lst.back()->append( *lst.front(), true);
        //cout << lst.front() << endl;
        delete lst.front();
        lst.pop_front();
    }
    
    priority_queue< pair< BigInt, PolygonSegment*> > queue;
    BOOST_FOREACH( PolygonSegment* seg, lst)
    {
        assert( ( significantCoordinate( seg->vertices().front()) == clip_pos) && 
                ( significantCoordinate( seg->vertices().back() ) == clip_pos));
                
        queue.push( pair<BigInt, PolygonSegment*>( max( otherCoordinate(seg->vertices().front()),
                                                        otherCoordinate(seg->vertices().back() ) ), seg));
    }
    
    
    while (queue.size())
    {
        PolygonSegment *seg1 = queue.top().second; 
        queue.pop();
        /** this is either the last remaining segment, or its endpoints are the two segment endpoints 
          * with the highest y-coordinates. Either way, make this segment form a closed polygon by itself
         **/
        if ((queue.size() == 0) || 
            (queue.top().first < min( otherCoordinate(seg1->front()), 
                                      otherCoordinate(seg1->back() ) )))  
        {
            //cout << "creating closed polygon out of single segment" << endl << *seg1 << endl;
            if (seg1->front() != seg1->back())
                seg1->append(seg1->vertices().front());

            assert( seg1->front() == seg1->back() && "clipped polygon is not closed");

            out.push_back(*seg1);
            delete seg1;
            continue;
        }

        PolygonSegment *seg2 = queue.top().second; 
        queue.pop();
        assert( seg1 != seg2);
        //cout << "connecting two segments" << endl << *seg1 << endl << *seg2 << endl;
        
        if (otherCoordinate(seg1->front()) < otherCoordinate( seg1->back()  ) &&
            otherCoordinate(seg2->back())  < otherCoordinate( seg2->front() ) )
        {
            seg2->append( *seg1, seg2->back() == seg1->front() );
            delete seg1;
            seg1 = seg2;
        } 
        else if (otherCoordinate(seg2->front()) < otherCoordinate( seg2->back()  ) &&
                   otherCoordinate(seg1->back())  < otherCoordinate( seg1->front() ) )
        {
            seg1->append(*seg2, (seg1->back() == seg2->front()) );
            delete seg2;
        } else assert(false && "invalid segment orientation");
        
        queue.push( pair<BigInt, PolygonSegment*>(max( otherCoordinate( seg1->front()), 
                                                       otherCoordinate( seg1->back() )), seg1));
    }

}


/** ensures that no two consecutive vertices of a polygon are identical, 
    and that no three consecutive vertices are colinear.
    This is necessary for many advanced algorithms */
void PolygonSegment::canonicalize()
{
    bool closed = m_vertices.front() == m_vertices.back();
    if (closed)
    {
        // first==last causes to many special cases, so remove the duplicate here add re-add it later
        /* As a side-effect, this re-adding will only occur iff the polygon still has at least 3 vertices
         * after removing colinearities and consecutive duplicates. So only non-degenerated polygon will 
         * ever be closed, and closed polygons are guaranteed to never be degenerated.
        */
        m_vertices.pop_back(); 
    }

    if ( m_vertices.size() < 2) return; //segments of length 0 and 1 are always canonical
    
    if ( m_vertices.size() == 2)
    { //segments of length 2 are never colinear, only need to check if vertices are identical
        if (m_vertices.front() == m_vertices.back()) m_vertices.pop_back();
        return;
    }
    
    assert( m_vertices.size() >= 3);
    list<Vertex>::iterator v3 = m_vertices.begin();
    list<Vertex>::iterator v1 = v3++; 
    list<Vertex>::iterator v2 = v3++;
    
    //at this point, v1, v2, v3 hold references to the first three vertices of the polygon
    
    //first part: find and remove colinear vertices
    while (v3 != m_vertices.end())
    {
        if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) //colinear
        {
            m_vertices.erase(v2);
            v2 = v3++;
        }
        else
        {
            v1++;
            v2++;
            v3++;
        }
    }
    
    /** special case: in closed polygons, the last two and the first vertex, 
        or the last and the first two vertices may be colinear */
    if (closed)
    {
        if (m_vertices.size() >= 3)
        {
            list<Vertex>::iterator v1 = --m_vertices.end();
            list<Vertex>::iterator v2 = m_vertices.begin(); 
            list<Vertex>::iterator v3 = ++m_vertices.begin();
            if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) 
                m_vertices.erase(v2);
        }

        // if we still have three vertices, even after potentially removing one in the previous step
        if (m_vertices.size() >= 3)
        {
            list<Vertex>::iterator v1 = ----m_vertices.end();
            list<Vertex>::iterator v2 = --m_vertices.end(); 
            list<Vertex>::iterator v3 = m_vertices.begin();
            if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) 
                m_vertices.erase(v2);
        }
        
    }

    //second part: remove consecutive identical vertices
    v1 = m_vertices.begin();
    v2 = ++m_vertices.begin();
    while (v2 != m_vertices.end())
    {
        if (*v1 == *v2)
        {
            m_vertices.erase(v1);
                v1 = v2++;
        } else
        {
            v1++;
            v2++;
        }   
    }
    
    while (m_vertices.front() == m_vertices.back()) m_vertices.pop_back();
    
    if (closed && m_vertices.size() >= 3)
        //re-duplicate the first vertex to again mark the polygon as closed 
        m_vertices.push_back(m_vertices.front());   
}

const Vertex& PolygonSegment::front() const 
{ 
    return m_vertices.front();
}

const Vertex& PolygonSegment::back()  const 
{ 
    return m_vertices.back();
}

const list<Vertex>& PolygonSegment::vertices() const 
{ 
    return m_vertices;
}

void PolygonSegment::reverse() 
{ 
    m_vertices.reverse();
}

void PolygonSegment::append(const Vertex& node) 
{
    m_vertices.push_back(node);
}

void PolygonSegment::append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end ) 
{
    m_vertices.insert(m_vertices.end(),begin, end);
}



bool PolygonSegment::isSimple() const
{

    #warning causes false positives when three/four edges touch without intersecting
    /*  e.g.:   _________
     *             /\
     *            /  \            */

    for (list<Vertex>::const_iterator v1 = m_vertices.begin(); v1 != m_vertices.end(); v1++)
    {
        list<Vertex>::const_iterator v2 = v1;
        v2++;
        if (v2 == m_vertices.end()) 
            break;
        
        list<Vertex>::const_iterator v3 = v2;
        while (++v3 != m_vertices.end())
        {
            list<Vertex>::const_iterator v4 = v3;
            v4++;
            if (v4 == m_vertices.end()) 
                break;

            LineSegment ls1(*v1, *v2);
            LineSegment ls2(*v3, *v4);
            if (ls1.intersects(ls2)) 
                return false;
        }
    }
    return true;
}



template<VertexCoordinate significantCoordinate, VertexCoordinate otherCoordinate> 
static void clip( const list<Vertex> & polygon, BigInt clip_pos, list<PolygonSegment*> &above, list<PolygonSegment*> &below)
{
    assert( polygon.front() == polygon.back());
    #warning TODO: handle the edge case that front() and back() both lie on the split line

    PolygonSegment *current_seg = new PolygonSegment();
    list<Vertex>::const_iterator v2 = polygon.begin();

    do { current_seg->append( *v2 ); v2++;}
    while (significantCoordinate (current_seg->back()) == clip_pos);

    bool isAbove = significantCoordinate( current_seg->back() ) <= clip_pos;
    list<Vertex>::const_iterator v1 = v2;
    for ( v1--; v2 != polygon.end(); v1++, v2++)
    {
        if (( isAbove && significantCoordinate(*v2) <= clip_pos) || 
            (!isAbove && significantCoordinate(*v2)  > clip_pos))
            { current_seg->append(*v2); continue;} //still on the same side of the clipping line

        // if this point is reached, v1 and v2 are on opposite sides of the clipping line
        assert(significantCoordinate(*v2) != significantCoordinate(*v1) );
        //line (v1, v2) must have intersected the clipping line
        
        /*BigInt clip_other = otherCoordinate(*v1) + ((clip_pos - significantCoordinate(*v1)) *
                        ( otherCoordinate(*v2) - otherCoordinate(*v1) ) )/
                        ( significantCoordinate(*v2) - significantCoordinate(*v1) )*/;
        int64_t num = (int64_t)((clip_pos - significantCoordinate(*v1)) * ( otherCoordinate(*v2) - otherCoordinate(*v1) ) );
        int64_t denom=(int64_t)( significantCoordinate(*v2) - significantCoordinate(*v1) );
        
        BigInt clip_other = (int64_t)( (int64_t)otherCoordinate(*v1) + (num+ 0.5)/denom);
        
        Vertex vClip;
        if      (significantCoordinate == getXCoordinate) vClip = Vertex(clip_pos, clip_other);
        else if (significantCoordinate == getYCoordinate) vClip = Vertex(clip_other, clip_pos);
        else  {assert(false && "unsupported coordinate accessor"); exit(0);}
        
        current_seg->append(vClip);
        
        
        if (isAbove)
            above.push_back(current_seg);
        else
            below.push_back(current_seg);

        isAbove = !isAbove;
        current_seg = new PolygonSegment();
        current_seg->append( Vertex(vClip) );
        current_seg->append( *v2);
    }
    if (isAbove) above.push_back(current_seg);
    else below.push_back(current_seg);
    
    #warning may produce degenerated polygons with zero area
    //FIXME: test whether all vertices of a segment lie completely on the clipping line; if so, discard the segment
    
    #warning may produce connected line segments that are co-linear and should be replaced by a single line
}


void PolygonSegment::clipSecondComponent( BigInt clip_y, list<PolygonSegment> &out_above, list<PolygonSegment> &out_below) const
{
    list<PolygonSegment*> above, below;
    
    clip<getYCoordinate, getXCoordinate>( m_vertices, clip_y, above, below);

    if (above.size()) connectClippedSegments<getYCoordinate,getXCoordinate>(clip_y, above, out_above);
    if (below.size()) connectClippedSegments<getYCoordinate,getXCoordinate>(clip_y, below, out_below);

}

void PolygonSegment::clipFirstComponent( BigInt clip_x, list<PolygonSegment> &out_left, list<PolygonSegment> &out_right) const
{
    list<PolygonSegment*> left, right;
    
    clip<getXCoordinate, getYCoordinate>( m_vertices, clip_x, left, right);

    if (left.size())  connectClippedSegments<getXCoordinate,getYCoordinate>(clip_x, left,  out_left );
    if (right.size()) connectClippedSegments<getXCoordinate,getYCoordinate>(clip_x, right, out_right);
}

bool PolygonSegment::isClockwise() const
{
    assert (front() == back() && " Clockwise test is defined for closed polygons only");
    
    list<Vertex>::const_iterator vMin = m_vertices.begin();
    
    for (list<Vertex>::const_iterator v = m_vertices.begin(); v != m_vertices.end(); v++)
        if (*v < *vMin)
            vMin = v;
            
    Vertex v = *vMin;
    Vertex vPred = getPredecessor( vMin, m_vertices);
    Vertex vSucc = getSuccessor(   vMin, m_vertices);
    
    
    assert( v.pseudoDistanceToLine( vPred, vSucc) != 0 && "colinear vertices detected");
    return v.pseudoDistanceToLine( vPred, vSucc) < 0;
}

/*
static void getExtremeIntersections( const list<Vertex> vertices, const LineSegment line, 
                              list<Vertex>::const_iterator &min_it,
                              list<Vertex>::const_iterator &max_it)
{
    double min_intersect = 1;
    double max_intersect = 0;
    
    for (list<Vertex>::const_iterator v = vertices.begin(); v != vertices.end(); v++)
    {
        LineSegment other( *v, getSuccessor(v, vertices), 0, 0);
        if ( line.intersects(other))
        {
            double c = line.getIntersectionCoefficient(other);
            if (c < min_intersect)
            {
                min_intersect = c;
                min_it = v;
            }
            
            if (c > max_intersect)
            {
                max_intersect = c;
                max_it = v;
            }
        }   
    }
    
    if (min_intersect == 0 || max_intersect == 1)
    {
        BOOST_FOREACH(Vertex v, vertices)
            cout << v << endl;
    }
    assert (min_intersect > 0 && max_intersect < 1);

}*/
#if 0
static bool isLeftTurnAt( const list<Vertex> &vertices, list<Vertex>::const_iterator it)
{
    assert ( (*it).pseudoDistanceToLine(getPredecessor(it, vertices), getSuccessor(it, vertices)) != 0);
    return ( (*it).pseudoDistanceToLine(getPredecessor(it, vertices), getSuccessor(it, vertices)) < 0);

}

/** #warning: as long as isSimple() is an O(n²) algorithm, this may take a long time */
bool PolygonSegment::isClockwiseHeuristic() const
{
    if (isSimple()) return isClockwise();
    
    int nVotesClockwise = 0;
    int nVotesCounterClockwise = 0;
    
    list<Vertex>::const_iterator min = m_vertices.begin();
    list<Vertex>::const_iterator max = m_vertices.begin();
    AABoundingBox bb(*min);
    for (list<Vertex>::const_iterator v = m_vertices.begin(); v != m_vertices.end(); v++)
    {
        if (*v < *min) min = v;
        if (*max < *v) max = v;
        bb+= *v;
    }
    
    if (isLeftTurnAt(m_vertices, min) )  nVotesClockwise++; else nVotesCounterClockwise++;
    if (isLeftTurnAt(m_vertices, max) )  nVotesClockwise++; else nVotesCounterClockwise++;

    list<Vertex>::const_iterator intersect_min, intersect_max;

    int64_t mid_h = (bb.left + bb.right)/2;
    LineSegment vertSplit( mid_h, bb.top - 1, mid_h, bb.bottom+1, 0, 0);
    getExtremeIntersections( m_vertices, vertSplit, intersect_min, intersect_max);
    if (isLeftTurnAt(m_vertices, intersect_min)) nVotesClockwise++; else nVotesCounterClockwise++;
    if (isLeftTurnAt(m_vertices, intersect_max)) nVotesClockwise++; else nVotesCounterClockwise++;

    int64_t mid_v = (bb.top + bb.bottom) /2;
    LineSegment horizSplit(bb.left - 1, mid_v, bb.right + 1, mid_v, 0, 0);
    getExtremeIntersections( m_vertices, horizSplit, intersect_min, intersect_max);
    if (isLeftTurnAt(m_vertices, intersect_min)) nVotesClockwise++; else nVotesCounterClockwise++;
    if (isLeftTurnAt(m_vertices, intersect_max)) nVotesClockwise++; else nVotesCounterClockwise++;

    
    assert( nVotesClockwise + nVotesCounterClockwise == 6);
    
    if (nVotesClockwise > 4) return true;
    else if (nVotesCounterClockwise > 4) return false;
    else assert(false && "no clear vote");
}

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg)
{
    for (list<Vertex>::const_iterator it = seg.vertices().begin(); it != seg.vertices().end(); it++)
        os << *it << endl;
    return os;
}
#endif

/** ============================================================================= */
LineSegment::LineSegment( const Vertex v_start, const Vertex v_end): 
                        start(v_start), end(v_end) { assert(start != end);}
                        
LineSegment::LineSegment( BigInt start_x, BigInt start_y, BigInt end_x, BigInt end_y): 
    start(Vertex(start_x, start_y)), end(Vertex(end_x, end_y)) { assert(start != end);}


bool LineSegment::parallelTo( const LineSegment &other) const 
{ 
    return (end.x- start.x)*(other.end.y - other.start.y) ==
           (end.y- start.y)*(other.end.x - other.start.x);
}


// @returns whether the vertex lies on the line (not necessarily on the line segment)
bool LineSegment::isColinearWith(const Vertex v) const
{
    if (( v.x-start.x)*(end.y - start.y) != 
        ( v.y-start.y)*(end.x - start.x)) return false;
    
    BigInt squared_seg_len = (end - start).squaredLength();
    BigInt dist_start =      (v - start).squaredLength();
    BigInt dist_end =        (v - end).squaredLength();
    return  dist_start <squared_seg_len && dist_end <= squared_seg_len;
}
                       
                                                  
bool LineSegment::intersects( const LineSegment &other) const
{
    if (parallelTo(other))
    {
        // if the two parallel line segments do not lie on the same line, they cannot intersect
        if (start.pseudoDistanceToLine(other.start, other.end) != 0) return false;
        return isColinearWith(other.start) 
            /*|| contains(other.end) */
            || other.isColinearWith(start) 
            /*|| other.contains(end )*/
            || ((start == other.end) && (end == other.start));
    } else
    {
        BigInt num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
        BigInt num2 = (      end.x-      start.x)*(start.y-other.start.y) - (      end.y-      start.y)*(start.x-other.start.x);
        BigInt denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);

        assert(denom != 0); //should only be zero if the lines are parallel, but this case has already been handled above

        //for the line segments to intersect, num1/denom and num2/denom both have to be inside the range [0, 1);
        
        //if the absolute of at least one coefficient would be bigger than or equal to one 
        if (abs(num1) >=abs(denom) || abs(num2) >= abs(denom)) return false; 
        
        // at least one coefficient would be negative
        if (( num1 < 0 || num2 < 0) && denom > 0) return false; 
        if (( num1 > 0 || num2 > 0) && denom < 0) return false; // coefficient would be negative
        
        return true; // not negative and not bigger than/equal to one --> segments intersect
    }
    
}

void LineSegment::getIntersectionCoefficient( const LineSegment &other, BigInt &out_num, BigInt &out_denom) const
{
    assert( !parallelTo(other));
    //TODO: handle edge case that two line segments overlap
    out_num = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
    out_denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);
    
    assert(out_denom != 0 && "Line segments are parallel or coincide" );

}

Vertex LineSegment::getRoundedIntersection(const LineSegment &other) const
{
    /*
    BigInt num, denom;
    getIntersectionCoeffcicient(other, num, denom);
    assert ( ((num >= 0 && denom > 0 && num > denom) || (num <= 0 && denom < 0 && num < denom)) && "Do not intersect");
    int64_t iNum = (int64_t)num;
    int64_t iDenom = (int64_t)denom;
    assert (iNum == num && iDenom == denom);
    */
    return Vertex();
}


double LineSegment::getIntersectionCoefficient( const LineSegment &other) const
{
    BigInt num, denom;
    getIntersectionCoefficient(other, num, denom);
    return num.toDouble()/denom.toDouble();
}

/** ============================================================================= */

/*
bool intersect (const Vertex l1_start, const Vertex l1_end, const Vertex l2_start, const Vertex l2_end)
{
    Vertex dir1 = l1_end - l1_start;
    Vertex dir2 = l2_end - l2_start;
    assert( dir1 != Vertex(0,0) && dir2!= Vertex(0,0));
    
    if (dir1.parallelTo(dir2))
    {
        
    } else
    {
        
    }
}*/

#if 0
void findIntersectionsBruteForce(const list<LineSegment> &segments, list<LineSegment> &intersections_out)
{
    for (list<LineSegment>::const_iterator seg1 = segments.begin(); seg1!= segments.end(); seg1++)
    {
        list<LineSegment>::const_iterator seg2 = seg1;
        seg2++;
        for (; seg2!= segments.end(); seg2++)
        {
            if (seg1->intersects(*seg2)) 
            { 
                intersections_out.push_back(*seg1); intersections_out.push_back(*seg2);
            }
        }
    }
}

void findPairwiseIntersections(const list<LineSegment> &set1, const list<LineSegment> &set2, list<LineSegment> &intersections_out)
{
    for (list<LineSegment>::const_iterator seg1 = set1.begin(); seg1!= set1.end(); seg1++)
        for (list<LineSegment>::const_iterator seg2 = set2.begin(); seg2!= set2.end(); seg2++)
            if (seg1->intersects(*seg2)) 
            {
                intersections_out.push_back(*seg1); 
                intersections_out.push_back(*seg2);
            }
}

void findIntersections(const list<LineSegment> &segments, const AABoundingBox &box, list<LineSegment> &intersections_out)
{
    
    //if too few line segments, or area too small for further subdivision, test each segment against every other segment
    if ((segments.size() < 4) || (box.right - box.left < 2 && box.bottom - box.top < 2)) 
    {
        findIntersectionsBruteForce(segments, intersections_out);
        return;
    }
    
    
    /** either way, extend the line segment by 1 in both directions to avoid ambiguities 
        due to a line end point not being part of the line segment */
    LineSegment splitLine = (box.right - box.left > box.bottom-box.top) ?
        //vertical split
        LineSegment(Vertex( (box.right+box.left)/2, box.bottom+1), Vertex((box.right+box.left)/2, box.top-1), -1, -1) :
        //horizontal split
        LineSegment(Vertex( box.left - 1, (box.top + box.bottom)/2), Vertex( box.right+1, (box.top+box.bottom)/2), -1, -1);
        
    list<LineSegment> left, right, intersect;
    AABoundingBox *bb_left = NULL, *bb_right = NULL;
    for (list<LineSegment>::const_iterator seg = segments.begin(); seg!= segments.end(); seg++)
    {
        if ( splitLine.intersects(*seg)) intersect.push_back(*seg);
        else if ( seg->start.pseudoDistanceToLine( splitLine.start, splitLine.end) < 0 ) 
        {
            if (!bb_left) bb_left = new AABoundingBox(seg->start);
            (*bb_left)+= seg->start;
            (*bb_left)+= seg->end;
            left.push_back(*seg);
        }
        else 
        {
            if (!bb_right) bb_right = new AABoundingBox(seg->start);
            (*bb_right)+= seg->start;
            (*bb_right)+= seg->end;
            right.push_back(*seg);
        }
    }
    /** at this point the set of line segments has been split into three subsets:
        line segments left of, intersecting, and right of the split line.
        Of these, segments in the left subset cannot possibly intersect those in the right subset.
        But segments in the left subset may intersect each other, those in the right subset may intersect each other,
        and segments crossing the split line may intersect all segments
    */
    
    if (left.size() > 1) {assert(bb_left); findIntersections(left, *bb_left, intersections_out);}

    //test all segments against those in the 'intersect' set
    findPairwiseIntersections(intersect, left, intersections_out);
    findIntersectionsBruteForce(intersect, intersections_out);
    findPairwiseIntersections(intersect, right, intersections_out);

    if (right.size() > 1) {assert(bb_right); findIntersections(right, *bb_right, intersections_out);}
    
    if (bb_left) delete bb_left;
    if (bb_right) delete bb_right;
}

void findIntersections(const list<Vertex> &path, list<LineSegment> &intersections_out)
{
    list<LineSegment> segments;
    list<Vertex>::const_iterator line_start = path.begin();
    list<Vertex>::const_iterator line_end = line_start; line_end++;
    
    AABoundingBox box( *line_start);
    int32_t id = 0;
    //std::cout << *line_start << endl;
    while (line_end != path.end())
    {
        segments.push_back( LineSegment(*line_start, *line_end, id, id+1));
        //std::cout << *line_end << endl;
        box += *line_end;
        line_start++;
        line_end++;
        id++;
    }
    
    findIntersections( segments, box, intersections_out);
}

/** Algorithm to create simple (i.e. non-selfintersecting) polygons:
  * find the first intersection between two line segments. Since each line segment
  * consist of two vertices with indices X and X+1, these line segments will have indices
  * X, X+1, Y, Y+1 ( WLOG, X is the smallest index)
  * To resolve the self-intersection, cut out all vertices from X+1 to Y (both inclusive)
  * and let them form their own polygon. This removes a single self-intersection. Now,
  * apply the algorithm to the two separate poylgons separately until no self-intersections remain
  */
void createSimplePolygons(const list<Vertex> in, const list<LineSegment> intersections, list<PolygonSegment> &out)
{

    if ( in.size() < 4) 
    {
        cout << "not a polygon->discarded" << endl;
        return;
    }
    //cout << "======" << endl;
    assert(in.front() == in.back());
    //if (in.front() != in.back()) in.push_back(in.front());
    
    //findIntersections(in, intersections);
    assert(intersections.size() % 2 == 0); //each intersection is given by *two* line segments
    
    if (intersections.size() == 0) //no self intersection --> whole polygon is intersection-free
    {
        PolygonSegment tmp;
        tmp.append(in.begin(), in.end());
        out.push_back(tmp);
        return;
    }
    int32_t earliest_idx = 0x7fffffff;
    uint32_t num_earliest = 0;
    for (list<LineSegment>::const_iterator it = intersections.begin(); it!= intersections.end(); it++)
    {
        assert (it->tag1+1 == it->tag2);
        if (it->tag1 == earliest_idx) num_earliest++;
        //if (it->tag2 == earliest_idx) num_earliest++;
        if (it->tag1 < earliest_idx) {earliest_idx = it->tag1; num_earliest = 1;}
        //if (it->tag2 < earliest_idx) {earliest_idx = it->tag2; num_earliest = 1;}
    }
    
    list<LineSegment>::const_iterator earliest_int_seg, earliest_int_seg2;
    double earliest_coeff = 1.0;
    
    bool even = true;
    /** not that we know the first line segment to be part of a self-intersection,
      * find the one other segment that intersects it the earliest (there may only be one intersection at all)*/
    for (list<LineSegment>::const_iterator it = intersections.begin(); it!= intersections.end(); it++)
    {
        if (it->tag1 == earliest_idx)
        {
            list<LineSegment>::const_iterator other = it;
            if (even) other++; else other--;
            double coeff = it->getIntersectionCoefficient(*other);
            if (coeff < earliest_coeff) 
            {
                earliest_coeff = coeff;
                earliest_int_seg = it;
                earliest_int_seg2 = other;
            }
        }
        even=!even;
    }
    int32_t out_idx = earliest_idx; //the last vertex not to be cut out
    int32_t in_idx = earliest_int_seg2->tag2; //the first vertex after the cut-out
    //int32_t num_removed = in_idx - out_idx +1;
    list<Vertex> first_poly, second_poly;
    list<LineSegment> first_intersections, second_intersections;
    int idx = 0;
    for (list<Vertex>::const_iterator it = in.begin(); it != in.end(); it++, idx++)
    {
        if (idx <= out_idx || idx >= in_idx) 
            first_poly.push_back(*it);
        else second_poly.push_back(*it);
    }
    second_poly.push_back(second_poly.front()); //close the second polygon (first one is already closed)

    list<LineSegment>::const_iterator seg1 = intersections.begin();
    list<LineSegment>::const_iterator seg2= seg1; 
    seg2++;
    /** sort the set of intersection:
        - those on lines that do not exist any more (because the line enpoints now belong to different polygons) are discarded
        - those whose lines now belong to different polygons are discarded
        - those whose lines belong to the same polygon (either both to the original, or both to the cut-out section) are
          added to the corresponding list of segments;
      */
    //an intersection consists of two successive entries, so handle them together
    for (;seg1 != intersections.begin(); seg1++,seg1++, seg2++, seg2++)
    {
        
        bool v11_in = seg1->tag1 <= out_idx || seg1->tag1 >= in_idx;
        bool v12_in = seg1->tag2 <= out_idx || seg1->tag2 >= in_idx;
        bool v21_in = seg2->tag1 <= out_idx || seg2->tag1 >= in_idx;
        bool v22_in = seg2->tag2 <= out_idx || seg2->tag2 >= in_idx;
        if (v11_in != v12_in || v21_in != v22_in) continue; //line split, vertices now belong to different polygons
        if (v11_in != v21_in) continue;                     //lines belong to two different polygons
        assert( v12_in == v22_in);
        if (v11_in && v12_in && v21_in && v22_in)
        {
            LineSegment s = *seg1;
            if (s.tag1 >= in_idx) s.tag1-= num_removed;
            if (s.tag2 >= in_idx) s.tag2-= num_removed;
            first_intersections.push_back(s);
            s = *seg2;
            if (s.tag1 >= in_idx) s.tag1-= num_removed;
            if (s.tag2 >= in_idx) s.tag2-= num_removed;
            first_intersections.push_back(s);
        } else if (!v11_in && !v12_in && !v21_in && !v22_in)
        {
            LineSegment s = *seg1;
            s.tag1-= (out_idx+1);
            s.tag2-= (out_idx+1);
            second_intersections.push_back(s);
            s = *seg2;
            s.tag1-= (out_idx+1);
            s.tag2-= (out_idx+1);
            second_intersections.push_back(s);
        }
    }
    
    LineSegment new_1( earliest_int_seg->start, earliest_int_seg2->end, earliest_int_seg->tag1, earliest_int_seg->tag1+1);
    

    //TODO: add intersections of new lines ( [out_idx, in_idx], [out_idx+1, in_idx-1]) to first_intersections and second_intersections
    list<LineSegment> ints;
    findIntersections( first_poly, ints);
    createSimplePolygons(first_poly, ints, out);
    
    ints.clear();
    findIntersections( second_poly, ints);
    createSimplePolygons(second_poly, ints, out);
    
/*    cout << "======" << endl;
    idx=0;
    for (list<Vertex>::const_iterator it = in.begin(); it != in.end(); it++, idx++)
    {
        if (idx > out_idx && idx < in_idx) cout << *it << endl;
    }
        

    //TODO: cut out, recurse;
    cout << earliest_idx << ", " << num_earliest << endl;
    cout << "earliest intersection at index " << earliest_idx << " between " << earliest_int_seg->start << "->" 
         << earliest_int_seg->end << " and " << earliest_int_seg2->start << "->" << earliest_int_seg2->end 
         << " at " << earliest_coeff << endl;*/
    //getIntersectionCoefficient    
}


#endif

// ====================================================================

AABoundingBox::AABoundingBox(const Vertex v) { left = right= v.x; top=bottom=v.y;}
AABoundingBox::AABoundingBox(BigInt t, BigInt l, BigInt b, BigInt r): top(t), left(l), bottom(b), right(r) { }
AABoundingBox & AABoundingBox::operator+=(const Vertex v) {
    if (v.x < left) left = v.x;
    if (v.x > right) right = v.x;
    if (v.y < top) top = v.y;
    if (v.y > bottom) bottom = v.y;
    return *this;
}

static bool isNormalized( const AABoundingBox &box)
{
    return box.right >= box.left && box.bottom >= box.top;
}

AABoundingBox AABoundingBox::getOverlap(const AABoundingBox &other) const
{
    assert ( isNormalized(*this) && isNormalized(other));
    BigInt new_left = max( left, other.left);
    BigInt new_right= min( right, other.right);
    assert(new_left <= new_right);
    
    BigInt new_top  = max( top, other.top);
    BigInt new_bottom=min( bottom, other.bottom);
    assert(new_top <= new_bottom);
    
    AABoundingBox box(Vertex(new_left, new_top));
    box+= Vertex(new_right, new_bottom);
    return box;
}

bool AABoundingBox::overlapsWith(const AABoundingBox &other) const
{  
    assert ( isNormalized(*this) && isNormalized(other));
    return (max( left, other.left) <= min( right, other.right)) && 
           (max( top, other.top)   <= min( bottom, other.bottom));
}

BigInt AABoundingBox::width()  const { return right - left;}
BigInt AABoundingBox::height() const { return bottom - top;}

//=======================================================================





