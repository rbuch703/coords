
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
enum EventType { START, STOP, SPLIT, MERGE, REGULAR };

// ===========================================================

//#error CONTINUEHERE determine algorithm sweep direction, verify classification, especially for cases with identical x/y-values
//#warning handle edge case than two adjacent vertices share the same x/y value and thus none is a start/stop/split/merge vertex
#warning ensure that polygon is oriented clockwise. Otherwise the classification will be incorrect

//#warning TODO: handle special cases where line segments share their x-coordinate (and thus either none or both vertices would be stop/split/stop/merge vertices)
//TODO: if two adjacent vertices share the same x coordinate 
#warning TODO: change to direct vertex data access (non-int128_t)
EventType classifyVertex(const vector<Vertex> &vertices, uint64_t vertex_id)
{
//    assert (isClockwise(vertices));
    Vertex pos = vertices[vertex_id];
    Vertex pred= vertices[ (vertex_id + vertices.size() - 1) % vertices.size()];
    Vertex succ= vertices[ (vertex_id                   + 1) % vertices.size()];
    
    assert( pos != pred && pos != succ && pred != succ);
    /* colinear successive vertices would cause additional special cases --> forbid them
     * they should have been removed by canonicalize() anyway */
    assert( ! LineSegment(pred, succ).isColinearWith(pos)); 
    
    if (pos.get_x() < pred.get_x() && pos.get_x() < succ.get_x())    //is either a START or a SPLIT vertex
    {
        int128_t dist = succ.pseudoDistanceToLine(pred, pos);
        assert (dist != 0 && "colinear vertices");
        if (dist < 0) //left turn
            return EventType::SPLIT;
        else    //right turn
            return EventType::START;
    
    } else if (pos.get_x() > pred.get_x() && pos.get_x() > succ.get_x()) //is either a STOP or a MERGE vertex
    {
        int128_t dist = succ.pseudoDistanceToLine(pred, pos);
        assert (dist != 0 && "colinear vertices");
        if (dist < 0) //left turn
            return EventType::MERGE;
        else
            return EventType::STOP;
        
    } else if ((pos.get_x() > pred.get_x() && pos.get_x() < succ.get_x()) || //one above, one below
               (pos.get_x() < pred.get_x() && pos.get_x() > succ.get_x()) )
    {
        return EventType::REGULAR;
    } else
    {
        int num_equal = 0;
        if (pos.get_x() == pred.get_x()) num_equal++;
        if (pos.get_x() == succ.get_x()) num_equal++;
        assert (num_equal == 1);
        
        if (pos.get_x() == pred.get_x())
        {
            Vertex ppred = vertices[ (vertex_id + vertices.size() - 2) % vertices.size() ];
            assert (ppred.get_x() != pos.get_x && "colinear sequential vertices");        
            if (ppred.get_x() < pos.get_x() && succ.get_x() > pos.get_x()) return EventType::REGULAR;
            if (ppred.get_x() > pos.get_x() && succ.get_x() < pos.get_x()) return EventType::REGULAR;
            
            if (ppred.get_x() > pos.get_x() && succ.get_x() > pos.get_x()) return EventType::MERGE;
            
        } else if (pos.get_x() == succ.get_x())
        {
        } else assert(false);
        
        
        
        assert(false && "uncategorized vertex");
    }
}

class SimpEvent
{
public:
    SimpEvent();
    SimpEvent( Vertex pPos, EventType t ): pos(pPos), type(t) {}

    SimpEvent( const vector<Vertex> &vertices, uint64_t vertex_id): 
        pos(vertices[vertex_id])
    {
        assert( (vertices.front() != vertices.back()) && "For this algorithm, the duplicate back vertex must have been removed");
        type = classifyVertex(vertices, vertex_id);
    }
    
    bool operator==(const SimpEvent & other) const {return pos == other.pos; }
    bool operator!=(const SimpEvent & other) const {return pos != other.pos; }
    bool operator <(const SimpEvent &other) const  {return pos <  other.pos; }
private:
    Vertex pos;//, pred, succ;
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

    void remove(SimpEvent ev) { this->remove(ev); }
    
  
    void remove(Vertex v)
    {
        this->remove( SimpEvent(v, REGULAR));   //uses arbitrary event type 'REGULAR', which remove() ignores anyway
    }    
};

// ===========================================================

#if 0
typedef AVLTreeNode<LineSegment> *EdgeContainer;
 // protected inheritance to hide detail of AVLTree, since for a LineArrangement, AVLTree::insert must never be used
class LineArrangement: protected AVLTree<LineSegment>
{
public:
    EdgeContainer addEdge(const LineSegment &a, const BigFraction xPosition);

    bool isConsistent(EdgeContainer node, BigFraction xPos) const
    {
        bool left = !node->m_pLeft || 
                    (node->m_pLeft->m_Data.isLessOrEqual(node->m_Data, xPos) 
                    && isConsistent(node->m_pLeft, xPos));
                    
        bool right= !node->m_pRight ||
                    (node->m_Data.isLessOrEqual(node->m_pRight->m_Data, xPos) && 
                    isConsistent(node->m_pRight, xPos));
        return left && right;
    }
    
    int size() const { return AVLTree<LineSegment>::size(); }
    
    bool isConsistent(BigFraction xPos) const { return !m_pRoot || isConsistent(m_pRoot, xPos); }
    
    bool hasPredecessor( EdgeContainer node)
    {
        iterator it(node, *this);
        return it != begin();
    }

    ActiveEdge getPredecessor( EdgeContainer node)
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
    
    ActiveEdge getSuccessor( EdgeContainer node)
    {
        iterator it(node, *this);
        assert (++it != end());
        return *it;
    }
  
    void remove(ActiveEdge item, const BigFraction xPos)
    {
        AVLTreeNode<ActiveEdge> *node = findPos(item, xPos);
        assert( node && "Node to be removed does not exist");
        AVLTree<ActiveEdge>::remove(node);
    }
    
    EdgeContainer findPos(ActiveEdge item, const BigFraction xPos)
    {
    	if (! m_pRoot) return NULL;

	    BigFraction yVal = item.getYValueAt(xPos);

	    AVLTreeNode<ActiveEdge>* pPos = m_pRoot;
	    while (pPos)
	    {
	        BigFraction yVal2 = pPos->m_Data.getYValueAt(xPos);
	        if (yVal == yVal2) return pPos;
	        pPos = yVal < yVal2 ? pPos->m_pLeft : pPos->m_pRight;
	    }
	    return pPos;
    }
    
    list<EdgeContainer> findAllIntersectingEdges(ActiveEdge item, const BigFraction xPos)
    {
        /* findPos will return any ActiveEdge that intersects 'item' at xPos, not necessarily xPos itself*/
	    AVLTreeNode<ActiveEdge> *pPos = findPos(item, xPos);    
	    if (!pPos) return list<AVLTreeNode<ActiveEdge>*>();
	    
	    list<AVLTreeNode<ActiveEdge>*> res;
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
    /*
	AVLTree::iterator getIterator(ActiveEdge e, const BigInt xPos)
	{
	    AVLTreeNode<ActiveEdge>* p = findPos(e, xPos);
	    assert(p);
	    return AVLTree::iterator(p, *this);
	}*/

    /*void swapEdges(ActiveEdge &e1, ActiveEdge &e2, const int64_t xPosition)*/
};
#endif

#endif

