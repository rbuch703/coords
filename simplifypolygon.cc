
#include "simplifypolygon.h"

#include <boost/foreach.hpp>

/**
    Data Structures:
        - priority queue of events (new segment, end of segment, intersection), 
            - sorted by sweep direction (first by x-coordinate, then by y-coordinate)
            - ability to remove elements in O( log(n) ) (mostly intersections that did not materialize)
            
        - list of active edges (those polygon edges that intersect the current sweep line), 
            - sorted by current y-coordinate
            - ability to swap adjacent edges in O(1) (when they intersect)
            - ability to find edges in O(log(n)) (e.g. the active edge matching the current event)
            - ability to remove edges in O(log(n)) (when they no longer intersec the sweep line)
*/


ActiveEdge::ActiveEdge(const Vertex pLeft, const Vertex pRight): 
    left(pLeft), right(pRight)
{ 
    assert (left < right || (left == Vertex(0,0) && (right ==Vertex(0,0) ) ) ); 

}

BigFraction ActiveEdge::getYValueAt(const BigFraction &xPosition) const
{
    
    assert(left.x <= right.x); 

    //otherwise we'll get a divide by zero later on
    if (left.x == right.x) assert(false && "Not implemented");
    
    BigFraction dy= BigFraction(BigInt(right.y) - left.y, BigInt(right.x) - left.x)
                    * (xPosition - left.x);
    
    return dy + left.y;
}

BigFraction pseudoDistanceToLine( BigFraction start_x, BigFraction start_y, BigFraction end_x, BigFraction end_y, BigFraction x, BigFraction y)
{
    return (end_x - start_x)*(start_y - y) - (end_y - start_y)*(start_x - x);
}

bool ActiveEdge::isLessThanFuture(const ActiveEdge & other, const BigFraction &xPosition) const
{
    BigFraction y1 = getYValueAt(xPosition);
    BigFraction y2 = other.getYValueAt(xPosition);
    if (y1 != y2) return y1 < y2;
    
    if ( toLineSegment().parallelTo( other.toLineSegment() ) ) return false;
    
    assert(y1 == y2);
    /* 'other' ends at the intersection point and thus has no future --> lines are not parallel, but
     * pseudodistance would still be zero */
    if ((xPosition == other.right.x ) && (y1 == other.right.y))  
    {
        assert (false && "Not implemented");
    }
    
    BigFraction dist = pseudoDistanceToLine(xPosition, y1, other.right.x, other.right.y, right.x, right.y);
    assert(dist != BigFraction(0) );
    bool isLeftTurn =  (dist < BigFraction(0) );
    return isLeftTurn;
}

bool ActiveEdge::isLessThan(const ActiveEdge & other, const BigFraction &xPosition) const
{ return getYValueAt(xPosition) < other.getYValueAt(xPosition); }

bool ActiveEdge::isEqual(const ActiveEdge & other, const BigFraction &xPosition) const
{ return getYValueAt(xPosition) == other.getYValueAt(xPosition); }
    
bool ActiveEdge::isLessOrEqual(const ActiveEdge & other, const BigFraction &xPosition) const
{ return getYValueAt(xPosition) <= other.getYValueAt(xPosition); }
    
    //HACK: the operator needs to be defined for the AVLTree to work at all; but it must never be used,
    //because the comparision of two ActiveEdges has no meaning unless the y position at which
    //the comparison is to be evaluated is provided as well.
bool ActiveEdge::operator<(const ActiveEdge &) { 
    assert(false && "ActiveEdges cannot be compared");
}

bool ActiveEdge::intersects(ActiveEdge other) const
{
    return LineSegment(left, right).intersects(LineSegment(other.left, other.right));
}

void ActiveEdge::getIntersectionWith(ActiveEdge &other, BigFraction &out_x, BigFraction &out_y) const
{
    LineSegment A(left, right);
    LineSegment B(other.left, other.right);

    assert (A.intersects(B));
    BigInt num, denom;
    
    A.getIntersectionCoefficient(B, num, denom);
    BigFraction coeff(num, denom);
    
    out_x = coeff*(A.end.x - A.start.x) + A.start.x;
    out_y = coeff*(A.end.y - A.start.y) + A.start.y;
}


