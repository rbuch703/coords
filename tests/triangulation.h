
#ifndef SIMPLIFYPOLYGON_H
#define SIMPLIFYPOLYGON_H

#include "../config.h"
#include "../geometric_types.h"
#include "../vertexchain.h"
#include "../avltree.h"

#include <list>


//typedef list<Vertex> OpenPolygon;


/* Order is important, since events for the same 2D-Point are processed in the
 * order this enumeration is defined. With this order, it is ensured that all
 * intersections for a point are found event if some line segements also start 
 * or end at that point. */
enum EventType { START, END, SPLIT, MERGE, REGULAR };

// ===========================================================


//TODO: change to direct vertex data access (non-int128_t)
EventType classifyVertex(const vector<Vertex> &vertices, uint64_t vertex_id)
{
    /* algorithms to deal with edge cases (two subsequent vertices have the same x-value):
     *  - wlog, define the vertex with the higher y-coordinate as always being REGULAR
     *  - 
     */

//    assert (isClockwise(vertices));
    Vertex pos = vertices[vertex_id];
    Vertex pred= vertices[ (vertex_id + vertices.size() - 1) % vertices.size()];
    Vertex succ= vertices[ (vertex_id                   + 1) % vertices.size()];
    
    if (pos.get_x() == pred.get_x()) // edge case
    {
        assert (pos.get_y() != pred.get_y());
        if (pos.get_y() > pred.get_y()) return REGULAR;
        else
        {
            // replace 'pred' by its predecessor
            pred = vertices[ (vertex_id + vertices.size() - 2) % vertices.size()];
            assert (pos.get_x() != pred.get_x());
            //assert(false && "Not implemented");
        }
    }
    
    if (pos.get_x() == succ.get_x()) // edge case
    {
        assert (pos.get_y() != succ.get_y());
        if (pos.get_y() > succ.get_y()) return REGULAR;
        else
        {
            // replace 'succ' by its successor
            succ = vertices[ (vertex_id + vertices.size() + 2) % vertices.size()];
            assert (pos.get_x() != pred.get_x());
        }
    }
    
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
    } else  // all edge cases where either 'pred' xor 'succ' has the same x-value as 'pos'
        assert(false && "uncategorized vertex");
}

class SimpEvent
{
public:
    SimpEvent();
    //SimpEvent( Vertex pPos, EventType t ): pos(pPos), type(t) {}

    SimpEvent( const vector<Vertex> &vertices, uint64_t vertex_id): 
        pos(  vertices[  vertex_id]), 
        pred( vertices[ (vertex_id + vertices.size() -1) % vertices.size() ]), 
        succ( vertices[ (vertex_id                  + 1) % vertices.size() ])
    {
        assert( (vertices.front() != vertices.back()) && "For this algorithm, the duplicate back vertex must have been removed");
        type = classifyVertex(vertices, vertex_id);
        cout << "creating event " << pred << "/" << pos << "/" << succ << endl;
    }
    
    bool operator==(const SimpEvent & other) const {return pos == other.pos; }
    bool operator!=(const SimpEvent & other) const {return pos != other.pos; }
    bool operator <(const SimpEvent &other) const  {return pos <  other.pos; }
public:
    Vertex pos, pred, succ;
    EventType type;
};


//:TODO replace by a heap, don't need the complexity of an AVLTree
class MonotonizeEventQueue : protected AVLTree<SimpEvent> 
{
public:

    int size() const { return AVLTree<SimpEvent>::size(); }

    void add( const vector<Vertex> &vertices, uint64_t vertex_id )
    {
        insert( SimpEvent( vertices, vertex_id) );
    }

    bool containsEvents() const { return size();}
    
	SimpEvent pop()
	{
	    assert ( size() > 0 );
	    SimpEvent item = *begin();
	    this->remove (item);
	    return item;
    }
    
	SimpEvent top() 
	{
	    assert ( size() >0 );
	    return *begin();
    }

    void remove(SimpEvent ev) { AVLTree<SimpEvent>::remove(ev); }
    
  
/*    void remove(Vertex v)
    {
        this->remove( SimpEvent(v, REGULAR));   //uses arbitrary event type 'REGULAR', which remove() ignores anyway
    }    */
};

ostream &operator<<( ostream &os, SimpEvent ev)
{
    switch (ev.type)
    {
        case START:  os << "START";  break;
        case END:    os << "END";    break;
        case SPLIT:  os << "SPLIT";  break;
        case MERGE:  os << "MERGE";  break;
        case REGULAR:os << "REGULAR";break;
    }
    os << " " << ev.pred <<" / *" << ev.pos << "* / " << ev.succ;
    return os;
}

// ===========================================================

/** returns whether the line 'a' has a smaller or equal y-value at xPos than the line 'b'.
    The method silently assumes that both segments actually intersect the vertical line at xPos.
    Do not make this a method os the LineSegment class, as it has nothing to do with it semantically
  */
