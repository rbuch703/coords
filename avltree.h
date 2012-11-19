#ifndef AVLTREE_H
#define AVLTREE_H

#include <assert.h>
#include <stdlib.h> // for abs()
#include <stdint.h>

#include <iostream>

using namespace std;

template<class t> class AVLTree ; //forward declaration
template<class t> class AVLTreeIterator ; //forward declaration
template<class t> class AVLTreeConstIterator;

template<class t> 
class AVLTreeNode {
public:
	AVLTreeNode<t>(AVLTreeNode<t> *pLeft = NULL, AVLTreeNode<t> *pRight = NULL, 
	               AVLTreeNode<t> *pParent = NULL, const t &data = 0): 
	  m_Data(data), m_pLeft(pLeft), m_pRight(pRight), m_pParent(pParent), m_dwDepth(-1) {}
	 t m_Data;
	 
	void deleteRecursive() 
	{
		if (m_pLeft) m_pLeft->deleteRecursive();
		if (m_pRight) m_pRight->deleteRecursive();
		delete this;
	}
	
	uint32_t getNumChildren()
	{
	    uint32_t nChildren = 0;
	    if (m_pLeft) nChildren+= 1 + m_pLeft->getNumChildren();
	    if (m_pRight) nChildren+= 1 + m_pRight->getNumChildren();
	}
private:
	void check(); //no return value, a failed check should always fail an assertion

	AVLTreeNode<t>* m_pLeft, *m_pRight, *m_pParent; //array indices for those
	int m_dwDepth;

public:
	friend class AVLTree<t>;
	friend class AVLTreeIterator<t>;
	friend class AVLTreeConstIterator<t>;
};

template<class t> class AVLTree {
public:
	AVLTree<t>(): m_pRoot(NULL) { }
	virtual ~AVLTree<t>() { if (m_pRoot) m_pRoot->deleteRecursive(); }
	AVLTreeNode<t>* insert(const t &item);
	bool contains( const t &item) const;
	t& getItem( const t &item);
	
	void print() const { print(m_pRoot);}
	void check(); //check consistency
	int size() const { return (m_pRoot) ? 1 + m_pRoot->getNumChildren() : 0; }
	//void sort(DynamicArray<t> &res) const { sort(res, m_pRoot);};
	void sort(t* dest) const { int dest_idx = 0; asSortedArray(dest, dest_idx, m_pRoot); }
	
	virtual void clear() { if (m_pRoot) m_pRoot->deleteRecursive(); m_pRoot = NULL;}
private:
	void print(AVLTreeNode<t> *root) const;
	void rearrange( AVLTreeNode<t>* pNode, AVLTreeNode<t>* pred1, AVLTreeNode<t>* pred2);
    void sort( t* dest, int &dest_idx, AVLTreeNode<t> *root) const;
	//void sort( DynamicArray<t> &res, int idx) const;
	int newTreeNode(); 

protected:
	void updateDepth( AVLTreeNode<t> *pNode, AVLTreeNode<t> *pred1, AVLTreeNode<t> *pred2 );
	AVLTreeNode<t>* findPos(const t &item, AVLTreeNode<t> *&parent ) const;
	AVLTreeNode<t>* findPos(const t &item) const;
	AVLTreeNode<t>* insert(const t &item, AVLTreeNode<t> *parent);
	
	
	//AVLTreeNode<t> *m_pNodes; //array of tree nodes owned (i.e. that are part of) this tree
	AVLTreeNode<t> *m_pRoot;
	//int m_nMaxNodes;
	//int m_nNodes;
	
	friend class AVLTreeIterator<t>;
	friend class AVLTreeConstIterator<t>;

};

template <class t>
class AVLTreeIterator {
public:
	AVLTreeIterator<t>(AVLTree<t> &tree) : m_tree(tree), m_pCurrent (0) {};
	t& item() 	{ 
//		std::cout << (m_pCurrent) << " ";
		return m_tree.m_pNodes[m_pCurrent].m_Data; }
	// "int" is just the marker for postfix operators
	bool operator ++(int) {	
		return (++m_pCurrent <= m_tree.m_nNodes); }
	void reset() {m_pCurrent = 0;}
private:
	AVLTree<t> &m_tree;
	int m_pCurrent;
};

