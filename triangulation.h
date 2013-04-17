
#ifndef TRIANGULATION_H
#define TRIANGULATION_H

#include "vertexchain.h"
#include "avltree.h"
#include <stdint.h>
#include <vector>

vector<int32_t> triangulate( const VertexChain &c);

// =====================================================
// everything below this line is only exposed for debug purposes

list<VertexChain> toMonotonePolygons( const VertexChain &chain);
void triangulateMonotonePolygon( const VertexChain &monotone_polygon, vector<int32_t> &res);
list<LineSegment> createEndMonotoneDiagonals( const VertexChain &chain);

enum EventType { START, END, SPLIT, MERGE, REGULAR };

class SimpEvent
{
public:
    SimpEvent( const vector<Vertex> &vertices, uint64_t vertex_id, EventType p_type);
    
    bool operator==(const SimpEvent & other) const;
    bool operator!=(const SimpEvent & other) const;
    bool operator <(const SimpEvent & other) const;
public:
    Vertex pos, pred, succ;
    EventType type;
};

//:TODO replace by a heap, don't need the complexity of an AVLTree
class MonotonizeEventQueue : protected AVLTree<SimpEvent> 
{
public:

    int size() const { return AVLTree<SimpEvent>::size(); }

    MonotonizeEventQueue( const vector<Vertex> &polygon)
    {
        assert(polygon.front() != polygon.back() && "Not suported for vertex classification");
        for (uint64_t i = 0; i < polygon.size(); i++)
        {
            Vertex pos(  polygon[ i]);
            Vertex pred( polygon[ (i + polygon.size() -1) % polygon.size() ]);
            Vertex succ( polygon[ (i                 + 1) % polygon.size() ]);
            assert( (pred.get_x() != pos.get_x()) || (succ.get_x() != pos.get_x() ));
            
            if (pred.get_x() == pos.get_x()) continue;  //already handled as a pair
            if (succ.get_x() == pos.get_x())            //handled as pair edge case
            {
                pair<EventType, EventType> events = classifyPair( polygon, i);
                insert( SimpEvent( polygon, i,   events.first ));
                insert( SimpEvent( polygon, i+1, events.second));
            } else
                insert( SimpEvent(polygon, i, classifyVertex(polygon, i)));
        }

        
    }

    bool containsEvents() const { return size();}
    
	SimpEvent pop()
	{
	    assert ( size() > 0 );
	    SimpEvent item = *begin();
	    this->remove (item);
	    return item;
    }
    
/*	SimpEvent top() 
	{
	    assert ( size() >0 );
	    return *begin();
    }*/

    void remove(SimpEvent ev) { AVLTree<SimpEvent>::remove(ev); }
    
private:

    // ===========================================================

    /*  method to classify polygon vertices for the edge case that two subsequent
        vertices have a common x-value.
      */
    //TODO: change to direct vertex data access (non-int128_t)
      
