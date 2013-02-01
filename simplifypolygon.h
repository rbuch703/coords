
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

    bool lessThan(const ActiveEdge & other, const BigFraction &xPosition) const;
    bool operator<(const ActiveEdge &);

    Vertex left, right;
    std::list<ActiveEdge*> intersections;
    
};
std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge);

enum SimpEventType { SEG_START, SEG_END, INTERSECTION };

// ===========================================================

class SimpEvent
{
public:
    SimpEvent();
    SimpEvent( SimpEventType pType, ActiveEdge* pThisEdge, ActiveEdge* pOtherEdge = NULL);
    bool operator==(const SimpEvent & other) const;
    bool operator!=(const SimpEvent & other) const;
    bool operator <(const SimpEvent &other) const;
    
    SimpEventType type;
    ActiveEdge *m_thisEdge, *m_otherEdge;

    // x and y coordinates for the position at which the event takes place
    BigFraction      x, y;
};

// ===========================================================

class LineArrangement: public AVLTree<ActiveEdge>
{
public:
    AVLTreeNode<ActiveEdge>* addEdge(const ActiveEdge &a, const BigFraction xPosition);
/*    AVLTreeNode<ActiveEdge>* findPos(ActiveEdge item, const BigInt xPos)
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

