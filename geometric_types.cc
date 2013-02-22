
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
    return asDouble(num*num) / asDouble(denom_sq);
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
bool Vertex::operator< (const Vertex &other) const { return (x < other.x) || ((x == other.x) && (y < other.y));}
bool Vertex::operator> (const Vertex &other) const { return (x > other.x) || ((x == other.x) && (y > other.y));}

Vertex Vertex::operator+(const Vertex &a) const { return Vertex(x+a.x, y+a.y);}
Vertex Vertex::operator-(const Vertex &a) const { return Vertex(x-a.x, y-a.y);}

//Vertex operator*(const int64_t a, const Vertex b) { return Vertex(a*b.x, a*b.y);}
//Vertex operator*(const BigInt &a, const Vertex &b) { return Vertex(a*b.x, a*b.y);}


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}

/** ============================================================================= */
LineSegment::LineSegment( const Vertex v_start, const Vertex v_end): 
                        start(v_start), end(v_end) { assert(start != end);}
                        
LineSegment::LineSegment( BigInt start_x, BigInt start_y, BigInt end_x, BigInt end_y): 
    start(Vertex(start_x, start_y)), end(Vertex(end_x, end_y)) { assert(start != end);}


bool LineSegment::isParallelTo( const LineSegment &other) const 
{ 
    return (end.x- start.x)*(other.end.y - other.start.y) ==  (end.y- start.y)*(other.end.x - other.start.x);
}


// @returns whether the vertex lies on the line (not necessarily on the line segment)
bool LineSegment::isColinearWith(const Vertex v) const
{
    return  (( v.x-start.x)*(end.y - start.y) == ( v.y-start.y)*(end.x - start.x));
}

// in accordance with the semantics of line segments, intersects() also returns 'true' is the segments only share a single point
bool LineSegment::intersects( const LineSegment &other) const
{
    if (isParallelTo(other))
    {
        // if the two parallel line segments do not lie on the same line, they cannot intersect
        if (! isColinearWith(other.start)) return false;
        assert( isColinearWith(other.end) );
            
        // if they do lie on the same line, they intersect if they overlap
        static const BigFraction zero(0,1); // (0/1)
        static const BigFraction one(1,1);  // (1/1)
        
        BigFraction c1 = getCoefficient(other.start);
        BigFraction c2 = getCoefficient(other.end);
        
        return !((c1 < zero && c2 < zero) || (c1 > one && c2 > one )); //no overlap iff 'other' lies completely to one side of 'this'
        
    } else
    {
        BigInt num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
        BigInt num2 = (      end.x-      start.x)*(start.y-other.start.y) - (      end.y-      start.y)*(start.x-other.start.x);
        BigInt denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);

        assert(denom != 0); //should only be zero if the lines are parallel, but this case has already been handled above

        //for the line segments to intersect, num1/denom and num2/denom both have to be inside the range [0, 1];
        
        //if the absolute of at least one coefficient would be bigger than one 
        if (abs(num1) >abs(denom) || abs(num2) > abs(denom)) return false; 
        
        // at least one coefficient would be negative
        if (( num1 < 0 || num2 < 0) && denom > 0) return false; 
        if (( num1 > 0 || num2 > 0) && denom < 0) return false; // coefficient would be negative
        
        return true; // not negative and not bigger than/equal to one --> segments intersect
    }
    
}

void LineSegment::getIntersectionCoefficient( const LineSegment &other, BigInt &out_num, BigInt &out_denom) const
{
    if (isParallelTo(other))
    {
        assert ( other.start.pseudoDistanceToLine(start, end) == 0 && 
                 "Trying to determine intersection coefficient of non-intersecting parallel lines");
        out_denom = 1;  //result can only be either 0 or 1, so the denominator is always 1
        if ((start == other.end)|| (start == other.start)) 
            out_num = 0;
        else if ((end   == other.end)|| (end   == other.start)) 
            out_num = 1;
        else 
            assert(false && "No intersection coefficient exists for two line segments overlapping in more than a single point");

        return;
    }
    //TODO: handle edge case that two line segments overlap
    out_num = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
    out_denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);
    
    assert(out_denom != 0 && "Line segments are parallel or coincide" );

}