    pair<EventType,EventType> classifyPair(const vector<Vertex> &vertices, uint64_t vertex_id)
    {
        assert(vertices.front() != vertices.back());
        Vertex pred = vertices[ (vertex_id + vertices.size() - 1) % vertices.size()];
        Vertex pos1 = vertices[  vertex_id];
        Vertex pos2 = vertices[ (vertex_id                   + 1) % vertices.size()];
        Vertex succ = vertices[ (vertex_id                   + 2) % vertices.size()];
        
        assert(pos1.get_x() == pos2.get_x());
        assert(pos1.get_y() != pos2.get_y());
        assert(pred.get_x() != pos1.get_x());
        assert(pos2.get_x() != succ.get_x());
        
        /* if two subsequent vertices:
         * - form a REGULAR vertex: --> make both regular
         * - form a START vertex: the one that will occur earlier in the event queue has to be the START vertex (to start the "compartment"); other: REGULAR
         * - form a SPLIT vertex: the vertex with the earlier (= smaller y-value) event must be the SPLIT vertex to open the new compartment; other: REGULAR
         * - form a END vertex: the one that will occur later in the event queue has to be the END vertex (so that the compartment's final vertex closes it); other: REGULAR
         * - form a MERGE vertex: the vertex with the later event must be the MERGE vertex so that the last vertex closes the compartment
         */
        
        if ( ((pred.get_x() < pos1.get_x()) && (succ.get_x() > pos1.get_x())) ||
             ((pred.get_x() > pos1.get_x()) && (succ.get_x() < pos1.get_x()))) 
            return pair<EventType, EventType>( REGULAR, REGULAR);
        else if (pred.get_x() < pos1.get_x() && succ.get_x() < pos1.get_x()) // MERGE or END
        {
            if (pos1.get_y() < pos2.get_y())
                return pair<EventType, EventType>( REGULAR, MERGE);
            else 
                return pair<EventType, EventType>( END, REGULAR);   //pos1 is the later event (= higher y-value) vertex
        } else if ((pred.get_x() > pos1.get_x()) && (succ.get_x() > pos1.get_x())) // START or SPLIT
        {
            if (pos1.get_y() < pos2.get_y())
                return pair<EventType, EventType>( START, REGULAR);
            else
                return pair<EventType, EventType>( REGULAR, SPLIT); //pos2 is the earlier event (= lower y-value) vertex
        } else assert(false);
        
        return pair<EventType, EventType>( REGULAR, REGULAR); //dummy
    }

    //TODO: change to direct vertex data access (non-int128_t)
    EventType classifyVertex(const vector<Vertex> &vertices, uint64_t vertex_id)
    {
        /* algorithms to deal with edge cases (two subsequent vertices have the same x-value):
         *  - wlog, define the vertex with the higher y-coordinate as always being REGULAR
         *  - 
         */

        Vertex pos = vertices[vertex_id];
        Vertex pred= vertices[ (vertex_id + vertices.size() - 1) % vertices.size()];
        Vertex succ= vertices[ (vertex_id                   + 1) % vertices.size()];
        
        // subsequent vertices with identical x-value are an edge case (for this x-sweep) and must be handeld separately
        assert( pos.get_x() != pred.get_x() && pos.get_x() != succ.get_x());

        assert( pos != pred && pos != succ && pred != succ);
        /* Colinear successive vertices would cause additional special cases --> forbid them.
         * They should have been removed by canonicalize() anyway */
        assert( ! LineSegment(pred, succ).isColinearWith(pos)); 
        
        if (pos.get_x() < pred.get_x() && pos.get_x() < succ.get_x())    //is either a START or a SPLIT vertex
        {
            int128_t dist = succ.pseudoDistanceToLine(pred, pos);
            assert (dist != 0 && "colinear vertices");
            if (dist < 0) //left turn
                return EventType::SPLIT;
            else    //right turn
                return EventType::START;
        
        } else if (pos.get_x() > pred.get_x() && pos.get_x() > succ.get_x()) //is either an END or a MERGE vertex
        {
            int128_t dist = succ.pseudoDistanceToLine(pred, pos);
            assert (dist != 0 && "colinear vertices");
            if (dist < 0) //left turn
                return EventType::MERGE;
            else
                return EventType::END;
            
        } else if ((pos.get_x() > pred.get_x() && pos.get_x() < succ.get_x()) || //one above, one below
                   (pos.get_x() < pred.get_x() && pos.get_x() > succ.get_x()) )
        {
            return EventType::REGULAR;
        } else
            assert(false && "uncategorized vertex");
        return REGULAR;
    }
/*    void remove(Vertex v)
    {
        this->remove( SimpEvent(v, REGULAR));   //uses arbitrary event type 'REGULAR', which remove() ignores anyway
    }    */
};

ostream &operator<<( ostream &os, SimpEvent ev);
#endif

