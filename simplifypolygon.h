
#ifndef SIMPLIFYPOLYGON_H
#define SIMPLIFYPOLYGON_H

#include "geometric_types.h"
#include "avltree.h"
#include <list>

void simplifyPolygon(const PolygonSegment &seg, list<PolygonSegment> &res);

typedef list<Vertex> OpenPolygon;

struct ActiveEdge
{
    ActiveEdge(const Vertex pLeft, const Vertex pRight, const bool pIsTopEdge, OpenPolygon *pPoly):
        left(pLeft), right(pRight), isTopEdge(pIsTopEdge), poly(pPoly) 
        {
            assert (left < right);
        }
    Vertex left, right;
    bool isTopEdge; // we do a plane sweep in x-direction, so every active edge limits a polygon either as a top or a bottom edge
    OpenPolygon *poly;
    

    bool lessThan(const ActiveEdge & other, int64_t xPosition) const
    {
        assert(left.x <= right.x && other.left.x <= other.right.x);
        assert(left.x != right.x || other.left.x != other.right.x);
        
        if (left.x == right.x || other.left.x == other.right.x)
            assert(false && "Not implemented");

        double this_y = (right.y - left.y)/ (double)(right.x - left.x) * (xPosition - left.x) + left.y;
        double other_y =(other.right.y - other.left.y)/ (double)(other.right.x - other.left.x) * 
                        (xPosition - other.left.x) + other.left.y;
        return this_y < other_y;
    }
    
    //the operator needs to be defined for the AVLTree to work at all; but it must never be used,
    //because the comparision of two ActiveEdges has no meaning unless the y position at which
    //the comparison is to be evaluated is provided as well.
    bool operator<(const ActiveEdge &) { 
        assert(false && "ActiveEdges cannot be compared");
    }
};

std::ostream& operator <<(std::ostream& os, const ActiveEdge &edge);


class LineArrangement: public AVLTree<ActiveEdge>
{
public:

    AVLTreeNode<ActiveEdge>* addEdge(const ActiveEdge &a, const int64_t xPosition)
    {
        if (!m_pRoot)
	    {
	        m_pRoot = new AVLTreeNode<ActiveEdge>(NULL,NULL, NULL, a);;
	        m_pRoot->m_dwDepth = 0;    //no children --> depth=0
	        num_items++;
	        return m_pRoot;
	    }
        
        AVLTreeNode<ActiveEdge> *parent;//= m_pRoot;
        AVLTreeNode<ActiveEdge> **pos = &m_pRoot;
        do
        {
            parent = *pos;
            pos = a.lessThan(parent->m_Data, xPosition) ? &parent->m_pLeft : &parent->m_pRight;
        } while (*pos);
        
        assert (! *pos);
        *pos = new AVLTreeNode<ActiveEdge>(NULL, NULL, parent, a);
        (*pos)->m_dwDepth = 0;
        
        updateDepth(*pos);
        return *pos;
    }
private:
};


#endif