static Vertex getRoundedIntersectionPoint(const LineSegment &A, const LineSegment &B)
{
    assert (A.start.x <= A.end.x && B.start.x <= B.end.x);

    BigInt num, denom;
    
    A.getIntersectionCoefficient(B, num, denom);
    assert ( ((num >= 0 && denom > 0 && num <= denom) || (num <= 0 && denom < 0 && num >= denom)) && "Do not intersect");
    /*int64_t iNum = (int64_t)num;
    int64_t iDenom = (int64_t)denom;
    assert (iNum == num && iDenom == denom);*/
    
    
    Vertex v( (A.start.x + num * (A.end.x - A.start.x) / denom), (A.start.y + num * (A.end.y - A.start.y) / denom) );
    /** make sure that the intersection position is rounded *up* to the next integer
      * first, this is only necessary if the division num/denom has a remainder - otherwise the result is already exact
      * second, it is only necessary if the slope in the respective direction is positive
      *     - if it was zero, x/y would be constant over the lenght of the whole line and does not need to be adjusted
      *     - if it was negative, the formula above already computed a value that is rounded up
      *
      **/
    if (num % denom != 0)    
    {
        if (A.end.x > A.start.x) v.x = v.x + 1;
        if (A.end.y > A.start.y) v.y = v.y + 1;
    }
    
    return v;
}


Vertex LineSegment::getRoundedIntersection(const LineSegment &other) const
{
    LineSegment A = start < end ? LineSegment(start, end) : LineSegment(end, start);
    LineSegment B = other.start < other.end ? LineSegment(other.start, other.end) : LineSegment(other.end, other.start);
    return getRoundedIntersectionPoint( A, B);
 }

/*
double LineSegment::getIntersectionCoefficient( const LineSegment &other) const
{
    BigInt num, denom;
    getIntersectionCoefficient(other, num, denom);
    return num.toDouble()/denom.toDouble();
}*/


bool LineSegment::operator<(const LineSegment &other) const
{
    return (start < other.start) ||
           ((start == other.start) && (end < other.end));
}

bool LineSegment::operator==(const LineSegment &other) const
{
    return (start == other.start) && (end == other.end);
}

bool LineSegment::operator!=(const LineSegment &other) const
{
    return (start != other.start) || (end != other.end);
}


BigFraction LineSegment::getCoefficient(const Vertex v) const
{
    assert (start != end);
    
    if (start.x == end.x)
    {
        assert( v.x == start.x && "vertex does not lie on line segment");
        return BigFraction( v.y-start.y, end.y-start.y);
    } else if (start.y == end.y)
    {
        assert( v.y == start.y && "vertex does not lie on line segment");
        return BigFraction( v.x-start.x, end.x-start.x);
    }
    
    BigFraction c1(v.y-start.y, end.y-start.y);
#ifndef NDEBUG
    BigFraction c2(v.x-start.x, end.x-start.x);
    assert( c1 == c2 && "incorrect coefficient computation");
#endif
    return c1;
}

bool LineSegment::overlapsWith(const LineSegment &other) const
{
    if (!isParallelTo(other)) return false;
    if (!isColinearWith(other.start)) return false;
    assert( isColinearWith(other.end)); // if parallel and one point is colinear to 'this', the other point has to be, too
    static const BigFraction zero(0,1); // (0/1)
    static const BigFraction one(1,1);  // (1/1)
    
    BigFraction c1 = getCoefficient(other.start);
    BigFraction c2 = getCoefficient(other.end);
    
    if ((c1 < zero && c2 <= zero) || (c1 <= zero && c2 <  zero)) return false; //if both segments have at most one point in common at lower end of 'this'
    if ((c1 >= one && c2 >   one) || (c1 >  one  && c2 >= one )) return false; //... or at the upper end of 'this'
    
    return true;
    
}

LineSegment::operator bool() const
{
    //static const Vertex zero(0,0);
    return (start.x != 0 || start.y != 0 || end.x != 0 || end.y != 0);;
}
/** ============================================================================= */

