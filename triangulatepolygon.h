
#ifndef SIMPLIFYPOLYGON_H
#define SIMPLIFYPOLYGON_H

#include "geometric_types.h"
#include "vertexchain.h"
#include "avltree.h"
#include <list>

#include "config.h"


/** necessary properties:
    ActiveEdge:
        - left vertex, right vertex
        - left collision event, right collision event
        - left adjacent edge
        - right adjacent edge
 
**/

//void simplifyPolygon(const PolygonSegment &seg, list<PolygonSegment> &res);

typedef list<Vertex> OpenPolygon;


/* Order is important, since events for the same 2D-Point are processed in the
 * order this enumeration is defined. With this order, it is ensured that all
 * intersections for a point are found event if some line segements also start 
 * or end at that point. */
enum EventType { START, STOP, SPLIT, MERGE, REGULAR };

// ===========================================================

#error CONTINUEHERE determine algorithm sweep direction, verify classification, especially for cases with identical x/y-values
#warning handle edge case than thwo adjacent vertices share the same x/y value and thus none is a start/stop/split/merge vertex
EventType classifyVertex(Vertex pos, Vertex pred, Vertex succ)
{
    //FIXME set event type based on spacial relation between the three vertices
    assert( pos != pred && pos != succ && pred != succ);
    if (pos < pred && pos< succ)    //is either a START or a SPLIT vertex
    {
    } else if (pos > pred && pos > succ) //is either a STOP or a MERGE vertex
    {
    } else //is a regular vertex
    {
        return EventType::REGULAR;
    }
}

class SimpEvent
{
public:
    SimpEvent();
    SimpEvent( Vertex pPos, EventType t ): pos(pPos), type(t) {}

    SimpEvent( Vertex pPos, Vertex pPred, Vertex pSucc ): 
        pos(pPos)
    {
        //TODO: do we need 'pred' and 'succ' after the event type has been determined?
        type = classifyVertex(pos, pPred, pSucc);
    }
    
    bool operator==(const SimpEvent & other) const {return pos == other.pos; }
    bool operator!=(const SimpEvent & other) const {return pos != other.pos; }
    bool operator <(const SimpEvent &other) const  {return pos <  other.pos; }
private:
    EventType type;
    Vertex pos;//, pred, succ;
};


//:TODO replace by a heap
class MonotonizeEventQueue : protected AVLTree<SimpEvent> 
{
public:

    int size() const { return AVLTree<SimpEvent>::size(); }

    void add( Vertex pos, Vertex pred, Vertex succ )
    {
        insert( SimpEvent( pos, pred, succ));
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
        this->remove( SimpEvent(v, REGULAR));   //arbitrary event type, remove() is based only on the Vertex
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

