
#include "vertexchain.h"
#include "quadtree.h" //for toSimplePolygons()

#include <queue>
#include <boost/foreach.hpp>

static BigInt getXCoordinate(const Vertex &v) { return v.get_x();}
static BigInt getYCoordinate(const Vertex &v) { return v.get_y();}

void simplifySection(list<Vertex> &m_vertices, list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);

bool isClockwise(const list<Vertex> &m_vertices);

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


VertexChain::VertexChain(const int32_t * vertices, int64_t num_vertices)
{
    m_vertices.reserve(num_vertices);
    
    while (num_vertices--)
    {
        m_vertices.push_back( Vertex( vertices[0], vertices[1]));
        vertices+=2;
    }
}

void VertexChain::append(const VertexChain &other, bool shareEndpoint)
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


VertexChain::VertexChain( const list<OSMVertex> &vertices)
{
    BOOST_FOREACH( OSMVertex v, vertices)
        append( Vertex( v.x, v.y));
}


bool VertexChain::simplifyArea(double allowedDeviation)
{
    bool clockwise = isClockwise();
    assert( m_vertices.front() == m_vertices.back() && "Not a Polygon");
    /** simplifySection() requires the Polygon to consist of at least two vertices,
        so we need to test the number of vertices here. Also, a segment cannot be a polygon
        if its vertex count is less than four (since the polygon of smallest order is a triangle,
        and first and last vertex are identical. Since simplification cannot add vertices,
        we can safely terminate the simplification here, if we are short of four vertices. */
    if ( m_vertices.size() < 4) { m_vertices.clear(); return false; }


    list<Vertex> vertices( m_vertices.begin(), m_vertices.end() );
    m_vertices.clear();
    
    
    list<Vertex>::iterator last = vertices.end();
    last--;
    simplifySection( vertices, vertices.begin(), last, allowedDeviation);

    m_vertices = vector<Vertex>(vertices.begin(), vertices.end() );

    canonicalize();
    
    /** Need three vertices to form an area; four since first and last are identical
        If the polygon was simplified to degenerate to a line or even a single point,
        This means that the whole polygon would not be visible given the current 
        allowedDeviation. It may thus be omitted completely */
    if (m_vertices.size() < 4) { return false; }

    assert( m_vertices.front() == m_vertices.back());
    if ( isClockwise() != clockwise) this->reverse();
    return true;
}


AABoundingBox VertexChain::getBoundingBox() const
{
    AABoundingBox box(m_vertices.front());
    for (vector<Vertex>::const_iterator it = m_vertices.begin(); it!= m_vertices.end(); it++)
        box += *it;
        
    return box;
}

void VertexChain::simplifyStroke(double allowedDeviation)
{
    
    //TODO: check whether the whole VertexChain would be simplified to a single line (start-end)
    //      if so, do not allocate the list, but set the result right away

    list<Vertex> vertices( m_vertices.begin(), m_vertices.end());
    m_vertices.clear();
    
    
    list<Vertex>::iterator last = vertices.end();
    last--;
    simplifySection( vertices, vertices.begin(), last, allowedDeviation);
    
    m_vertices = vector<Vertex>( vertices.begin(), vertices.end() );    
    
}

void simplifySection(list<Vertex> &m_vertices, list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation)
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
            asDouble ((*it-A).squaredLength()) : 
            asDouble ( it->squaredDistanceToLine(A, B));
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
        simplifySection(m_vertices, segment_first, it_max, allowedDeviation);
        simplifySection(m_vertices, it_max, segment_last, allowedDeviation);
    }
}

/*
static Vertex getSuccessor(list<Vertex>::const_iterator it, const list<Vertex> &lst)
{
    bool closed = lst.front() == lst.back();

    it++;
    if (it != lst.end())
        return *it;
    
    it = lst.begin();
    if (closed && it != lst.end()) //check for lst.end() is necessary for lists with 0/1 elements
    {
        it++;    
        if (it == lst.end())    //work-around for lists with a single element
            it--;
    }
    
    return *it;
}

static Vertex getPredecessor(list<Vertex>::const_iterator it, const list<Vertex> &lst)
{
    bool closed = lst.front() == lst.back();
    
    list<Vertex>::const_iterator pred = (it == lst.begin()) ? lst.end(): it;
    if (pred == lst.end() && closed)
        pred--; // move from end() to the actual last element (which equals the first element);
    return pred == lst.begin() ? *pred : *(--pred);
}*/