/* resolves the overlap of two line segments by shortening one of them. 
 * This may result in one segment being completely destroyed (=invalidated).
 * It is guaranteed that this algorithm never extends a segment, it only shortens them
 */
bool resolveOverlap(LineSegment &A, LineSegment &B)
{
    if (! A.isParallelTo(B)) return false;
    
    
    BigFraction c1( A.getCoefficient(B.start));
    BigFraction c2( A.getCoefficient(B.end));
    static const BigFraction zero(0,1);
    static const BigFraction one(1,1);

    if ((c1 <= zero && c2 <= zero) || (c1 >= one && c2 >= one)) return false; //no overlap
        
    if ((c1 >= zero && c1 <= one) && (c2 >= zero && c2 <= one)) // B is completely contained in A
    {  
        B = LineSegment(); //invalidate B
        return true;
    } 

    if ((c1 <= zero && c2 >= one) || (c2 <= zero && c1 >= one)) // A is completely contained in B
    {
        A = LineSegment(); //invalidate A
        return true;
    }
    
    //at this point, exactly one of the endpoints of B lies in A
    bool start_in_A = (c1 > zero && c1 < one);
    bool end_in_A   = (c2 > zero && c2 < one);
    
    assert(  start_in_A ^ end_in_A); 
    
    if (start_in_A)
    {
        if      ( c2 < zero )  { B.start = A.start; return true;}
        else if ( c2 > one  )  { B.start = A.end; return true; }
        else assert(false);
    }
    else if (end_in_A)
    {
        if      ( c1 < zero ) { B.end = A.start; return true;}
        else if ( c1 > one  ) { B.end = A.end; return true; }
        else assert(false);
    }
    else assert(false);
    return false;
}

/* Receives two line segments and resolves a possible intersection or overlap between them. 
 * This is done by splitting (in the case of intersections) or shortening (in the case of 
 * overlaps) the segment when necessary*/

void handleIntersection(LineSegment &seg1, LineSegment &seg2, LineSegment &seg1_aux, LineSegment &seg2_aux, 
                        bool &seg1_modified, bool &seg2_modified)
{
    seg1_aux = LineSegment();
    seg2_aux = LineSegment();

    if (seg1.overlapsWith(seg2))  //overlapping line segments would lead to an infinite number of intersections, so this case must be handled individually
    {
        //std::cout << "overlap detected " << std::endl;
        LineSegment seg1_tmp = seg1;
        LineSegment seg2_tmp = seg2;
        resolveOverlap(seg1, seg2);   //resolves the overlap, but may invalidate either segment.
        
        seg1_modified = (seg1 != seg1_tmp);
        seg2_modified = (seg2 != seg2_tmp);
        return;
    }

    if ( (!seg1.intersects(seg2)) || ( seg1.isParallelTo(seg2) ))
    {   //do not intersect or only share endpoints
        seg1_modified = seg2_modified = false;
        return;
    }

    //std::cout << "Intersection " << num_ints << " between " << (*seg1) << " and " << (*seg2) << std::endl;
    Vertex intersect = seg1.getRoundedIntersection(seg2);

    seg1_modified = (intersect != seg1.start) && (intersect != seg1.end);
    if (seg1_modified) 
    {
        seg1_aux = LineSegment(seg1.start, intersect);
        seg1     = LineSegment(intersect, seg1.end);
    }
    
    seg2_modified = (intersect != seg2.start) && (intersect != seg2.end);
    if (seg2_modified) 
    {
        seg2_aux = LineSegment(seg2.start, intersect);
        seg2     = LineSegment(intersect, seg2.end);
    }
}

