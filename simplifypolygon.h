
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
    bool isLessOrEqual(const ActiveEdge & other, const BigFraction &xPosition) const;
    bool operator<(const ActiveEdge &);

    LineSegment toLineSegment() { return LineSegment(left, right); }

    void getIntersectionWith(ActiveEdge &other, BigFraction &out_x, BigFraction &out_y) const;
    bool intersects(ActiveEdge other) const;
    
    explicit operator bool() { return (left != Vertex(0,0))  and (right != Vertex(0,0)); }
    Vertex left, right;
    /* don't need this here, we can store the mapping between active edges and their
     * intersections in a separate map. This makes ActiveEdge a small object, that can
     * easily be copied ( instead of using pointers to the originals); */
    //std::list<ActiveEdge*> intersections;  
        
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
    
	SimpEvent top() 
	{
	    assert (size() >0); 
	    return *begin();
    }
    
  
    void remove(const SimpEventType type, ActiveEdge thisEdge, ActiveEdge otherEdge)
    {
        AVLTree<SimpEvent>::remove(SimpEvent(type, thisEdge, otherEdge));
    }
    
    void scheduleIntersectionIfExists(ActiveEdge a, ActiveEdge b, BigFraction x_pos, BigFraction y_pos)
    {
        if ( a.intersects(b) )
        {
            BigFraction x,y;
                        
            a.getIntersectionWith( b, /*out*/x, /*out*/y);
            assert ( (x != x_pos || y != y_pos) && "not implemented");
            
            if ( x > x_pos || (x == x_pos && y > y_pos))
                this->add( INTERSECTION, a, b);
        }
    }

    void removeIntersectionIfExists(ActiveEdge a, ActiveEdge b, BigFraction x_pos, BigFraction y_pos)
    {
        if ( a.intersects( b ) )
        {
            BigFraction x,y;

            a.getIntersectionWith(b, x, y);
            assert ( (x != x_pos || y != y_pos)  && "not implemented");
            if ( x > x_pos || (x == x_pos && y > y_pos))
                this->remove(INTERSECTION, a, b);
        }
    }
    
};

// ===========================================================

 // protected inheritance to hide detail of AVLTree, since for a LineArrangement, AVLTree::insert must never be used
class LineArrangement: protected AVLTree<ActiveEdge>
{
public:
    AVLTreeNode<ActiveEdge>* addEdge(const ActiveEdge &a, const BigFraction xPosition);

    bool isConsistent(AVLTreeNode<ActiveEdge>* node, BigFraction xPos) const
    {
        bool left = !node->m_pLeft || (node->m_pLeft->m_Data.isLessOrEqual(node->m_Data, xPos) && isConsistent(node->m_pLeft, xPos));
        bool right= !node->m_pRight|| (node->m_Data.isLessOrEqual(node->m_pRight->m_Data, xPos) && isConsistent(node->m_pRight, xPos));
        return left && right;
    }

    
    bool isConsistent(BigFraction xPos) const { return isConsistent(m_pRoot, xPos); }
    
    bool hasPredecessor( AVLTreeNode<ActiveEdge>* node)
    {
        iterator it(node, *this);
        return it != begin();
    }

    ActiveEdge getPredecessor( AVLTreeNode<ActiveEdge>* node)
    {
        iterator it(node, *this);
        assert (it != begin());
        return *(--it);
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
  
    void remove(ActiveEdge item, const BigFraction xPos)
    {
        AVLTreeNode<ActiveEdge> *node = findPos(item, xPos);
        assert( node && "Node to be removed does not exist");
        AVLTree<ActiveEdge>::remove(node);
    }
    
    AVLTreeNode<ActiveEdge>* findPos(ActiveEdge item, const BigFraction xPos)
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
    
    list<AVLTreeNode<ActiveEdge>*> findAllIntersectingEdges(ActiveEdge item, const BigFraction xPos)
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