template<VertexCoordinate significantCoordinate, VertexCoordinate otherCoordinate> 
static void connectClippedSegments( BigInt clip_pos, list<VertexChain*> lst, list<VertexChain> &out)
{

    if (lst.size() == 0) return;
    
    if (lst.size() ==1)
    {
        VertexChain *seg = lst.front();
        lst.pop_front();
        
        if (seg->front() != seg->back()) seg->append(seg->front());
        
        seg->canonicalize();
        if (seg->size() >= 4)   //skip degenerate segments (points, lines) that do not form a polygon
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

    if ( significantCoordinate(lst.front()->data().front() ) != clip_pos && 
         significantCoordinate(lst.back()->data().back()   ) != clip_pos)
    {
        assert(lst.front()->data().front() == lst.back()->data().back() );
        lst.back()->append( *lst.front(), true);
        //cout << lst.front() << endl;
        delete lst.front();
        lst.pop_front();
    }
    
    priority_queue< pair< BigInt, VertexChain*> > queue;
    BOOST_FOREACH( VertexChain* seg, lst)
    {
        assert( ( significantCoordinate( seg->data().front()) == clip_pos) && 
                ( significantCoordinate( seg->data().back() ) == clip_pos));
                
        queue.push( pair<BigInt, VertexChain*>( max( otherCoordinate(seg->data().front()),
                                                     otherCoordinate(seg->data().back() ) ), seg));
    }
    
    
    while (queue.size())
    {
        VertexChain *seg1 = queue.top().second; 
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
                seg1->append(seg1->data().front());

            assert( seg1->front() == seg1->back() && "clipped polygon is not closed");
            
            seg1->canonicalize();
            if (seg1->size() >= 4)
                out.push_back(*seg1);
            delete seg1;
            continue;
        }

        VertexChain *seg2 = queue.top().second; 
        queue.pop();
        assert( seg1 != seg2);
        //cout << "connecting two segments" << endl << *seg1 << endl << *seg2 << endl;
        
        if ( otherCoordinate(seg1->front()) > otherCoordinate(seg1->back() )) 
            seg1->reverse(); //now seg1 ends with the vertex at which seg2 is to be appended
         
        if ( otherCoordinate(seg2->front()) < otherCoordinate(seg2->back() ))
            seg2->reverse(); //now seg2 starts with the vertex with which it is to be appended to seg1
            
        seg1->append(*seg2, (seg1->back() == seg2->front()) );
        delete seg2;
        
        queue.push( pair<BigInt, VertexChain*>(max( otherCoordinate( seg1->front()), 
                                                       otherCoordinate( seg1->back() )), seg1));
    }

}

bool VertexChain::isCanonicalPolygon()
{
    if (front() != back()) return false;
    
    m_vertices.pop_back();
    
    uint64_t n = size();
    for (uint64_t me = 0; me < n-1; me++)
        if (m_vertices[me].pseudoDistanceToLine( m_vertices[(me+n-1)%n], m_vertices[(me+1) % n]) == 0)
        {   //colinear --> not canonical
            m_vertices.push_back( front());
            return false;
        }
    
    for (uint64_t me = 0; me < n; me++)
    {
        if ( m_vertices[me] == m_vertices[ (me+1) % n])
        {   //identical to successor --> not canonical
            m_vertices.push_back( front() );
            return false;
        }
    }
    
    m_vertices.push_back( front() );
    return true;
    
}

/** ensures that no two consecutive vertices of a polygon are identical, 
    and that no three consecutive vertices are colinear.
    This property is a necessary prerequisite for many advanced algorithms */