/* This method modifies the set of line segments so that all intersections occur at integer coordinates.
   
   To that end, intersecting segments are split into two segments at the intersection
   point (start-intersect, intersect-end) and the intersection point is moved to the nearest integer
   coordinate. As a side-effect, intersections now occur only at line segment end points.
*/
void moveIntersectionsToIntegerCoordinates(list<LineSegment> &segments)
{
    list<LineSegment>::iterator seg1 = segments.begin();
    while (seg1 != segments.end())
    {
        list<LineSegment>::iterator seg2 = segments.begin(); //must not start at (seg1)+1, because new segments may be added in the process, and these need to be checked against all other segments
               
        bool seg1_needs_increment = true;
        while (seg2!= segments.end())
        {
            if (seg2 == seg1)  { (seg2++); continue; }
            //std::cout << "testing ["  << distance(segments.begin(), seg1) << "] " << *seg1 << ", [" 
            //                          << distance(segments.begin(), seg2) << "] " << *seg2 <<  endl;
        
            LineSegment seg1_aux, seg2_aux;
            bool seg1_modified, seg2_modified;
            handleIntersection (*seg1, *seg2, seg1_aux, seg2_aux, seg1_modified, seg2_modified);
            /*
            if (seg1_modified) cout << "\t seg1 is now " << *seg1 << endl;
            if (seg1_aux)      cout << "\t added seg1_aux " << seg1_aux << endl;
            if (! (bool)*seg1) cout << "\t removing seg1 " << *seg1 << endl;

            if (seg2_modified) cout << "\t seg2 is now " << *seg2 << endl;
            if (seg2_aux)      cout << "\t added seg2_aux " << seg2_aux << endl;
            if (! (bool)*seg2) cout << "\t removing seg2 " << *seg2 << endl;
            */
            if (seg2_aux) segments.push_back(seg2_aux); //if an auxiliary to seg2 exists (=there was an intersection), add it to the list of segments
            if (!*seg2) assert (seg2_modified);
            if (seg2_modified)
            {   
                if ( (bool)*seg2) segments.push_back(*seg2); //needs to be checked against all other segments --> put to end of the list
                seg2 = segments.erase(seg2); 
                
            } else seg2++;
            
            if (seg1_aux) segments.push_back(seg1_aux);
            if (!*seg1) assert (seg1_modified);
            if (seg1_modified)
            {
                if (!(bool)*seg1) seg1 = segments.erase(seg1);
                //if seg1 was deleted in the previous step: iterator has already been incremented by this deletion
                //if seg1 was not deleted (but has been modified) --> needs to be checked against all other segments again --> next iteration without increment of iterator
                seg1_needs_increment = false;    
                break;
            }
        }
        
        if (seg1_needs_increment)
            seg1++;
    }

}

struct QuadTreeNode
{
    QuadTreeNode(): tl(NULL), tr(NULL), bl(NULL), br(NULL) {}
    QuadTreeNode *tl, *tr, *bl, *br;
    list<LineSegment> segments;
};

void addToQuadrant(BigInt mid_x, BigInt mid_y, LineSegment seg, list<LineSegment> &tl,
                   list<LineSegment> &tr, list<LineSegment> &bl, list<LineSegment> &br,
                   list<LineSegment> &here)
{
    if (seg.start.x < mid_x && seg.end.x < mid_x && seg.start.y < mid_y && seg.end.y < mid_y)
        tl.push_front(seg);
    else if (seg.start.x < mid_x && seg.end.x < mid_x && seg.start.y > mid_y && seg.end.y > mid_y)
        bl.push_front(seg);
    else if (seg.start.x > mid_x && seg.end.x > mid_x && seg.start.y < mid_y && seg.end.y < mid_y)
        tr.push_front(seg);
    else if (seg.start.x > mid_x && seg.end.x > mid_x && seg.start.y > mid_y && seg.end.y > mid_y)
        br.push_front(seg);
    else
        here.push_back(seg);    //crosses more than one quadrant
}

void moveIntersectionsRec(QuadTreeNode &node, AABoundingBox box, set<LineSegment> &segs, int depth = 0)
{
    for (int d = depth; d; d--)
        cout << " ";
    cout << "(" << box.left << ", " << box.top << ") - (" << box.right << ", " << box.bottom << ")" << endl;

    BigInt mid_x = (box.right + box.left) / 2;
    BigInt mid_y = (box.top + box.bottom) / 2;
    
    list<LineSegment> here, tl, tr, bl, br;
    while (node.segments.begin() != node.segments.end())
    {
        LineSegment seg = node.segments.front();
        node.segments.pop_front();
        addToQuadrant(mid_x, mid_y, seg, tl, tr, bl, br, here);
   }
   
   for (list<LineSegment>::const_iterator it = here.begin(); it != here.end(); it++)
   {
        list<LineSegment>::const_iterator it2 = it;
        for (it2++; it2 != here.end(); it2++)
        {
            
        }
   }
   cout << "tl:" << tl.size() << ", tr:" << tr.size() << ", bl:" << bl.size() << ", br:" << br.size() 
        << ", here:" << here.size() << endl;
}

