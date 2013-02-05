
#ifndef SIMPLIFYPOLYGON_H
#define SIMPLIFYPOLYGON_H

#include "geometric_types.h"
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

void simplifyPolygon(const PolygonSegment &seg, list<PolygonSegment> &res);

typedef list<Vertex> OpenPolygon;

struct ActiveEdge
{
    ActiveEdge() {} 
    ActiveEdge(const Vertex pLeft, const Vertex pRight);

    bool isLessThan(const ActiveEdge & other, const BigFraction &xPosition) const;
    bool isEqual(const ActiveEdge & other, const BigFraction &xPosition) const;
    bool isLessThanOrEqual(const ActiveEdge & other, const BigFraction &xPosition) const;
    bool operator<(const ActiveEdge &);

    operator bool() { return (left != Vertex(0,0))  and (right != Vertex(0,0)); }
    Vertex left, right;
    /* don't need this here, we can store the mapping between active edges and their
     * intersections in a separate map. This makes ActiveEdge a small object, that can
     * easily be copied ( instead of using pointers to the originals); */
    //std::list<ActiveEdge*> intersections;  
        
protected:
    BigFraction getYValueAt(const BigFraction &xPosition) const;
};
std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge);

enum SimpEventType { SEG_START, SEG_END, INTERSECTION };

// ===========================================================

class SimpEvent
{
public:
    SimpEvent();
    SimpEvent( SimpEventType pType, ActiveEdge pThisEdge, 
                ActiveEdge pOtherEdge);
    bool operator==(const SimpEvent & other) const;
    bool operator!=(const SimpEvent & other) const;
    bool operator <(const SimpEvent &other) const;
    
    SimpEventType type;
    ActiveEdge m_thisEdge, m_otherEdge;

    // x and y coordinates for the position at which the event takes place
    BigFraction      x, y;
};

class SimpEventQueue : protected AVLTree<SimpEvent> 
{
public:
    void add(const SimpEventType type, ActiveEdge thisEdge, 
             ActiveEdge otherEdge = ActiveEdge( Vertex(0,0), Vertex(0,0)) )
    {
        insert( SimpEvent( type, thisEdge, otherEdge));
    }

    bool containsEvents() const { return size();}
    
	SimpEvent pop() 
	{
	    assert (size() >0); 
	    SimpEvent item = *begin();
	    AVLTree<SimpEvent>::remove (item); 
	    return item;
    }
    
    bool contains(const SimpEventType type, ActiveEdge thisEdge, ActiveEdge otherEdge)
    {
        return AVLTree<SimpEvent>::contains( SimpEvent(type, thisEdge, otherEdge) ) || 
               AVLTree<SimpEvent>::contains( SimpEvent(type, otherEdge,thisEdge ) );
    }
    
    void remove(const SimpEventType type, ActiveEdge thisEdge, ActiveEdge otherEdge)
    {
        /*assert ( AVLTree<SimpEvent>::contains( SimpEvent(type, thisEdge, otherEdge))  !=
                 AVLTree<SimpEvent>::contains( SimpEvent(type, otherEdge,thisEdge)));
        if (AVLTree<SimpEvent>::contains( SimpEvent(type, thisEdge, otherEdge)))*/
        AVLTree<SimpEvent>::remove(SimpEvent(type, thisEdge, otherEdge));
        /*else
            AVLTree<SimpEvent>::remove(SimpEvent(type, otherEdge, thisEdge));*/
    }
    
    
};

// ===========================================================

class LineArrangement: public AVLTree<ActiveEdge>
{
public:
    AVLTreeNode<ActiveEdge>* addEdge(const ActiveEdge &a, const BigFraction xPosition);
    
    bool hasPredecessor( AVLTreeNode<ActiveEdge>* node)
    {
        iterator it(node, *this);
        return it != begin();
    }

    ActiveEdge getPredecessor( AVLTreeNode<ActiveEdge>* node)
    {
        iterator it(node, *this);
        assert (it != begin());
        return *it;
    }
    
    bool hasSuccessor( AVLTreeNode<ActiveEdge>* node )
    {
        iterator it(node, *this);
        return ++it != end();
    }
    
    ActiveEdge getSuccessor( AVLTreeNode<ActiveEdge>* node)
    {
        iterator it(node, *this);
        assert (++it != end());
        return *it;
    }
    /*AVLTreeNode<ActiveEdge>* findPos(ActiveEdge item, const BigInt xPos)
    {
    	if (! m_pRoot) return NULL;

	    AVLTreeNode<ActiveEdge>* pPos = m_pRoot;
	    while ( pPos->m_Data != item)
	    {
	        pPos = (lessThan(item, pPos->m_Data) ) ? pPos->m_pLeft : pPos->m_pRight;
		    if (!pPos) return NULL;
	    }
	    return pPos;
    }
    
	AVLTree::iterator getIterator(ActiveEdge e, const BigInt xPos)
	{
	    AVLTreeNode<ActiveEdge>* p = findPos(e, xPos);
	    assert(p);
	    return AVLTree::iterator(p, *this);
	}*/

    /*void swapEdges(ActiveEdge &e1, ActiveEdge &e2, const int64_t xPosition)*/
};


#endif

