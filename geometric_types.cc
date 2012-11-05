
#include "geometric_types.h"

#include <queue>
#include <iostream>

#include <assert.h>
#include <stdlib.h> //for llabs
#include <math.h>

double Vertex::distanceToLine(const Vertex A, const Vertex B) const
{
    assert( B!= A);
    double num = fabs( ((double)B.x-A.x)*((double)A.y-y) - ((double)B.y-A.y)*((double)A.x-x));
    double denom = sqrt( ((double)B.x-A.x)*((double)B.x-A.x) + ((double)B.y-A.y)*((double)B.y-A.y));
    assert(denom != 0);
    return num / denom;
}

int64_t Vertex::pseudoDistanceToLine(const Vertex start, const Vertex end) const
{
    assert( end!= start);
    /** this computation should never overflow on OSM data, because its longitude (y-coordinate)
        needs only 31 bits to be stored */
        
    /* x-difference: 32bits plus sign bit; y-difference: 31bits plus sign bit
     * product of both: 31+32 = 63 bits plus sign bit*/
    return (end.x-start.x)*(start.y-y) - (end.y-start.y)*(start.x-x);
}


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}

/** =================================================*/

void PolygonSegment::append(const PolygonSegment &other, bool shareEndpoint)
{
    if (shareEndpoint) 
    {
        assert(back() == other.front());
        m_vertices.insert( m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
    } else
    {
        assert(back() != other.front()); //otherwise we would have two consecutive identical vertices
        cout << "concatenating line segments that are " 
             << sqrt( (back() -other.front()).squaredLength() )/100.0 << "m apart"<< endl;
        m_vertices.insert( m_vertices.end(),   other.m_vertices.begin(), other.m_vertices.end());
    }
    
    //std::cout << "Merged: " << res
    //return res;
}

bool PolygonSegment::simplifyArea(double allowedDeviation)
{
    assert( m_vertices.front() == m_vertices.back() && "Not a Polygon");

    list<Vertex>::iterator last = m_vertices.end();
    last--;
    simplifySection( m_vertices.begin(), last, allowedDeviation);
    
    // Need three vertices to form an area; four since first and last are identical
    if (m_vertices.size() < 4) return false;

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
    uint64_t max_dist = 0;
    Vertex A = *segment_first;
    Vertex B = *segment_last;
    
    list<Vertex>::iterator it  = segment_first;
    // make sure that 'it' starts from segment_first+1; segment_first must never be deleted
    for (it++; it != segment_last; it++)
    {
        uint64_t dist = (A == B) ? sqrt( (*it-A).squaredLength()) : it->distanceToLine(A, B);
        if (dist > max_dist) { it_max = it; max_dist = dist;}
    }
    if (max_dist == 0) return;
    
    if (max_dist < allowedDeviation) 
    {
        // no point further than 'allowedDeviation' from line A-B --> can delete all points in between
        segment_first++;
        // erase range includes first iterator but excludes second one
        m_vertices.erase( segment_first, segment_last);
    } else  //use point with maximum deviation as additional necessary point in the simplified polygon, recurse
    {
        simplifySection(segment_first, it_max, allowedDeviation);
        simplifySection(it_max, segment_last, allowedDeviation);
    }
}

void connectClippedSegmentsX( int32_t clip_x, list<PolygonSegment*> lst, list<PolygonSegment> &out)
{
    assert (lst.size() > 0);
    
    if (lst.size() ==1) // no clipping took place
    {
        out.push_back(*lst.front());
        delete lst.front();
        lst.pop_front();
        return;
    }
    
    /** the clipping algorithm starts at the first vertex of the polygon, which is 
      * not necessarily a polygon on the clipping line.
      * Thus, the first and last polygon segment may not end at the clipping line, but at the start/end vertex
        of the original (unclipped) PolygonSegment. These need to be joined together before the actual joining of
        clipping segments begins, because that joining assumes that all segments start and end at the clipping line */

    if ( lst.front()->vertices().front().x != clip_x && lst.back()->vertices().back().x != clip_x)
    {
        assert(lst.front()->vertices().front() == lst.back()->vertices().back() );
        lst.back()->append( *lst.front(), true);
        //cout << lst.front() << endl;
        delete lst.front();
        lst.pop_front();
    }
    
    priority_queue< pair< int32_t, PolygonSegment*> > queue;
    for ( list<PolygonSegment*>::const_iterator seg = lst.begin(); seg != lst.end(); seg++)
    {
        assert( ( (*seg)->vertices().front().x == clip_x) && ( (*seg)->vertices().back().x == clip_x));
        pair<int32_t, PolygonSegment*> p( max( (*seg)->vertices().front().y, (*seg)->vertices().back().y), *seg);
        queue.push( p);
    }
    
    
    while (queue.size())
    {
        PolygonSegment *seg1 = queue.top().second; 
        queue.pop();
        /** this is either the last remaining segment, or its endpoints are the two segment endpoints 
          * with the highest y-coordinates. Either way, make this segment form a closed polygon by itself
         **/
        if ((queue.size() == 0) || (queue.top().first < min(seg1->front().y, seg1->back().y)))  
        {
            //cout << "creating closed polygon out of single segment" << endl << *seg1 << endl;
            if (seg1->front() != seg1->back())
                seg1->append(seg1->vertices().front());
            out.push_back(*seg1);
            delete seg1;
            continue;
        }

        PolygonSegment *seg2 = queue.top().second; 
        queue.pop();
        assert( seg1 != seg2);
        //cout << "connecting two segments" << endl << *seg1 << endl << *seg2 << endl;
        if ( seg1->front().y > seg1->back().y) seg1->reverse();  //now seg1 ends with the vertex at which seg2 is to be appended
        if ( seg2->front().y < seg2->back().y) seg2->reverse();  //now seg2 starts with the vertex with which it is to be appended to seg1
        seg1->append(*seg2, false);
        delete seg2;
        
        queue.push( pair<int32_t, PolygonSegment*>(max(seg1->front().y, seg1->back().y), seg1));
    }

}
//bool less_p(const pair<int32_t, PolygonSegment*> &p1, const pair<int32_t, PolygonSegment*> &p2) { return p1.first < p2.first;}
void connectClippedSegmentsY( int32_t clip_y, list<PolygonSegment*> lst, list<PolygonSegment> &out)
{
    assert (lst.size() > 0);
    
    if (lst.size() ==1) // no clipping took place
    {
        out.push_back(*lst.front());
        delete lst.front();
        lst.pop_front();
        return;
    }
    
    /** the clipping algorithm starts at the first vertex of the polygon, which is 
      * not necessarily a polygon on the clipping line.
      * Thus, the first and last polygon segment may not end at the clipping line, but at the start/end vertex
        of the original (unclipped) PolygonSegment. These need to be joined together before the actual joining of
        clipping segments begins, because that joining assumes that all segments start and end at the clipping line */

    if ( lst.front()->vertices().front().y != clip_y && lst.back()->vertices().back().y != clip_y)
    {
        assert(lst.front()->vertices().front() == lst.back()->vertices().back() );
        lst.back()->append( *lst.front(), true);
        //cout << lst.front() << endl;
        delete lst.front();
        lst.pop_front();
    }
    
    priority_queue< pair< int32_t, PolygonSegment*> > queue;
    for ( list<PolygonSegment*>::const_iterator seg = lst.begin(); seg != lst.end(); seg++)
    {
        assert( ( (*seg)->vertices().front().y == clip_y) && ( (*seg)->vertices().back().y == clip_y));
        pair<int32_t, PolygonSegment*> p( max( (*seg)->vertices().front().x, (*seg)->vertices().back().x), *seg);
        queue.push( p);
    }
    
    
    while (queue.size())
    {
        PolygonSegment *seg1 = queue.top().second; 
        queue.pop();
        /** this is either the last remaining segment, or its endpoints are the two segment endpoints 
          * with the highest x-coordinates. Either way, make this segment form a closed polygon
         **/
        if ((queue.size() == 0) || (queue.top().first < min(seg1->front().x, seg1->back().x)))  
        {
            //cout << "creating closed polygon out of single segment" << endl << *seg1 << endl;
            if (seg1->front() != seg1->back())
                seg1->append(seg1->vertices().front());
            out.push_back(*seg1);
            delete seg1;
            continue;
        }

        PolygonSegment *seg2 = queue.top().second; 
        queue.pop();
        assert( seg1 != seg2);
        //cout << "connecting two segments" << endl << *seg1 << endl << *seg2 << endl;
        if ( seg1->front().x > seg1->back().x) seg1->reverse();  //now seg1 ends with the vertex at which seg2 is to be appended
        if ( seg2->front().x < seg2->back().x) seg2->reverse();  //now seg2 starts with the vertex with which it is to be appended to seg1
        seg1->append(*seg2, false);
        delete seg2;
        
        queue.push( pair<int32_t, PolygonSegment*>(max(seg1->front().x, seg1->back().x), seg1));
    }

}

void PolygonSegment::clipHorizontally( int32_t clip_y, list<PolygonSegment> &out_above, list<PolygonSegment> &out_below) const
{
    assert( m_vertices.front() == m_vertices.back());
    //TODO: handle the edge case that front() and back() both lie on the split line
    assert( m_vertices.front().y != clip_y || m_vertices.back().y != clip_y);

    // pairs of the highest x-coordinate of a segment end point, and the segment itself    
    list<PolygonSegment*> above, below;
    
    bool isAbove = m_vertices.front().y <= clip_y;
    PolygonSegment *current_seg = new PolygonSegment();
    current_seg->append(m_vertices.front());
    list<Vertex>::const_iterator v2 = m_vertices.begin(); v2++;

    for (list<Vertex>::const_iterator v1 = m_vertices.begin(); v2 != m_vertices.end(); v1++, v2++)
    {
        if ( (isAbove && v2->y <= clip_y) || (!isAbove && v2->y > clip_y))
            { current_seg->append(*v2); continue;} //still on the same side of the clipping line

        // if this point is reached, v1 and v2 are on opposite sides of the clipping line
                    
        assert(v2->y != v1->y);
        //line (v1, v2) must have intersected the clipping line
        int64_t clip_x = v1->x + ((clip_y - v1->y)*(v2->x - v1->x))/(v2->y-v1->y);
        current_seg->append(Vertex(clip_x, clip_y));
        
        if (isAbove)
        {
            above.push_back(current_seg);
        }
        else
        {
            below.push_back(current_seg);
        }
        isAbove = !isAbove;
        current_seg = new PolygonSegment();
        current_seg->append(Vertex(clip_x, clip_y));
        current_seg->append( *v2);
    }
    if (isAbove) above.push_back(current_seg);
    else below.push_back(current_seg);
    //cout << "Above:" << endl;
    if (above.size())
        connectClippedSegmentsY(clip_y, above, out_above);
    
    //cout << "Below:" << endl;
    if (below.size())
        connectClippedSegmentsY(clip_y, below, out_below);
}

void PolygonSegment::clipVertically( int32_t clip_x, list<PolygonSegment> &out_left, list<PolygonSegment> &out_right) const
{
    assert( m_vertices.front() == m_vertices.back());
    //TODO: handle the edge case that front() and back() both lie on the split line
    assert( m_vertices.front().x != clip_x || m_vertices.back().x != clip_x);

    // pairs of the highest x-coordinate of a segment end point, and the segment itself    
    list<PolygonSegment*> above, below;
    
    bool isAbove = m_vertices.front().x <= clip_x;
    PolygonSegment *current_seg = new PolygonSegment();
    current_seg->append(m_vertices.front());
    list<Vertex>::const_iterator v2 = m_vertices.begin(); v2++;

    for (list<Vertex>::const_iterator v1 = m_vertices.begin(); v2 != m_vertices.end(); v1++, v2++)
    {
        if ( (isAbove && v2->x <= clip_x) || (!isAbove && v2->x > clip_x))
            { current_seg->append(*v2); continue;} //still on the same side of the clipping line

        // if this point is reached, v1 and v2 are on opposite sides of the clipping line
                    
        assert(v2->x != v1->x);
        //line (v1, v2) must have intersected the clipping line
        int64_t clip_y = v1->y + ((clip_x - v1->x)*(v2->y - v1->y))/(v2->x-v1->x);
        current_seg->append(Vertex(clip_x, clip_y));
        
        if (isAbove)
        {
            above.push_back(current_seg);
        }
        else
        {
            below.push_back(current_seg);
        }
        isAbove = !isAbove;
        current_seg = new PolygonSegment();
        current_seg->append(Vertex(clip_x, clip_y));
        current_seg->append( *v2);
    }
    if (isAbove) above.push_back(current_seg);
    else below.push_back(current_seg);
    //cout << "Above:" << endl;
    if (above.size())
        connectClippedSegmentsX(clip_x, above, out_left);
    
    //cout << "Below:" << endl;
    if (below.size())
        connectClippedSegmentsX(clip_x, below, out_right);
}

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg)
{
    for (list<Vertex>::const_iterator it = seg.vertices().begin(); it != seg.vertices().end(); it++)
        os << *it << endl;
    return os;
}


