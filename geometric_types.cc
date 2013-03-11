
#include "geometric_types.h"

#include <queue>
#include <iostream>

//#include <boost/foreach.hpp>

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
    os << "( " << (int64_t)v.x << ", " << (int64_t)v.y << ")";
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

std::ostream& operator <<(std::ostream &os, const LineSegment edge)
{
    os << edge.start  << " - " << edge.end;
    return os;
}

// @returns whether the vertex lies on the line (not necessarily on the line segment)
bool LineSegment::isColinearWith(const Vertex v) const
{
    return  (( v.x-start.x)*(end.y - start.y) == ( v.y-start.y)*(end.x - start.x));
}

bool LineSegment::intersects( const LineSegment &other) const
{
    BigInt dummy;
    return intersects(other, dummy, dummy);
}

bool LineSegment::intersects( const LineSegment &other, BigFraction &intersect_x_out, BigFraction &intersect_y_out) const
{
    BigInt num1, denom;
    if (! intersects(other, num1, denom)) return false;
    
    BigFraction alpha(num1, denom);
    intersect_x_out = alpha * (end.x - start.x) + start.x;
    intersect_y_out = alpha * (end.y - start.y) + start.y;
    return true;
}


// in accordance with the semantics of line segments, intersects() also returns 'true' if the segments only share a single point
bool LineSegment::intersects( const LineSegment &other, BigInt &num1_out, BigInt &denom_out) const
{

    BigInt odx = other.end.x-other.start.x;
    BigInt ody = other.end.y-other.start.y;
    BigInt tdx =       end.x-      start.x;
    BigInt tdy =       end.y-      start.y;

    BigInt denom= ody * tdx - odx * tdy;
    bool areParallel = (denom == 0);

    if (areParallel)
    {

        // if the two parallel line segments do not lie on the same line, they cannot intersect
        bool colinearWithStart = ( other.start.x-start.x)*(tdy) == ( other.start.y-start.y)*(tdx);
        if (! colinearWithStart) return false;
        assert( ( other.end.x-start.x)*(tdy) == ( other.end.y-start.y)*(tdx) );
            
        // if they do lie on the same line, they intersect if they overlap
        static const BigFraction zero(0,1); // (0/1)
        static const BigFraction one(1,1);  // (1/1)
        
        BigFraction c1 = getCoefficient(other.start);
        BigFraction c2 = getCoefficient(other.end);
        
        return !((c1 < zero && c2 < zero) || (c1 > one && c2 > one )); //no overlap iff 'other' lies completely to one side of 'this'
        
    } else
    {
      /*BigInt num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
        BigInt num2 = (      end.x-      start.x)*(start.y-other.start.y) - (      end.y-      start.y)*(start.x-other.start.x);
        BigInt denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);*/

        /*optimization: pre-computed all multiple-used differences */
        BigInt dsx = start.x    -other.start.x;
        BigInt dsy = start.y    -other.start.y;

        BigInt num1 = odx * dsy - ody * dsx;
        BigInt num2 = tdx * dsy - tdy * dsx;
        

        assert(denom != 0); //should only be zero if the lines are parallel, but this case has already been handled above

        //for the line segments to intersect, num1/denom and num2/denom both have to be inside the range [0, 1];
        
        //if the absolute of at least one coefficient would be bigger than one 
        BigInt adenom = abs(denom);
        if (abs(num1) >adenom || abs(num2) > adenom) return false; 
        
        // at least one coefficient would be negative
        if (( num1 < 0 || num2 < 0) && denom > 0) return false; 
        if (( num1 > 0 || num2 > 0) && denom < 0) return false; // coefficient would be negative
        
        num1_out = num1;
        denom_out= denom;
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
        {
            assert(false && "No intersection coefficient exists for two line segments overlapping in more than a single point");
            out_denom = -1;
        }

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


/** returns whether two line segments overlap by more than a single point.
  * As a necessary condition for overlap, the two segments have to be parallel */
bool LineSegment::overlapsWith(const LineSegment &other) const
{
    BigInt odx = other.end.x-other.start.x;
    BigInt ody = other.end.y-other.start.y;
    BigInt tdx =       end.x-      start.x;
    BigInt tdy =       end.y-      start.y;

    BigInt denom= ody * tdx - odx * tdy;
    bool areParallel = (denom == 0);


    if (!areParallel) return false;
    
    bool colinearWithStart = ( other.start.x-start.x)*(tdy) == ( other.start.y-start.y)*(tdx);
    if (! colinearWithStart) return false;
    // if parallel and one point is colinear to 'this', the other point has to be, too
    assert( ( other.end.x-start.x)*(tdy) == ( other.end.y-start.y)*(tdx) );

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


/*
    Tests segments in 'segments' against each other. Whenever a test results in an overlap or an intersection,
    this is resolved by shortening a segment (for overlaps) or splitting segments at the intersection point
    and moving that point to the nearest integer coordinates (for intersections)
    
    Whenever a segment is modified or added in the process, it is transferred to newSegments.
    Thus, after this method has been executed, the segments remaining in 'segments' have all tested against each other
    and do not intersect or overlap each other. The segments in 'newSegments' have not been tested in any way
 */
static void testSegments(list<LineSegment> &segments, list<LineSegment> &newSegments)
{
    list<LineSegment>::iterator seg1 = segments.begin();
    while (seg1 != segments.end())
    {
        list<LineSegment>::iterator seg2 = seg1;
        seg2++;
        bool seg1_needs_increment = true;
        while (seg2 != segments.end())
        {
            assert (seg2 != seg1);
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
            if (seg2_aux) newSegments.push_back(seg2_aux); //if an auxiliary to seg2 exists (=there was an intersection), add it to the list of segments
            if (!*seg2) assert (seg2_modified);
            if (seg2_modified)
            {   
                if ( (bool)*seg2) newSegments.push_back(*seg2); //needs to be checked against all other segments --> put to end of the list
                seg2 = segments.erase(seg2);
                
            } else seg2++;
            
            if (seg1_aux) newSegments.push_back(seg1_aux);
            if (!*seg1) assert (seg1_modified);
            if (seg1_modified)
            {
                if ((bool)*seg1) newSegments.push_back(*seg1);
                seg1 = segments.erase(seg1);
                //if seg1 was deleted in the previous step: iterator has already been incremented by this deletion
                seg1_needs_increment = false;    
                break;
            }
        }
        
        if (seg1_needs_increment)
            seg1++;
    }
}

static void crossTestSegments(list<LineSegment> &edges1, list<LineSegment> &edges2, list<LineSegment> &edgesOut)
{
    list<LineSegment>::iterator edge1 = edges1.begin();
    while (edge1 != edges1.end())
    {
        list<LineSegment>::iterator edge2 = edges2.begin();
        bool edge1_needs_increment = true;
        while (edge2 != edges2.end())
        {
            LineSegment edge1_aux, edge2_aux;
            bool edge1_modified, edge2_modified;
            handleIntersection (*edge1, *edge2, edge1_aux, edge2_aux, edge1_modified, edge2_modified);

            if (edge2_aux) edgesOut.push_back(edge2_aux); //if an auxiliary to seg2 exists (=there was an intersection), add it to the list of segments
            if (!*edge2) assert (edge2_modified);
            if (edge2_modified)
            {   
                if ( (bool)*edge2) edgesOut.push_back(*edge2); //needs to be checked against all other segments --> put to end of the list
                edge2 = edges2.erase(edge2);   
            } else edge2++;
            
            if (edge1_aux) edgesOut.push_back(edge1_aux);
            if (!*edge1) assert (edge1_modified);
            if (edge1_modified)
            {
                if ((bool)*edge1) edgesOut.push_back(*edge1);
                edge1 = edges1.erase(edge1);
                //if edge1 was deleted in the previous step: iterator has already been incremented by this deletion
                edge1_needs_increment = false;    
                break;
            }
        }
        
        if (edge1_needs_increment)
            edge1++;
    }
}


/*FIXME: instead of move around segments (which involves deleting and creating list node to hold these segments), 
 * use list::splice() to directly move the list nodes containing the segments */

/* This method modifies the set of line segments so that all intersections occur at integer coordinates.
   
   To that end, intersecting segments are split into two segments at the intersection
   point (start-intersect, intersect-end) and the intersection point is moved to the nearest integer
   coordinate. As a side-effect, intersections now occur only at line segment end points.
*/
void moveIntersectionsToIntegerCoordinates(list<LineSegment> &segments)
{
    list<LineSegment> untested, untested2;
    testSegments(segments, /*out*/untested);
    do
    {
        //at this point, no edge in 'segments' intersects any other edge in 'segments', but those in 'untested' do
        cout << "segments: " << segments.size() << ", untested: " << untested.size() << endl;   
        testSegments(untested, /*out*/untested2);
        //now no edge in 'untested' intersects any other edge in 'untested', but those in 'untested2' do
        
        crossTestSegments(segments, untested, /*out*/untested2);
        //now no edge in 'segments' or 'untested' intersects with any other edge from the two containers
        
        segments.insert(segments.end(), untested.begin(), untested.end());
        untested.clear();
        swap(untested, untested2);
        /*no edge in 'segments' intersects any other edge in that container; 
         *edges in 'untested' have not yet been tested
         *untested2 is empty */
        assert(untested2.size() == 0);
        
    } while (untested.begin() != untested.end() );
}




/* approach:
    filter 'untested' (move all segments that fit into a single quadrant into the corresponding quadrant container)
    while "untested" is not empty:
        test all 'untested' against each other --> 'untested' (all cross-tested); 'untested2' (newly created segments)
        cross-test all 'untested' against all 'segments' --> 'untested (cross-tested and tested against 'segments); added to untested2 (newly created segments)
        cross-test all 'untested' against all segments of sub-containers --> 'untested' (all fully tested); added to 'untested2' (newly created segments)
        filter 'untested2'
        add all 'untested' to 'segments'
        move all 'untested2' to 'untested'
        
    call recursively for all four quadrants
 */
 
//void crossTestIntersectionsRec(list<LineSegment> src, 
#if 0
void moveIntersectionsRec(QuadTreeNode &node, AABoundingBox box, int depth = 0)
{
    std::string strSpaces;
    for (int d = depth; d; d--)
        strSpaces+= "  ";
    cout << strSpaces << "(" << box.left << ", " << box.top << ") - (" << box.right << ", " << box.bottom << "); " <<node.untested.size() << " segments" <<  endl;

    BigInt mid_x = (box.right + box.left) / 2;
    BigInt mid_y = (box.top + box.bottom) / 2;
    
    list<LineSegment> untested2;//, tl, tr, bl, br;
    assert (!node.tl && !node.tr && !node.bl && !node.br);
    node.tl = new QuadTreeNode();
    node.tr = new QuadTreeNode();
    node.bl = new QuadTreeNode();
    node.br = new QuadTreeNode();
    
    
    //first step: move those segments to lower recursion levels that fit conpletely in one quadrant
    node.distributeSegments( node.untested, mid_x, mid_y );
    
    cout << strSpaces << "#" << node.untested.size() << "/ " << node.tl->untested.size() << "/ " 
         << node.tr->untested.size() << "/ " << node.bl->untested.size() << "/ " << node.br->untested.size() <<  endl;
   
    //second step: determine intersections between top-level segments

    while (node.untested.begin() != node.untested.end())
    {
        testSegments( node.untested, untested2);
        crossTestSegments(node.segments, node.untested, /*out*/  untested2);
        node.distributeSegments( untested2, mid_x, mid_y);
        node.segments.splice(node.segments.begin(), node.untested, node.untested.begin(), node.untested.end());
        node.untested.clear();
        swap(node.untested, untested2);
        assert(untested2.size() == 0);
        cout << strSpaces << "#segs: " << node.segments.size() << ", untested: " << node.untested.size() << " sub-quads: " << node.tl->untested.size() << "/ " 
             << node.tr->untested.size() << "/ " << node.bl->untested.size() << "/ " << node.br->untested.size() <<  endl;
    }
     
     
    return;
    
/*    for (list<LineSegment>::iterator seg1 = here.begin(); seg1 != here.end(); seg1++)
    {
        list<LineSegment>::iterator seg2 = seg1;
        for (seg2++; seg2 != here.end(); seg2++)
        {
            LineSegment seg1_aux, seg2_aux;
            bool seg1_modified, seg2_modified;
            handleIntersection (*seg1, *seg2, seg1_aux, seg2_aux, seg1_modified, seg2_modified);
        }
    }
    cout << "tl:" << tl.size() << ", tr:" << tr.size() << ", bl:" << bl.size() << ", br:" << br.size() 
        << ", here:" << here.size() << endl;*/
}

void moveIntersectionsToIntegerCoordinates2(list<LineSegment> &segments)
{
    AABoundingBox box (segments.begin()->start);
    //list<LineSegment> segs;
    QuadTreeNode root;
    for (list<LineSegment>::const_iterator it = segments.begin(); it != segments.end(); it++)
    {
        box += it->start;
        box += it->end;
    }
    //segs.insert( *it );    
    root.untested.insert(root.untested.begin(), segments.begin(), segments.end());
    
    moveIntersectionsRec( root, box, 0);
   
}
#endif


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
                    cout << "intersecting edges " << *seg1 << " and " << *seg2 << " at " << v << endl;
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

AABoundingBox::AABoundingBox(const Vertex v) { tl.x = br.x = v.x; tl.y = br.y =v.y;}
AABoundingBox::AABoundingBox(Vertex tl_, Vertex br_): tl(tl_), br(br_) { assert(tl.x < br.x); assert(tl.y < br.y); }
AABoundingBox & AABoundingBox::operator+=(const Vertex v) {
    if (v.x < tl.x) tl.x = v.x;
    if (v.x > br.x) br.x = v.x;
    if (v.y < tl.y) tl.y = v.y;
    if (v.y > br.y) br.y = v.y;
    return *this;
}

std::ostream& operator <<(std::ostream& os, const AABoundingBox box)
{
    os << box.tl << " - " << box.br;
    return os;
}

#ifndef NDEBUG
static bool isNormalized( const AABoundingBox &box)
{
    return box.tl.x < box.br.x && box.tl.y < box.br.y;
}
#endif

AABoundingBox AABoundingBox::getOverlap(const AABoundingBox &other) const
{
    assert ( isNormalized(*this) && isNormalized(other));
    BigInt new_left = max( tl.x, other.tl.x);
    BigInt new_right= min( br.x, other.br.x);
    assert(new_left <= new_right);
    
    BigInt new_top  = max( tl.y, other.tl.y);
    BigInt new_bottom=min( br.y, other.br.y);
    assert(new_top <= new_bottom);
    
    AABoundingBox box(Vertex(new_left, new_top));
    box+= Vertex(new_right, new_bottom);
    return box;
}

bool AABoundingBox::overlapsWith(const AABoundingBox &other) const
{  
//    assert ( isNormalized(*this) && isNormalized(other));
    return (max( tl.x, other.tl.x) <= min( br.x, other.br.x)) && 
           (max( tl.y, other.tl.y) <= min( br.y, other.br.y));
}



BigInt AABoundingBox::width()  const { return br.x - tl.x;}
BigInt AABoundingBox::height() const { return br.y - tl.y;}

//=======================================================================