bool leq(const LineSegment a, const LineSegment b, BigInt xPos)
{
    assert(a.start.get_x() != a.end.get_x());
    assert(b.start.get_x() != b.end.get_x());
    
    BigInt dbx = b.end.get_x() - b.start.get_x();
    BigInt dax = a.end.get_x() - a.start.get_x();
    
    BigInt dby = b.end.get_y() - b.start.get_y();
    BigInt day = a.end.get_y() - a.start.get_y();
    
    bool flipSign = (dax < 0) != (dbx < 0);
    
    BigInt left = (a.start.get_y() - b.start.get_y()) * dbx * dax;
    BigInt right=  dby * dax * (xPos - b.start.get_x()) - day * dbx * (xPos - a.start.get_x());
    
    return (left < right) != flipSign;
}

bool eq(const LineSegment a, const LineSegment b, BigInt xPos)
{
    assert(a.start.get_x() != a.end.get_x());
    assert(b.start.get_x() != b.end.get_x());
    
    BigInt dbx = b.end.get_x() - b.start.get_x();
    BigInt dax = a.end.get_x() - a.start.get_x();
    
    BigInt dby = b.end.get_y() - b.start.get_y();
    BigInt day = a.end.get_y() - a.start.get_y();
    
    BigInt left = (a.start.get_y() - b.start.get_y()) * dbx * dax;
    BigInt right=  dby * dax * (xPos - b.start.get_x()) - day * dbx * (xPos - a.start.get_x());
    
    return left == right;
}

//=====================================