std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge)
{
    os << edge.left << " - " << edge.right;
    return os;
}


SimpEvent::SimpEvent() { type = (SimpEventType)-1;}
SimpEvent::SimpEvent( SimpEventType pType, ActiveEdge pThisEdge, ActiveEdge pOtherEdge):
    type(pType), m_thisEdge(pThisEdge), m_otherEdge(pOtherEdge)
{
    switch (type)
    {
        case SEG_START:
            assert(m_thisEdge && !m_otherEdge);
            x = BigFraction(m_thisEdge.left.x, 1);
            y = BigFraction(m_thisEdge.left.y, 1);
            break;
        case SEG_END:
            assert(m_thisEdge && !m_otherEdge);
            x = BigFraction(m_thisEdge.right.x, 1);
            y = BigFraction(m_thisEdge.right.y, 1);
            break;
        case INTERSECTION:
        
            assert(m_thisEdge && m_otherEdge);
            {
                LineSegment l1(m_thisEdge.left, m_thisEdge.right);
                LineSegment l2(m_otherEdge.left,m_otherEdge.right);
                assert( l1.intersects(l2) );
                BigInt num, denom;
                l1.getIntersectionCoefficient(l2, num, denom);
                // x = (start.x + num * (end.x - start.x) / denom) = (start.x*denom + num*(end.x-start.x))/denom
                x = BigFraction( l1.start.x*denom + num*(l1.end.x-l1.start.x), denom);
                // y = (start.y + num * (end.y - start.y) / denom) = (start.y*denom + num*(end.y-start.y))/denom
                y = BigFraction( l1.start.y*denom + num*(l1.end.y - l1.start.y), denom);
            }
            break;
        default: 
            x = y = BigFraction(0);
            assert(false && "Invalid event type"); 
            break;
    }
}

bool SimpEvent::operator==(const SimpEvent &other) const 
{
    return  (x == other.x) && 
            (y == other.y) && 
            (type == other.type) &&
            (m_thisEdge.left == other.m_thisEdge.left) && 
            (m_thisEdge.right == other.m_thisEdge.right) &&
            (m_otherEdge.left == other.m_otherEdge.left) &&
            (m_otherEdge.right == other.m_otherEdge.right);
}
           
           
bool SimpEvent::operator!=(const SimpEvent &other) const
{ 
    return ! (*this == other);
}

bool SimpEvent::operator <(const SimpEvent &other) const
{
    if ( x != other.x ) return x < other.x;
    if ( y != other.y ) return y < other.y;
    if (type != other.type) return type < other.type;
    if (m_thisEdge.left != other.m_thisEdge.left ) return m_thisEdge.left <  other.m_thisEdge.left;
    if (m_thisEdge.right!= other.m_thisEdge.right) return m_thisEdge.right < other.m_thisEdge.right;
    if (m_otherEdge.left != other.m_otherEdge.left ) return m_otherEdge.left <  other.m_otherEdge.left;
    return m_otherEdge.right < other.m_otherEdge.right;
}

AVLTreeNode<ActiveEdge>* LineArrangement::addEdge(const ActiveEdge &a, const BigFraction xPosition)
{
    num_items++;
    if (!m_pRoot)
    {
        m_pRoot = new AVLTreeNode<ActiveEdge>(NULL,NULL, NULL, a);;
        m_pRoot->m_dwDepth = 0;    //no children --> depth=0
        return m_pRoot;
    }
    
    AVLTreeNode<ActiveEdge> *parent;//= m_pRoot;
    AVLTreeNode<ActiveEdge> **pos = &m_pRoot;
    do
    {
        parent = *pos;
        pos = a.isLessThanFuture(parent->m_Data, xPosition) ? &parent->m_pLeft : &parent->m_pRight;
    } while (*pos);
    
    assert (! *pos);
    *pos = new AVLTreeNode<ActiveEdge>(NULL, NULL, parent, a);
    (*pos)->m_dwDepth = 0;
    
    updateDepth(*pos);
    return *pos;
}

