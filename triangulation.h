
#ifndef TRIANGULATION_H
#define TRIANGULATION_H

#include "vertexchain.h"
#include "avltree.h"
#include <stdint.h>
#include <vector>

vector<int32_t> triangulate( VertexChain &c);
void triangulate( VertexChain &c, vector<int32_t> &triangles_out);

// =====================================================
// everything below this line is only exposed for debug purposes

list<VertexChain> toMonotonePolygons( VertexChain &chain);
void triangulateMonotonePolygon( const VertexChain &monotone_polygon, vector<int32_t> &res);
list<LineSegment> createEndMonotoneDiagonals( VertexChain &chain);

enum EventType { START, END, SPLIT, MERGE, REGULAR };

class SimpEvent
{
public:
    //SimpEvent( const vector<Vertex> &vertices, uint64_t vertex_id, EventType p_type);
    SimpEvent( Vertex p_pred, Vertex p_pos, Vertex p_aux, Vertex p_succ, bool p_isShared, EventType p_type);
    
    bool operator==(const SimpEvent & other) const;
    bool operator!=(const SimpEvent & other) const;
    bool operator <(const SimpEvent & other) const;
public:
    /* for single-vertex events: the event's position, and the associated predecessor and successor; aux is unused
     * for vertex pair events: the event's position, its neighbor with identical x-value (aux), 
     * and the predecessor and successor of that vertex pair.*/
    Vertex pred, pos, aux, succ;
    // whether the event consists of two adjacent vertices with identical x-values
    bool isShared; 
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
            assert( (pred.x != pos.x) || (succ.x != pos.x ));
            
            if (pred.x == pos.x) continue;  //already handled as a pair
            if (succ.x == pos.x)            //handle as 'pair' edge case
            {
                Vertex succ2 = polygon[ (i + 2) % polygon.size()];

                EventType event = classifyPair( pred, pos, succ, succ2);
                insert( SimpEvent( pred, pos, succ, succ2, true, event ));
                //insert( SimpEvent( polygon, i+1, events.second));
            } else
                insert( SimpEvent(pred, pos, Vertex(0,0), succ, false, classifyVertex(polygon, i)));
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
      
    EventType classifyPair(Vertex pred, Vertex pos1, Vertex pos2, Vertex succ)
    {
        assert(pos1.x == pos2.x);
        assert(pos1.y != pos2.y);
        assert(pred.x != pos1.x);
        assert(pos2.x != succ.x);
        
        if ( ((pred.get_x() < pos1.get_x()) && (succ.get_x() > pos1.get_x())) ||
             ((pred.get_x() > pos1.get_x()) && (succ.get_x() < pos1.get_x()))) 
            return REGULAR;
        else if (pred.get_x() < pos1.get_x() && succ.get_x() < pos1.get_x()) // MERGE or END
        {
            if (pos1.get_y() < pos2.get_y())
                return MERGE;
            else 
                return END;
        } else if ((pred.get_x() > pos1.get_x()) && (succ.get_x() > pos1.get_x())) // START or SPLIT
        {
            if (pos1.get_y() < pos2.get_y())
                return START;
            else
                return SPLIT;
        } else assert(false);
        
        return REGULAR; //dummy
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
        assert( pos.x != pred.x && pos.x != succ.x);

        assert( pos != pred && pos != succ && pred != succ);
        /* Colinear successive vertices would cause additional special cases --> forbid them.
         * They should have been removed by canonicalize() anyway */
        assert( ! LineSegment(pred, succ).isColinearWith(pos)); 
        
        if (pos.x < pred.x && pos.x < succ.x)    //is either a START or a SPLIT vertex
        {
            int dist = succ.atSideOfLine(pred, pos);
            assert (dist != 0 && "colinear vertices");
            if (dist < 0) //left turn
                return EventType::SPLIT;
            else    //right turn
                return EventType::START;
        
        } else if (pos.x > pred.x && pos.x > succ.x) //is either an END or a MERGE vertex
        {
            int dist = succ.atSideOfLine(pred, pos);
            assert (dist != 0 && "colinear vertices");
            return (dist < 0) /*left turn*/ ? EventType::MERGE : EventType::END;
            
        } else if ((pos.x > pred.x && pos.x < succ.x) || //one above, one below
                   (pos.x < pred.x && pos.x > succ.x) )
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