/** ============================================================================= */
LineSegment::LineSegment( const Vertex v_start, const Vertex v_end, int32_t v_tag1, int32_t v_tag2): 
                        start(v_start), end(v_end), tag1(v_tag1), tag2(v_tag2) { assert(start != end);}
                        
LineSegment::LineSegment( int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t v_tag1, int32_t v_tag2): 
    start(Vertex(start_x, start_y)), end(Vertex(end_x, end_y)), tag1(v_tag1), tag2(v_tag2) { assert(start != end);}

bool LineSegment::contains(const Vertex v) const
{
    //test whether the vertex lies on the line (not necessarily the line segment)
    if (( (int64_t)v.x-start.x)*((int64_t)end.y - start.y) != 
        ( (int64_t)v.y-start.y)*((int64_t)end.x - start.x)) return false;
    
    uint64_t squared_seg_len = (end - start).squaredLength();
    uint64_t dist_start =      (v - start).squaredLength();
    uint64_t dist_end =        (v - end).squaredLength();
    return  dist_start <squared_seg_len && dist_end <= squared_seg_len;
}
                       
                                                  
bool LineSegment::intersects( const LineSegment &other) const
{
    if (parallelTo(other))
    {
        // if the two parallel line segments do not lie on the same line, they cannot intersect
        if (start.pseudoDistanceToLine(other.start, other.end) != 0) return false;
        return contains(other.start) 
            /*|| contains(other.end) */
            || other.contains(start) 
            /*|| other.contains(end )*/
            || ((start == other.end) && (end == other.start));
    } else
    {
        int64_t num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
        int64_t num2 = (      end.x-      start.x)*(start.y-other.start.y) - (      end.y-      start.y)*(start.x-other.start.x);
        int64_t denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);

        assert(denom != 0); //should only be zero if the lines are parallel, but this case has already been handled above

        //for the line segments to intersect, num1/denom and num2/denom both have to be inside the range [0, 1);
        
        //the absolute of at least one coefficient would be bigger than or equal to one 
        if (llabs(num1) >=llabs(denom) || llabs(num2) >= llabs(denom)) return false; 
        
        // at least one coefficient would be negative
        if (( num1 < 0 || num2 < 0) && denom > 0) return false; 
        if (( num1 > 0 || num2 > 0) && denom < 0) return false; // coefficient would be negative
        
        return true; // not negative and not bigger than/equal to one --> segments intersect
    }
    
}

double LineSegment::getIntersectionCoefficient( const LineSegment &other) const
{
    assert( !parallelTo(other));
    //TODO: handle edge case that two line segments overlap
    int64_t num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
    int64_t denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);

    assert(denom != 0); //should only be zero if the lines are parallel

    return num1/(double)denom;
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

#if 0
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
#endif
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

#if 0
void Polygon::canonicalize()
{
    assert( m_vertices[0] == m_vertices[m_num_vertices-1]);
    
    /** TODO: ensure polygon is oriented counter-clockwise 
              (i.e. the inside of the polygon is to the left of each line segment)
      * TODO: ensure that there are no self-intersections */
    // note that these goals cannot be solved independently:
    // - while self-intersections exist, there is no consistent orientation of the polygon
    // - while there is no consistent orientation, it is very hard to resolve self-intersections correctly
    
    
}
#endif