template <class t>
class AVLTreeConstIterator {
public:
	AVLTreeConstIterator<t>(const AVLTree<t> &tree) : m_tree(tree), m_pCurrent (0) {};
	const t& item() 	{ 
//		std::cout << (m_pCurrent) << " ";
        assert(m_pCurrent > 0);
		return m_tree.m_pNodes[m_pCurrent].m_Data; }
	// "int" is just the marker for postfix operators
	bool operator ++(int) {	
		return (++m_pCurrent <= m_tree.m_nNodes); }
	void reset() {m_pCurrent = 0;}
private:
	const AVLTree<t> &m_tree;
	int m_pCurrent;
};

// ================ end of declarations =============================

// ================ start of AVLTree<t> definitions =================

template <class t>
void AVLTree<t>::print(AVLTreeNode<t> *root) const
{
	if (! root)	return;
	if (root->m_pLeft) print(root->m_pLeft);
	cout << root->m_Data << endl;
	if (root->m_pRight) print(root->m_pRight);
}

template <class t>
void AVLTree<t>::sort( t* dest, int &dest_idx, AVLTreeNode<t> *root) const
{
    if (! root) return;
	if (root->m_pLeft) asSortedArray(dest, dest_idx, root->m_pLeft);
	dest[dest_idx++] = root->m_Data;
	if (root->m_pRight) asSortedArray(dest, dest_idx, root->m_pRight);
}
/*
template <class t>
void AVLTree<t>::sort( DynamicArray<t> &res, int idx) const
{
    if (idx == 0) return;
    sort(res, m_pNodes[idx].m_pLeft);
    res.add(m_pNodes[idx].m_Data);
    sort(res, m_pNodes[idx].m_pRight);
}*/

template <class t>
AVLTreeNode<t>* AVLTree<t>::insert(const t &item, AVLTreeNode<t> *parent)
{
	AVLTreeNode<t>* pPos = new AVLTreeNode<t>(0,0, parent, item);
	
	if (!parent) 
	{
	    m_pRoot = pPos;
	    pPos->m_dwDepth = 0;    //no children --> depth=0
	    return pPos;   //no parent --> this is the root, nothing more to do
	}
	
	if (item < parent->m_Data)
	{
	    assert( ! parent->m_pLeft);
	    parent->m_pLeft = pPos;
	} else
	{
	    assert( parent->m_Data != item);
	    assert( ! parent->m_pRight);
	    parent->m_pRight = pPos;
	}
	
	updateDepth( pPos, NULL, NULL);
	return pPos;
}

template <class t>
AVLTreeNode<t>* AVLTree<t>::insert(const t &item)
{
	AVLTreeNode<t>* pParent = NULL;
#ifdef NDEBUG
    findPos(item, pParent);
#else
	AVLTreeNode<t>* pPos = findPos(item, pParent);
	assert(! pPos); //no duplicates allowed
#endif
/*	if (pPos)
	{
		if (replaceDuplicate)
			m_pNodes[pPos].m_Data = item;
		return replaceDuplicate;	// success if old version has been overwritten, failure if not
	}*/

    return insert (item, pParent);
}

//void sort(out DynamicArray<MultiProxel> &res);

template <class t>
AVLTreeNode<t>* AVLTree<t>::findPos(const t &item, AVLTreeNode<t> *&parent) const
{
	parent = NULL;
	if (! m_pRoot) return NULL;

	AVLTreeNode<t> *pPos = m_pRoot;
	while ( pPos->m_Data != item)
	{
		parent = pPos;
		pPos = (item < pPos->m_Data) ? pPos->m_pLeft : pPos->m_pRight;
		if (! pPos) return NULL;
	}
	return pPos;
}


