
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

enum SimpEventType { SEG_START, SEG_END, INTERSECTION };

class SimpEvent
{
public:
    SimpEvent() { type = (SimpEventType)-1;}
    SimpEvent( SimpEventType pType, ActiveEdge* pThisEdge, ActiveEdge* pOtherEdge = NULL): 
        type(pType), m_thisEdge(pThisEdge), m_otherEdge(pOtherEdge)
        {
            
            switch (type)
            {
                case SEG_START:
                    assert(m_thisEdge && !m_otherEdge);
                    xNum = m_thisEdge->left.x;
                    yNum = m_thisEdge->left.y;
                    xDenom = yDenom = 1;
                    break;
                case SEG_END:
                    assert(m_thisEdge && !m_otherEdge);
                    xNum = m_thisEdge->right.x;
                    yNum = m_thisEdge->right.y;
                    xDenom = yDenom = 1;
                    break;
                case INTERSECTION:
                
                    assert(m_thisEdge && m_otherEdge);
                    {
                        LineSegment l1(m_thisEdge->left, m_thisEdge->right);
                        LineSegment l2(m_otherEdge->left,m_otherEdge->right);
                        assert( l1.intersects(l2) );
                        BigInt num, denom;
                        l1.getIntersectionCoefficient(l2, num, denom);
                        // x = (start.x + num * (end.x - start.x) / denom) = (start.x*denom + num*(end.x-start.x))/denom
                        xNum = (l1.start.x*denom + num*(l1.end.x-l1.start.x));
                        xDenom = denom;
                        // y = (start.y + num * (end.y - start.y) / denom) = (start.y*denom + num*(end.y-start.y))/denom
                        yNum = (l1.start.y*denom + num*(l1.end.y - l1.start.y))/denom;
                        yDenom = denom;
                        
                        /* canonical form: denominator is non-negative; this ensures that 
                           the "<"-predicate computation does not require sign flips
                         */
                        if (denom < 0)  
                        {
                            xNum = -xNum;
                            xDenom = -xDenom;
                            yNum = -yNum;
                            yDenom = -yDenom;
                        }
                    }
                    break;
                default: 
                    xNum = yNum = xDenom = yDenom = 0;
                    assert(false && "Invalid event type"); 
                    break;
            }
        }

    bool operator==(const SimpEvent & other) const 
    {
         assert ( xDenom != 0 && yDenom != 0 && other.xDenom != 0 && other.yDenom != 0);
        /* x = other.x && y = other.y 
         *
         * xNum/xDenom = other.xNum/other.xDenom && yNum/yDenom = other.yNum/other.yDenom
         *           <=>
         */
        return xNum*other.xDenom == other.xNum*xDenom && 
               yNum*other.yDenom == other.yNum*yDenom;
    }
               
               
    bool operator!=(const SimpEvent & other) const
    { 
        return ! (*this == other);
    }
    
    bool operator <(const SimpEvent &other) const
    {
        /* canonical denominators should be positive; otherwise, we would have to consider 
           sign flips of the result (since we use rearraged inequality derived by multiplying
           the original formula by a potentially negative number)
           */
        assert ( xDenom > 0 && yDenom > 0 && other.xDenom > 0 && other.yDenom > 0);
        if (xNum*other.xDenom != other.xNum*xDenom) // differ in x coordinate;
            return xNum*other.xDenom < other.xNum*xDenom;     
        
        return yNum*other.yDenom < other.yNum*yDenom;
    }
    
    SimpEventType type;
    ActiveEdge *m_thisEdge, *m_otherEdge;
    // x and y coordinates for the position at which the event takes place
    // separate numerators and denominators are necessary, because an intersection
    // may happen at a non-integer position
    BigInt      xNum, xDenom, yNum, yDenom; 
    
};

std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge)
{
    os << edge.left << " - " << edge.right;
    return os;
}




list<OpenPolygon*> openPolygons;
/*
GeometryTree activeEdges;

void handleMinimalVertex( const ConnectedVertex &v)
{
    
}

void simplifyPolygon(const PolygonSegment &seg, list<PolygonSegment> &res)
{
    PolygonSegment s = seg;
    s.canonicalize();

    uint32_t nVertices = s.vertices().size() - 1; //first == last
    list<Vertex>::const_iterator it = s.vertices().begin();
    ConnectedVertex vertices[nVertices];
 
    for (uint32_t i = 0; i < nVertices; i++, it++)
    {
        vertices[i].x = it->x;
        vertices[i].y = it->y;
        vertices[i].pred = &vertices[ (i+1) % nVertices];
        vertices[i].succ = &vertices[ (i-1+nVertices) % nVertices];
    }

    AVLTree<SimpEvent> events;
    
    for (uint32_t i = 0; i < nVertices; i++)
    {
        std::cout << "#" << vertices[i] << endl;
        if ( vertices[i] < (*vertices[i].pred) && vertices[i] < (*vertices[i].succ))
            events.insert( SimpEvent(MIN_VERTEX, &vertices[i]));
    }

    while (events.size())
    {
        SimpEvent e = events.pop();
        
        switch (e.type)
        {
            case MIN_VERTEX:
                break;
            case END_OF_SEGMENT:
                break;
            case INTERSECTION:
                break;
            default:
                assert(false && "Invalid event type");
                break;
        }
    }
    std::cout << events.size() << " events" << endl;
    
    for (AVLTree<SimpEvent>::const_iterator it = events.begin(); it != events.end(); it++)
        std::cout << it->principalVertex << endl;


}
*/