void moveIntersectionsToIntegerCoordinates2(list<LineSegment> &segments)
{
    AABoundingBox box (segments.begin()->start);
    set<LineSegment> segs;
    QuadTreeNode root;
    for (list<LineSegment>::const_iterator it = segments.begin(); it != segments.end(); it++)
    {
        box += it->start;
        box += it->end;

    }
    //segs.insert( *it );    
    root.segments.insert(root.segments.begin(), segments.begin(), segments.end());
    
    moveIntersectionsRec( root, box, segs, 0);
   
}

map<Vertex,set<Vertex> > getConnectivityGraph(const list<LineSegment> &segments )
{
    map<Vertex, set<Vertex> > graph;
    
    for (list<LineSegment>::const_iterator seg = segments.begin(); seg != segments.end(); seg++)
    {
        if (graph.count( seg->start ) == 0) graph.insert( pair<Vertex, set<Vertex>>(seg->start, set<Vertex>()));
        if (graph.count( seg->end   ) == 0) graph.insert( pair<Vertex, set<Vertex>>(seg->end,   set<Vertex>()));
        
        graph[seg->start].insert( seg->end);
        graph[seg->end].insert( seg->start);
    }
    
    return graph;
}

bool intersectionsOnlyShareEndpoint(const list<LineSegment> &segments)
{
    for (list<LineSegment>::const_iterator seg1 = segments.begin(); seg1 != segments.end(); seg1++)
    {
        list<LineSegment>::const_iterator seg2 = seg1;
        for (seg2++; seg2 != segments.end(); seg2++)
        {
            //cout << "testing edges " << *seg1 << " and " << *seg2;
            if (seg1->intersects(*seg2))
            {
                //cout << "intersect" << endl;
                Vertex v = seg1->getRoundedIntersection(*seg2);
                if (( v != seg1->start && v != seg1->end) || ( v != seg2->start && v != seg2->end)) 
                {
                    //cout << "intersecting edges at " << *seg1 << " and " << *seg2 << endl;
                    return false;
                }
            }
            //cout << endl;
        }
    }
    return true;
}

map<LineSegment, list<LineSegment>> findIntersectionsBruteForce(const list<LineSegment> &segments)
{
    map<LineSegment, list<LineSegment>> intersections;
    for (list<LineSegment>::const_iterator seg1 = segments.begin(); seg1 != segments.end(); seg1++)
    {
        list<LineSegment>::const_iterator seg2 = seg1;
        for (seg2++; seg2 != segments.end(); seg2++)
            if (seg1->intersects(*seg2))
            {
#ifndef NDEBUG
                Vertex v = seg1->getRoundedIntersection(*seg2);
                assert ( v == seg1->start || v == seg1->end);
                assert ( v == seg2->start || v == seg2->end);
#endif
                if (!intersections.count(*seg1)) intersections.insert(pair<LineSegment,list<LineSegment> >(*seg1, list<LineSegment>() ));
                if (!intersections.count(*seg2)) intersections.insert(pair<LineSegment,list<LineSegment> >(*seg2, list<LineSegment>() ));
                
                intersections[*seg1].push_back(*seg2);
                intersections[*seg2].push_back(*seg1);
            }
    }
    return intersections;
}

#if 0
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
//    assert ( isNormalized(*this) && isNormalized(other));
    return (max( left, other.left) <= min( right, other.right)) && 
           (max( top, other.top)   <= min( bottom, other.bottom));
}

BigInt AABoundingBox::width()  const { return right - left;}
BigInt AABoundingBox::height() const { return bottom - top;}

//=======================================================================