template <class t>
AVLTreeNode<t>* AVLTree<t>::findPos(const t &item) const
{
	if (! m_pRoot) return NULL;

	AVLTreeNode<t>* pPos = m_pRoot;
	while ( pPos->m_Data != item)
	{
	    pPos = (item < pPos->m_Data) ? pPos->m_pLeft : pPos->m_pRight;

		if (!pPos) return NULL;
	}
	return pPos;
}


template <class t> 
void AVLTree<t>::check() //check consistency
{
	if (m_pRoot) m_pRoot->check();
}

template <class t>
t& AVLTree<t>::getItem( const t &item)
{
	AVLTreeNode<t> *p = findPos(item);
	if (!p)
	{
        insert(item);
	    p = findPos(item);
	}
	assert(p);
	return p->m_Data;
}

//must not be static, because it calls the non-static rearrange, which needs the this->m_pRoot object
template <class t>
void AVLTree<t>::updateDepth( AVLTreeNode<t> *pNode, AVLTreeNode<t> *pred1, AVLTreeNode<t> *pred2)
{
	int leftDepth = pNode->m_pLeft ? 1 + pNode->m_pLeft->m_dwDepth : 0;
	int rightDepth = pNode->m_pRight ? 1 + pNode->m_pRight->m_dwDepth : 0;
	int newDepth = leftDepth > rightDepth ? leftDepth : rightDepth;
	if (newDepth != pNode->m_dwDepth)
	{
		pNode->m_dwDepth = newDepth;
		if (abs(leftDepth - rightDepth) > 1)
			rearrange( pNode, pred1, pred2);
		else if (pNode->m_pParent)	// if the nodes have been rearranged, rearrange() will do the recursive updateDepth() with the new node order
			updateDepth( pNode->m_pParent, pNode, pred1);
	}
}


// rearranges the tree section containing pNode, pred1, pred2 and their children by
// - sorting the three nodes
// - sorting the four child nodes
// - using the sorted value to re-balance the sub-tree to the following layout:
//                  pNodes[1]
//      pNodes[0]       pNodes[2]
//   pCh[0]   pCh[1]  pCh[2]  pCh[3]

