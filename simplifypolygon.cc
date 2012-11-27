
#include "simplifypolygon.h"

#include <boost/foreach.hpp>

class ConnectedVertex: public Vertex
{

public:

//    ConnectedVertex *(

    ConnectedVertex *pred, *succ;    
};

enum SimpEventType { MIN_VERTEX, END_OF_SEGMENT, INTERSECTION  };

class SimpEvent
{
public:
    SimpEvent() { type = (SimpEventType)-1;}
    SimpEvent( SimpEventType pType, ConnectedVertex* pStartVertex, ConnectedVertex* pEndVertex = NULL,
               ConnectedVertex* pOtherStartVertex = NULL, ConnectedVertex *pOtherEndVertex = NULL):
                type(pType), startVertex(pStartVertex), endVertex(pEndVertex), 
                otherStartVertex(pOtherStartVertex), otherEndVertex(pOtherEndVertex)
               {
                    assert(startVertex);
                    switch (type)
                    {
                        case MIN_VERTEX:
                            assert(!endVertex && !otherStartVertex && !otherEndVertex);
                            principalVertex = Vertex( startVertex->x, startVertex->y);
                            break;
                        case END_OF_SEGMENT:
                            assert(endVertex && !otherStartVertex && !otherEndVertex);
                            principalVertex = Vertex( endVertex->x, endVertex->y);
                            break;
                        case INTERSECTION:
                            assert(endVertex && otherStartVertex && otherEndVertex);
                            assert( false && "line intersection not yet implemented");
                            break;
                        default: 
                            assert(false && "Invalid event type"); 
                            break;
                    }
               }

    bool operator==(const SimpEvent & other) const 
    {
        return (principalVertex == other.principalVertex) && 
               (type != other.type) &&
               (startVertex == other.startVertex) &&
               (endVertex == other.endVertex) &&
               (otherStartVertex == other.otherStartVertex) &&
               (otherEndVertex == other.otherEndVertex);

    }
               
               
    bool operator!=(const SimpEvent & other) const
    { 
        return ! (*this == other);
    }
    
    bool operator <(const SimpEvent &other) const
    {
        if (principalVertex != other.principalVertex) return principalVertex < other.principalVertex;
        if (type != other.type) return type < other.type;
        if (startVertex != other.startVertex) return startVertex < other.startVertex;
        if (endVertex != other.endVertex) return endVertex < other.endVertex;
        if (otherStartVertex != other.otherStartVertex) return otherStartVertex < other.otherStartVertex;
        return otherEndVertex < other.otherEndVertex;
    }
    
    SimpEventType type;
    ConnectedVertex *startVertex;
    ConnectedVertex *endVertex;
    ConnectedVertex *otherStartVertex;
    ConnectedVertex *otherEndVertex;
    
    /** the vertex by which the event is to be sorted into the event queue
      * for MIN_VERTEX, this is the startVertex
      * for END_OF_SEGMENT, this is the endVertex
      * for INTERSECTION, this is the position of the intersection*/
    Vertex           principalVertex; 
    
};

std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge)
{
    os << edge.left << " - " << edge.right << "; " << (edge.isTopEdge? "true":"false") << ", " << edge.poly;
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

