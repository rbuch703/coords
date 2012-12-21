
#ifndef SIMPLIFYPOLYGON_H
#define SIMPLIFYPOLYGON_H

#include "geometric_types.h"
#include "avltree.h"
#include <list>

#include "config.h"

void simplifyPolygon(const PolygonSegment &seg, list<PolygonSegment> &res);

typedef list<Vertex> OpenPolygon;

struct ActiveEdge
{
    ActiveEdge() {} 


    ActiveEdge(const Vertex pLeft, const Vertex pRight, const bool pIsTopEdge, OpenPolygon *pPoly):
        left(pLeft), right(pRight), isTopEdge(pIsTopEdge), poly(pPoly) 
        {
            assert (left < right);
        }
    Vertex left, right;
    bool isTopEdge; // we do a plane sweep in x-direction, so every active edge limits a polygon either as a top or a bottom edge
    OpenPolygon *poly;
    
    // @returns whether 'this' intersects with the vertical line at xPosition numerically earlier than 'other'
    // robustness: predicate is exact no matter what the input parameters are
    bool lessThan(const ActiveEdge & other, const BigInt &xPosition) const
    {
        assert(left.x <= right.x && other.left.x <= other.right.x);
        assert(left.x != right.x || other.left.x != other.right.x);
        
        if (left.x == right.x || other.left.x == other.right.x)
            assert(false && "Not implemented");

        BigInt num1 = (BigInt(right.y) - left.y)* (BigInt(xPosition) - left.x);
        BigInt denom1= BigInt(right.x) - left.x;
        BigInt offset1 = left.y;
        
        
        BigInt num2=  (BigInt(other.right.y) - other.left.y) * (BigInt(xPosition) - other.left.x);
        BigInt denom2= BigInt(other.right.x) - other.left.x;
        BigInt offset2= other.left.y;
        //double this_y = this_num / (double) this_denom + this_offset;
        //double this_y = this_num/ (double)this_denom + this_offset;
        //return this_y < other_y;
        
        bool pred = (num1 + offset1 * denom1) * denom2 < 
                    (num2 + offset2 * denom2) * denom1;
        return (denom1*denom2>= 0) ? pred : !pred;
        
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

    AVLTreeNode<ActiveEdge>* addEdge(const ActiveEdge &a, const BigInt xPosition)
    {
        num_items++;
        if (!m_pRoot)
	    {
	        m_pRoot = new AVLTreeNode<ActiveEdge>(NULL,NULL, NULL, a);;
	        m_pRoot->m_dwDepth = 0;    //no children --> depth=0
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
    
    void swapEdges(const ActiveEdge /*&e1*/, const ActiveEdge /*&e2*/, const int64_t /*xPosition*/)
    {
        
    }
private:
};


#endif