template <class t>
void AVLTree<t>::rearrange( AVLTreeNode<t>* pNode, AVLTreeNode<t>* pred1, AVLTreeNode<t>* pred2)
{
    assert( pNode && pred1 && pred2);
	AVLTreeNode<t>* pNodes[3];
	AVLTreeNode<t>* pChildren[4];
	
	bool isRoot = (! pNode->m_pParent); 
	bool isLeftChild = !isRoot ? ( pNode->m_pParent->m_pLeft == pNode) : false;
	if ((pNode->m_pLeft == pred1) && (pred1->m_pLeft == pred2) )
	{
		pNodes[0] = pred2;                         //                     pNode
		pNodes[1] = pred1;                         //            pred1             child
		pNodes[2] = pNode;                         //      pred2      child
								                   //  child   child
		pChildren[0] = pred2->m_pLeft;
		pChildren[1] = pred2->m_pRight;
		pChildren[2] = pred1->m_pRight;
		pChildren[3] = pNode->m_pRight;
	} else
	if ((pNode->m_pLeft == pred1) && (pred1->m_pRight == pred2) )
	{
		pNodes[0] = pred1;                         //                     pNode
		pNodes[1] = pred2;                         //            pred1             child
		pNodes[2] = pNode;                         //       child     pred2
											       //             child   child
		pChildren[0] = pred1->m_pLeft;
		pChildren[1] = pred2->m_pLeft;
		pChildren[2] = pred2->m_pRight;
		pChildren[3] = pNode->m_pRight;
	} else
	if ((pNode->m_pRight == pred1) && (pred1->m_pRight == pred2))
	{
		pNodes[0] = pNode;                         //                     pNode
		pNodes[1] = pred1;                         //            child             pred1
		pNodes[2] = pred2;                         //                         child     pred2
								                   //                               child   child
		pChildren[0] = pNode->m_pLeft;
		pChildren[1] = pred1->m_pLeft;
		pChildren[2] = pred2->m_pLeft;
		pChildren[3] = pred2->m_pRight;
	} else
	if ((pNode->m_pRight == pred1) && (pred1->m_pLeft == pred2))
	{
		pNodes[0] = pNode;                         //                     pNode
		pNodes[1] = pred2;                         //            child             pred1
		pNodes[2] = pred1;                         //                        pred2       child
												   //                    child   child
		pChildren[0] = pNode->m_pLeft;
		pChildren[1] = pred2->m_pLeft;
		pChildren[2] = pred2->m_pRight;
		pChildren[3] = pred1->m_pRight;
	} else
	{
		assert( false && "Inconsistency found in AVL tree");
		exit(1);
	}

// new Layout
  //              pNodes[1]
  //      pNodes[0]       pNodes[2]
  //   pCh[0]   pCh[1]  pCh[2]  pCh[3]

	if (isRoot)
	{
		m_pRoot = pNodes[1];
		pNodes[1]->m_pParent = NULL;
	} else
	{
    	AVLTreeNode<t> *pParent = pNode->m_pParent;
		if (isLeftChild)
			pParent->m_pLeft = pNodes[1];
		else pParent->m_pRight = pNodes[1];
		pNodes[1]->m_pParent = pParent;
	}

	pNodes[0]->m_pParent = pNodes[1];
	pNodes[2]->m_pParent = pNodes[1];
	
	pNodes[1]->m_pLeft  = pNodes[0];
	pNodes[1]->m_pRight = pNodes[2];
	
	pNodes[0]->m_pLeft = pChildren[0];
	if (pChildren[0]) pChildren[0]->m_pParent = pNodes[0];
	
	pNodes[0]->m_pRight = pChildren[1];
	if (pChildren[1]) pChildren[1]->m_pParent = pNodes[0];
	
	pNodes[2]->m_pLeft = pChildren[2];
	if (pChildren[2]) pChildren[2]->m_pParent = pNodes[2];
	
	pNodes[2]->m_pRight = pChildren[3];
	if (pChildren[3]) pChildren[3]->m_pParent = pNodes[2];
	
	// we changed the children of pNodes[0], so we need to adjust its depth
	int nLeft  = (pChildren[0]) ? pChildren[0]->m_dwDepth + 1 : 0;
	int nRight = (pChildren[1]) ? pChildren[1]->m_dwDepth + 1 : 0;
	pNodes[0]->m_dwDepth = nLeft > nRight ? nLeft : nRight;
	
	// we changed the children of pNodes[2], so we need to adjust its depth
	nLeft  = (pChildren[2]) ? pChildren[2]->m_dwDepth + 1 : 0;
	nRight = (pChildren[3]) ? pChildren[3]->m_dwDepth + 1 : 0;
	pNodes[2]->m_dwDepth = nLeft > nRight ? nLeft : nRight;
	
	updateDepth( pNodes[1], pNodes[0], pChildren[0]);

}


template <class t>
bool AVLTree<t>::contains( const t &item) const
{
	return (*findPos(item)); // if the result of findPos points to an existing point, the tree contains the item
}

//=======================Start of code for ALVTreeNode =========================
template <class t> 
void AVLTreeNode<t>::check() //check consistency
{
	if (m_pLeft)
	{
		assert( m_pLeft->m_pParent = this);
		assert( m_Data > m_pLeft->m_Data);
		m_pLeft->check();
	}
	if (m_pRight)
	{
		assert( m_pRight->m_pParent = this);
		assert( m_Data < m_pRight->m_Data);
		m_pRight->check();
	}
	
	int left_depth = m_pLeft ? 1 + m_pLeft->m_dwDepth : 0;
	int right_depth =m_pRight? 1 + m_pRight->m_dwDepth: 0;
	assert( abs(left_depth - right_depth) <= 1);

    int depth = left_depth > right_depth ? left_depth : right_depth;
    assert ( m_dwDepth == depth);
}
#endif