void VertexChain::canonicalize()
{
    //for open vertex chains, this method is incorrect; For these, colinearity must not be checked among first and last vertex
    assert ( m_vertices.front() == m_vertices.back() && "incomplete implementation");
    
    
    if (size() == 0) return;
    
    bool closed = m_vertices.front() == m_vertices.back();
    if (closed)
    {
        // first==last causes to many special cases, so remove the duplicate(s) here add re-add one later.
        /* As a side-effect, this re-adding will only occur iff the polygon still has at least 3 vertices
         * after removing colinearities and consecutive duplicates. So only non-degenerated polygon will 
         * ever be closed, and closed polygons are guaranteed to never be degenerated.
        */
        m_vertices.pop_back(); 
    }

    if ( size() < 2) return; //segments of length 0 and 1 are always canonical
    
    if ( size() == 2)
    { //segments of length 2 cannot be colinear, only need to check if vertices are identical
        if (m_vertices.front() == m_vertices.back()) 
            m_vertices.pop_back();
        return;
    }
    assert( size() >= 3 );
    
    
    bool hasMadeChanges = true;
    /* current algorithm cannot always find all colinear successive edges in a single run */
    while (hasMadeChanges)
    {
        hasMadeChanges = false;
        uint64_t n = size();
        uint64_t gap = 0;
        //when vertices is removed, all succesor vertices need to be moved some positions left (as indicated by 'gap')
        //in order to fill the empty spot(s). The variable 'alreadyShifted' hold whether m_vertices[i] has already 
        //been shifted
        
        bool alreadyShifted = false;    
        for (int i = 0; i+gap < n;)
        {
            if (! (alreadyShifted || (gap == 0)) )
            {
                assert (m_vertices[i] == Vertex(0,0)); //debug code, holds only while invalidated verices are overwritten with Vertex(0,0)
                m_vertices[i] = m_vertices[i+gap];
                m_vertices[i+gap] = Vertex(0,0); //debug code to visualize invalidated entries
            }
            alreadyShifted = false;
            
            uint64_t pred = (i+  n-1) % n;
            uint64_t succ = (i+gap+1) % n;
            
            if ( m_vertices[i].pseudoDistanceToLine( m_vertices[pred], m_vertices[ succ]) != 0) //not colinear
            {
                i++;
            } else
            {
                hasMadeChanges = true;
                gap++;
                m_vertices[i] = Vertex(0,0);    //debug code
                
                if (i > 0)
                {
                    /* since vertex 'i' was removed, vertex 'i-1' has a new successor and may be 
                     * colinear to that (and its the predecessor i-2) --> needs to be checked again */
                    i--;
                    alreadyShifted = true;
                }
                //otherwise we removed the first vertex --> no vertex to recheck, just continue as usual
            }
        }

        m_vertices.resize( size() - gap);
    }
    
    
    #ifndef NDEBUG
    uint64_t n = size();
    //gap = 0;
    for (uint64_t i = 0; i < n; i++)
        assert( m_vertices[i] != m_vertices[ (i+1)%n ]);
    
    #endif
    
    /*
    for (int i = 0; i+gap < n;)
    {
        if (gap > 0)
            m_vertices[i] = m_vertices[i+gap];
            
        if (m_vertices[i] == m_vertices[ (i+gap+1) % n]) //successive identical vertices
            gap++;
        else
            i++; 
    }*/

    //m_vertices.resize( size() - gap);
    if (m_vertices.size() >= 3 && closed) // reclose polygon if - after canonicalization - it still has an area
        m_vertices.push_back(m_vertices.front());
}

#if 0
/** ensures that no two consecutive vertices of a polygon are identical, 
    and that no three consecutive vertices are colinear.
    This property is a necessary prerequisite for many advanced algorithms */
void VertexChain::canonicalize()
{
    if (m_vertices.size() == 0) return;
    
    bool closed = m_vertices.front() == m_vertices.back();
    if (closed)
    {
        // first==last causes to many special cases, so remove the duplicate(s) here add re-add one later.
        /* As a side-effect, this re-adding will only occur iff the polygon still has at least 3 vertices
         * after removing colinearities and consecutive duplicates. So only non-degenerated polygon will 
         * ever be closed, and closed polygons are guaranteed to never be degenerated.
        */
        m_vertices.pop_back(); 
    }

    if ( m_vertices.size() < 2) return; //segments of length 0 and 1 are always canonical
    
    if ( m_vertices.size() == 2)
    { //segments of length 2 cannot be colinear, only need to check if vertices are identical
        if (m_vertices.front() == m_vertices.back()) 
            m_vertices.pop_back();
        return;
    }
    assert( m_vertices.size() >= 3 );
    
    list<Vertex> verts( m_vertices.begin(), m_vertices.end() );
    
    list<Vertex>::iterator v3 = verts.begin();
    list<Vertex>::iterator v1 = v3++;
    list<Vertex>::iterator v2 = v3++;
    
    //at this point, v1, v2, v3 hold references to the first three vertices of the polygon
    
    //first part: find and remove colinear vertices
    while (v3 != verts.end())
    {
        assert ( (v1!=v2) && (v2!=v3) && (v3!=v1) );
    
        if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) //colinear
        {
            verts.erase(v2);
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
        if (verts.size() >= 3)
        {
            list<Vertex>::iterator v1 = --verts.end();
            list<Vertex>::iterator v2 = verts.begin(); 
            list<Vertex>::iterator v3 = ++verts.begin();
            if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) 
                verts.erase(v2);
        }

        // if we still have three vertices, even after potentially removing one in the previous step
        if (verts.size() >= 3)
        {
            list<Vertex>::iterator v1 = ----verts.end();
            list<Vertex>::iterator v2 = --verts.end(); 
            list<Vertex>::iterator v3 = verts.begin();
            if ( (*v2).pseudoDistanceToLine(*v1, *v3) == 0) 
                verts.erase(v2);
        }
        
    }

    //second part: remove consecutive identical vertices
    v1 = verts.begin();
    v2 = ++verts.begin();
    assert( v1!= v2);
    assert( verts.size() > 1);
    
    while (v2 != verts.end())
    {
        if (*v1 == *v2)
        {
            verts.erase(v1);
            v1 = v2++;
        } else
        {
            v1++;
            v2++;
        }
    }
    
    /* when relying only on front() == back() as a loop condition, the while-loop could delete even the final element,
     * and continue after that. But at that point, calls to front() and back() are invalid, and cause segfault.
     * Thus the additional check with m_vertices.size() */
    int num_vertices = verts.size();   // without caching this, the next line could degenerate to O(n²)
    while (verts.front() == verts.back() && (--num_vertices))
    {
        verts.pop_back();
    }
    
    if (closed && verts.size() >= 3)
        //re-duplicate the first vertex to again mark the polygon as closed 
        verts.push_back(verts.front());
        
    m_vertices = vector<Vertex>(verts.begin(), verts.end());
        
}
#endif
const Vertex&       VertexChain::front()     const  { return m_vertices.front(); }
const Vertex&       VertexChain::back()      const  { return m_vertices.back(); }
//const list<Vertex>& VertexChain::vertices()  const  { return m_vertices; }
const vector<Vertex>& VertexChain::data()   const { return m_vertices; }
void                VertexChain::append(const Vertex& node) { m_vertices.push_back(node); }