// returns whether the line segment 'a' at x-value xPos has a y-value less than or equal to yPos
bool leq(const LineSegment a, BigInt xPos, BigInt yPos)
{
    BigInt dax = a.end.get_x() - a.start.get_x();
    assert ((( a.start.get_x() >= xPos && a.end.get_x()  <= xPos) ||
             ( a.end.get_x()   >= xPos && a.start.get_x()<= xPos)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    bool flipSign = (dax < 0);
    
    BigInt left = (xPos - a.start.get_x()) * ( a.end.get_y() - a.start.get_y() );
    BigInt right= (yPos - a.start.get_y()) * dax;
    
    return (left < right) != flipSign;
}

bool eq(const LineSegment a, BigInt xPos, BigInt yPos)
{
    BigInt dax = a.end.get_x() - a.start.get_x();
    assert ((( a.start.get_x() >= xPos && a.end.get_x()  <= xPos) ||
             ( a.end.get_x()   >= xPos && a.start.get_x()<= xPos)) &&
             "position not inside x range of segment");
             
    assert (dax != 0 && "edge case vertical line segment");
    
    BigInt left = (xPos - a.start.get_x()) * ( a.end.get_y() - a.start.get_y() );
    BigInt right= (yPos - a.start.get_y()) * dax;
    
    return (left == right);
}

typedef AVLTreeNode<LineSegment> *EdgeContainer;

 // protected inheritance to hide detail of AVLTree, since for a LineArrangement, AVLTree::insert must never be used
class LineArrangement: protected AVLTree<LineSegment>
{
public:
    EdgeContainer addEdge(const LineSegment &a, const BigFraction xPosition);
#ifndef NDEBUG
    bool isConsistent(EdgeContainer node, BigInt xPos) const
    {
        bool left = !node->m_pLeft || 
                    (leq( node->m_pLeft->m_Data, node->m_Data, xPos)
                    && isConsistent(node->m_pLeft, xPos));
                    
        bool right= !node->m_pRight ||
                    (leq( node->m_Data, node->m_pRight->m_Data, xPos) && 
                    isConsistent(node->m_pRight, xPos));
        return left && right;
    }
    
    bool isConsistent(BigInt xPos) const { return !m_pRoot || isConsistent(m_pRoot, xPos); }
#endif

    int size() const { return AVLTree<LineSegment>::size(); }
    
    
    bool hasPredecessor( EdgeContainer node)
    {
        iterator it(node, *this);
        return it != begin();
    }

    LineSegment getPredecessor( EdgeContainer node)
    {
        iterator it(node, *this);
        assert (it != begin());
        return *(--it);
    }
    
    bool hasSuccessor( EdgeContainer node )
    {
        iterator it(node, *this);
        return ++it != end();
    }
    
    LineSegment getSuccessor( EdgeContainer node)
    {
        iterator it(node, *this);
        assert (++it != end());
        return *it;
    }
  
    LineSegment getEdgeLeftOf( Vertex pos )
    {
        EdgeContainer e = findAdjacentEdge( pos );
        LineSegment edge = e->m_Data;
        assert (! eq( edge, pos.get_x(), pos.get_y() ));
        bool edgeLeftOfPos = leq(edge, pos.get_x(), pos.get_y());
        
        if (!edgeLeftOfPos)
        {
            assert( hasPredecessor(e) );
            edge = getPredecessor( e );
            assert( leq(edge, pos.get_x(), pos.get_y()));
        }
        
        return edge;
    }
    
    void remove(LineSegment item, const BigInt xPos)
    {
        cout << "removing edge " << item << endl;
    
        assert( item.start.get_x() <= item.end.get_x());
#ifndef NDEBUG
        #warning expensive debug checks
        for (const_iterator it = this->begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif

        AVLTreeNode<LineSegment> *node = findPos(item, xPos);
        assert( node && "Node to be removed does not exist");
        assert( node->m_Data == item);
        AVLTree<LineSegment>::remove(node);
    }
    
    EdgeContainer findPos(LineSegment item, const BigInt xPos)
    {
    	if (! m_pRoot) return NULL;

	    //BigFraction yVal = item.getYValueAt(xPos);

	    AVLTreeNode<LineSegment>* pPos = m_pRoot;
	    while (pPos)
	    {
	        //BigFraction yVal2 = pPos->m_Data.getYValueAt(xPos);
	        if ( eq(item, pPos->m_Data,xPos) ) return pPos;
	        pPos = leq(item, pPos->m_Data, xPos) ? pPos->m_pLeft : pPos->m_pRight;
	    }
	    return pPos;
    }
    
    
    /* returns an adjacent edge to (xPos, yPos), i.e. an edge that is closest to (xPos, yPos)
       either to the left or to the right.
       If any edge passes directly through (xPos, yPos), the method is guaranteed to return said edge.
       Otherwise, the method makes no guarantee on which of the two adjacent edges it returns. In
       particular, it need not return the closest of the two edges.
   */
    EdgeContainer findAdjacentEdge(const BigInt xPos, const BigInt yPos)
    {
    	if (! m_pRoot) return NULL;
    	
        EdgeContainer parent=m_pRoot;
	    EdgeContainer pPos = m_pRoot;
	    while (pPos)
	    {
	        parent = pPos;
	        if ( eq(pPos->m_Data,xPos, yPos) ) return pPos;
	        pPos = leq(pPos->m_Data, xPos, yPos) ? pPos->m_pRight : pPos->m_pLeft;
	    }
	    return parent;
    }

    EdgeContainer findAdjacentEdge(Vertex v)
    {
        return findAdjacentEdge( v.get_x(), v.get_y() );
    }
    
    EdgeContainer insert(const LineSegment &item, BigInt xPos)
    {
        cout << "inserting edge " << item << endl;
        assert( item.start.get_x() <= item.end.get_x());
    
#ifndef NDEBUG
        #warning expensive debug checks
        for (LineArrangement::const_iterator it = begin(); it != end(); it++)
            assert( it->start.get_x() <= xPos && it->end.get_x() >= xPos && "line arrangement contains outdated edge");
#endif
    
        EdgeContainer parent = findParent(item, xPos);
        
	    AVLTreeNode<LineSegment>* pPos = new AVLTreeNode<LineSegment>(0,0, parent, item);
	
	    if (!parent)
	    {
	        m_pRoot = pPos;
	        pPos->m_dwDepth = 0;    //no children --> depth=0
	        return pPos;   //no parent --> this is the root, nothing more to do
	    }
	
	    assert (!eq(item, parent->m_Data, xPos));
	    
	    if ( leq(item, parent->m_Data, xPos) )
	    {
	        assert( ! parent->m_pLeft);
	        parent->m_pLeft = pPos;
	    } else
	    {
	        assert( ! parent->m_pRight);
	        parent->m_pRight = pPos;
	    }
	
	    updateDepth( pPos);//, NULL, NULL);
	    return pPos;
    }
    
protected:

    /* returns the tree node whose one direct child would be 'item' (if 'item' was in the tree).
     * This method assumes that 'item' is not actually present in the tree */
    EdgeContainer findParent(const LineSegment &item, BigInt xPos) const
    {
	    EdgeContainer parent = NULL;
	    if (! m_pRoot) return NULL;

	    EdgeContainer pPos = m_pRoot;
	    while ( ! eq( pPos->m_Data, item, xPos) )
	    {
		    parent = pPos;
		    pPos = leq(item, pPos->m_Data, xPos) ? pPos->m_pLeft : pPos->m_pRight;
		    if (! pPos) return parent;
	    }
	    assert(false && "duplicate entry found");
	    
	    return pPos->m_pParent;
    }
    /*
    list<EdgeContainer> findAllIntersectingEdges(LineSegment item, const BigFraction xPos)
    {
        // findPos will return any LineSegment that intersects 'item' at xPos, not necessarily xPos itself
	    AVLTreeNode<LineSegment> *pPos = findPos(item, xPos);    
	    if (!pPos) return list<AVLTreeNode<LineSegment>*>();
	    
	    list<AVLTreeNode<LineSegment>*> res;
	    //res.push_back(pPos);
	    iterator it(pPos, *this);
	    
	    
	    while ( it!= begin())
	    {
	        --it;
	        if ( !it->isEqual(item, xPos)) break;
	    }
	    
	    if (!it->isEqual(item, xPos)) it++;

        
        for (; it != end() && it->isEqual(item, xPos); it++)
            res.push_back( it.getNode() );
	    
	    return res;
    }
    */
    
    
	AVLTree::iterator getIterator(LineSegment e, const BigInt xPos)
	{
	    AVLTreeNode<LineSegment>* p = findPos(e, xPos);
	    assert(p);
	    assert(p->m_Data == e);
	    return AVLTree::iterator(p, *this);
	}

    /*void swapEdges(LineSegment &e1, LineSegment &e2, const int64_t xPosition)*/
};

#endif