void VertexChain::append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end )
{
    m_vertices.insert(m_vertices.end(),begin, end);
}

void VertexChain::reverse()          
{ 
    uint64_t i = 0;
    uint64_t j = m_vertices.size()-1;
    
    while (i < j)
    {
        Vertex tmp = m_vertices[i];
        m_vertices[i] = m_vertices[j];
        m_vertices[j] = tmp;
    
        i++;
        j--;
    }
    
}


#if 0
bool VertexChain::isSimple() const
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
#endif



template<VertexCoordinate significantCoordinate, VertexCoordinate otherCoordinate> 
static void clip( const vector<Vertex> & polygon, BigInt clip_pos, list<VertexChain*> &above, list<VertexChain*> &below)
{
    assert( polygon.front() == polygon.back());

    VertexChain *current_seg = new VertexChain();
    vector<Vertex>::const_iterator v2 = polygon.begin();

    do { current_seg->append( *v2 ); v2++;}
    while ((significantCoordinate (current_seg->back()) == clip_pos) && (v2 != polygon.end()));
    
    if (v2 == polygon.end())    //edge case: all vertices lie on the clip line --> degenerate case, output is not a polygon
    {
        delete current_seg;
        return;    
    }
    
    bool isAbove = significantCoordinate( current_seg->back() ) <= clip_pos;
    vector<Vertex>::const_iterator v1 = v2;
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
        
        Vertex vClip(false);
        if      (significantCoordinate == getXCoordinate) vClip = Vertex(clip_pos, clip_other);
        else if (significantCoordinate == getYCoordinate) vClip = Vertex(clip_other, clip_pos);
        else  {assert(false && "unsupported coordinate accessor"); exit(0);}
        
        current_seg->append(vClip);
        
        
        if (isAbove)
            above.push_back(current_seg);
        else
            below.push_back(current_seg);

        isAbove = !isAbove;
        current_seg = new VertexChain();
        current_seg->append( Vertex(vClip) );
        current_seg->append( *v2);
    }
    if (isAbove) above.push_back(current_seg);
    else below.push_back(current_seg);
}

static void ensureOrientation( list<VertexChain> &in, list<VertexChain> &out, bool clockwise)
{
    for (uint64_t j = in.size(); j; j--)
    {
        VertexChain s = in.front();
        in.pop_front();
        if (! (clockwise == s.isClockwise()) )
            s.reverse();
        out.push_back(s);
    }
    assert(in.size() == 0);

}

//FIXME: notion of above/below is inverted
void VertexChain::clipSecondComponent( BigInt clip_y, list<VertexChain> &out_above, list<VertexChain> &out_below)
{
    list<VertexChain*> above, below;

    bool allAbove = true;
    bool allBelow= true;
    
    for (uint64_t i = 0; i < m_vertices.size(); i++)
    {
        allAbove &= m_vertices[i].get_y() <= clip_y;
        allBelow &= m_vertices[i].get_y() >= clip_y;
    }

    if (allAbove)
        out_above.push_back(*this);

    if (allBelow)
        out_below.push_back(*this);

    if (allAbove || allBelow)
        return;

    bool clockwise = isClockwise();
    
    clip<getYCoordinate, getXCoordinate>( m_vertices, clip_y, above, below);

    list<VertexChain> tmp_above, tmp_below;
    connectClippedSegments<getYCoordinate,getXCoordinate>(clip_y, above, tmp_above);
    connectClippedSegments<getYCoordinate,getXCoordinate>(clip_y, below, tmp_below);

    ensureOrientation(tmp_above, out_above, clockwise);
    ensureOrientation(tmp_below, out_below, clockwise);
}

void VertexChain::clipFirstComponent( BigInt clip_x, list<VertexChain> &out_left, list<VertexChain> &out_right)
{
    list<VertexChain*> left, right;
    
    bool allLeft = true;
    bool allRight= true;
    for (uint64_t i = 0; i < m_vertices.size(); i++)
    {
        allLeft &= m_vertices[i].get_x() <= clip_x;
        allRight&= m_vertices[i].get_x() >= clip_x;
    }

    if (allLeft)
        out_left.push_back(*this);

    if (allRight)
        out_right.push_back(*this);

    if (allLeft || allRight)
        return;

    bool clockwise = isClockwise();

    clip<getXCoordinate, getYCoordinate>( m_vertices, clip_x, left, right);

    list<VertexChain> tmp_left, tmp_right;
    connectClippedSegments<getXCoordinate,getYCoordinate>(clip_x, left,  tmp_left );
    connectClippedSegments<getXCoordinate,getYCoordinate>(clip_x, right, tmp_right);
    
    ensureOrientation(tmp_left, out_left, clockwise);
    ensureOrientation(tmp_right, out_right, clockwise);
}

bool VertexChain::isClockwise() { 
    /*static int i = 0;
    i++;
    
    if (i == 50859662)
    {
        cout << *this << endl;
        cout << "==============" << endl;
    }*/
    
    assert (m_vertices.front() == m_vertices.back() && "Clockwise test is defined for closed polygons only");
    canonicalize(); // otherwise, the minimum vertex and its two neighbors may be colinear, meaning that isClockwise won't work
    if (m_vertices.size() < 3) return false; //'clockwise' is only meaningful for closed polygons; and those require at least 3 vertices
    /*
    if (! isCanonicalPolygon())
    {
        std::cout << "non-canonical polygon at " << i << endl;
        cout << m_vertices << endl;

    }*/
    assert (isCanonicalPolygon());
    
    vector<Vertex>::const_iterator vMin = m_vertices.begin();
    
    m_vertices.pop_back();  //so that the modulus-arithmetic ignored the duplicate end vertex
    uint64_t min_pos = 0;
    for (uint64_t i = 0; i < m_vertices.size(); i++)
        if ( m_vertices[i] < m_vertices[min_pos]) min_pos = i;
         
    Vertex v = m_vertices[min_pos];
    Vertex vPred = m_vertices[ (min_pos - 1 + m_vertices.size()) % m_vertices.size()  ];   // '+ m_vertices.size()' for the edge case min_pos = 0;
    Vertex vSucc = m_vertices[ (min_pos + 1)                     % m_vertices.size()  ];
    
    /*if (v.pseudoDistanceToLine( vPred, vSucc) == 0)
    {
        cout << i << endl;
        cout << *this << endl;
        exit(0);
    }*/

    m_vertices.push_back( m_vertices.front());

    assert( v.pseudoDistanceToLine( vPred, vSucc) != 0 && "colinear vertices detected");
    return v.pseudoDistanceToLine( vPred, vSucc) < 0;
}


void VertexChain::mirrorX()
{
    for (uint64_t i = 0; i < m_vertices.size(); i++)
    {
        assert( m_vertices[i].get_x() != 0x80000000);
        m_vertices[i] = Vertex( -m_vertices[i].get_x(), m_vertices[i].get_y());
    }
}

/*list<VertexChain> VertexChain::toSimplePolygon()
{
    return toSimplePolygons(*this);
}*/


std::ostream& operator <<(std::ostream& os, const VertexChain &seg)
{
    const vector<Vertex> &verts = seg.data();
    for (vector<Vertex>::const_iterator it = verts.begin(); it != verts.end(); it++)
        os << *it << endl;
    return os;
}

