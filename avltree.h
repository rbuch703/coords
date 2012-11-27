
#ifndef AVLTREE_H
#define AVLTREE_H

#include <assert.h>
#include <stdlib.h> // for abs()
#include <stdint.h>

#include <iostream> // for cout

//using namespace std;

//forward declarations
template<class t> class AVLTree ; 
//template<class t> class iterator ; 
//template<class t> class const_iterator;

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
	    return nChildren;
	}
private:
	void check(); //no return value, a failed check should always fail an assertion

	AVLTreeNode<t>* m_pLeft, *m_pRight, *m_pParent; //array indices for those
	int m_dwDepth;

public:
	friend class AVLTree<t>;
	friend class LineArrangement;
	//friend class iterator<t>;
	//friend class const_iterator<t>;
};

template<class t> class AVLTree {
public:
	AVLTree<t>(): m_pRoot(NULL), num_items(0) { }
	virtual ~AVLTree<t>() { if (m_pRoot) m_pRoot->deleteRecursive(); }
	
	AVLTreeNode<t>* insert(const t &item);
	bool contains( const t &item) const;
	t&    getItem( const t &item);
	t& operator[]( const t &item) { return getItem(item);}
	void  remove ( const t &item);
	
	t     pop() 
	{ 
	    t item = *begin();
	    remove (item); 
	    return item;
    }
	
	void print() const { print(m_pRoot);}
	void check(); //check consistency
	int size() const { 
	    #warning extensive debug checks
	    uint32_t num = (m_pRoot) ? 1 + m_pRoot->getNumChildren() : 0; 
	    assert (num == num_items);
	    return num_items;
    }
	void sort(t* dest) const { int dest_idx = 0; asSortedArray(dest, dest_idx, m_pRoot); }
	
	void clear() { if (m_pRoot) m_pRoot->deleteRecursive(); m_pRoot = NULL;}
	
	class iterator ; 
	class const_iterator ; 
	iterator begin();
	iterator end();

    //typedef iterator iterator;	
    //typedef const_iterator const_iterator;	
protected:
	AVLTreeNode<t>* rearrange( AVLTreeNode<t>* pNode, AVLTreeNode<t>* pred1, AVLTreeNode<t>* pred2);
    AVLTreeNode<t>* rearrangeRotateLeft( AVLTreeNode<t>* pNode);
    AVLTreeNode<t>* rearrangeRotateRight(AVLTreeNode<t>* pNode);
	AVLTreeNode<t>* findPos(const t &item, AVLTreeNode<t> *&parent ) const;
	AVLTreeNode<t>* findPos(const t &item) const;
	AVLTreeNode<t>* insert(const t &item, AVLTreeNode<t> *parent);

	void updateDepth( AVLTreeNode<t> *pNode );
    void sort( t* dest, int &dest_idx, AVLTreeNode<t> *root) const;
	void print(AVLTreeNode<t> *root, int depth = 0) const;
protected:    
    AVLTreeNode<t> *m_pRoot;
    uint64_t num_items;	
	

public:
    class iterator {
    public:
	    iterator(AVLTreeNode<t> *node, AVLTree<t> &tree) : 
            m_pCurrent (node), m_Tree(tree) {};
	
	    t& operator*() 	{ return m_pCurrent->m_Data; }
	    t* operator->() { return & (m_pCurrent->m_Data);}
	    // "int" is just the marker for postfix operators
	    iterator& operator ++(int) 
	    {	
	        if (m_pCurrent->m_pRight)
	        {
	            m_pCurrent = m_pCurrent->m_pRight;
	            while (m_pCurrent->m_pLeft) 
	                m_pCurrent = m_pCurrent->m_pLeft;
                return *this;
	        }

            bool isLeftChild;
            do 
            {
                isLeftChild = m_pCurrent->m_pParent && (m_pCurrent->m_pParent->m_pLeft == m_pCurrent);
                m_pCurrent = m_pCurrent->m_pParent;
            } while (m_pCurrent && !isLeftChild);
            
            return *this;	
	    }

	    iterator& operator ++() { return (*this)++;} 

	    iterator& operator --(int) 
	    {	
	        if (m_pCurrent == NULL)
	        {
	            m_pCurrent = m_Tree.m_pRoot;
	            if (! m_pCurrent) return *this;
	            while (m_pCurrent->m_pRight) m_pCurrent = m_pCurrent->m_pRight;
	            return *this;
	        }
	        
	        if (m_pCurrent->m_pLeft)
	        {
	            m_pCurrent = m_pCurrent->m_pLeft;
	            while (m_pCurrent->m_pRight) 
	                m_pCurrent = m_pCurrent->m_pRight;
                return *this;
	        }

            bool isRightChild;
            do 
            {
                isRightChild = m_pCurrent->m_pParent && (m_pCurrent->m_pParent->m_pRight == m_pCurrent);
                m_pCurrent = m_pCurrent->m_pParent;
            } while (m_pCurrent && !isRightChild);
            return *this;	
	    }

	    iterator& operator --() { (*this)--;} 

	
	    bool operator==(const iterator &other) const { return m_pCurrent == other.m_pCurrent;}
	    bool operator!=(const iterator &other) const { return m_pCurrent != other.m_pCurrent;}
	    friend class const_iterator;
    private:
	    AVLTreeNode<t> *m_pCurrent;
	    AVLTree<t>     &m_Tree;
    };

    class const_iterator {
    public:
	    const_iterator(AVLTreeNode<t> *node, AVLTree<t> &tree) : 
            m_pCurrent (node), m_Tree(tree) {};

	    const_iterator(const iterator &other) : m_pCurrent (other.m_pCurrent), m_Tree(other.m_Tree) {};
	
	    const t& operator*() 	{ return m_pCurrent->m_Data; }
	    const t* operator->() { return & (m_pCurrent->m_Data);}
	    // "int" is just the marker for postfix operators
	    const_iterator& operator ++(int) 
	    {	
	        if (m_pCurrent->m_pRight)
	        {
	            m_pCurrent = m_pCurrent->m_pRight;
	            while (m_pCurrent->m_pLeft) 
	                m_pCurrent = m_pCurrent->m_pLeft;
                return *this;
	        }

            bool isLeftChild;
            do 
            {
                isLeftChild = m_pCurrent->m_pParent && (m_pCurrent->m_pParent->m_pLeft == m_pCurrent);
                m_pCurrent = m_pCurrent->m_pParent;
            } while (m_pCurrent && !isLeftChild);
            
            return *this;	
	    }

	    const_iterator& operator ++() { return (*this)++;} 

	    const_iterator& operator --(int) 
	    {	
	        if (m_pCurrent == NULL)
	        {
	            m_pCurrent = m_Tree.m_pRoot;
	            if (! m_pCurrent) return *this;
	            while (m_pCurrent->m_pRight) m_pCurrent = m_pCurrent->m_pRight;
	            return *this;
	        }
	        
	        if (m_pCurrent->m_pLeft)
	        {
	            m_pCurrent = m_pCurrent->m_pLeft;
	            while (m_pCurrent->m_pRight) 
	                m_pCurrent = m_pCurrent->m_pRight;
                return *this;
	        }

            bool isRightChild;
            do 
            {
                isRightChild = m_pCurrent->m_pParent && (m_pCurrent->m_pParent->m_pRight == m_pCurrent);
                m_pCurrent = m_pCurrent->m_pParent;
            } while (m_pCurrent && !isRightChild);
            return *this;	
	    }

	    const_iterator& operator --() { (*this)--;} 

	
	    bool operator==(const const_iterator &other) const { return m_pCurrent == other.m_pCurrent;}
	    bool operator!=(const const_iterator &other) const { return m_pCurrent != other.m_pCurrent;}
    private:
	    const AVLTreeNode<t> *m_pCurrent;
	    AVLTree<t>     &m_Tree;
    };
	
	//friend class iterator<t>;
	//friend class const_iterator<t>;
};




// ================ end of declarations =============================

// ================ start of AVLTree<t> definitions =================

template <class t>
typename AVLTree<t>::iterator AVLTree<t>::begin()
{ 
	    if (!m_pRoot) return iterator(NULL, *this);
	    
	    AVLTreeNode<t>* node = m_pRoot;
	    while (node->m_pLeft) node = node->m_pLeft;
        return iterator(node, *this);
}
	
template <class t>
typename AVLTree<t>::iterator AVLTree<t>::end() 
{ 
    return iterator(NULL, *this); 
}


template <class t>
void AVLTree<t>::print(AVLTreeNode<t> *root, int depth) const
{
	if (! root)	return;
	if (root->m_pLeft) print(root->m_pLeft, depth+1);
	for (int a = depth; a; a--) std::cout << " ";
	std::cout << root->m_Data << std::endl;
	if (root->m_pRight) print(root->m_pRight, depth+1);
}

template <class t>
void AVLTree<t>::sort( t* dest, int &dest_idx, AVLTreeNode<t> *root) const
{
    if (! root) return;
	if (root->m_pLeft) asSortedArray(dest, dest_idx, root->m_pLeft);
	dest[dest_idx++] = root->m_Data;
	if (root->m_pRight) asSortedArray(dest, dest_idx, root->m_pRight);
}

template <class t>
void AVLTree<t>::remove( const t &item)
{
    num_items--;
    AVLTreeNode<t> *parent;
    AVLTreeNode<t> *node = findPos(item, parent);
    assert(node && "Item not found");

    /** BASIC idea: find the node closest in-order to the one to be removed,
        (i.e. its in-order predecessor or successor). Then remove the actual 
        node by overwriting its data with that of the closest one. This does
        not violate the BST semantics (apart from the fact, that the data of
        the closest one now exists twice in the tree. Thus, then remove the
        original node of the closest one, which will either be a leaf, or a
        node with just one child. Finally, update the node depth values 
        starting from the original parent of the removed closest node.        

      */
    
    int leftDepth = node->m_pLeft ? 1 + node->m_pLeft->m_dwDepth : 0;
    int rightDepth = node->m_pRight ? 1 + node->m_pRight->m_dwDepth : 0;
    
    if (leftDepth + rightDepth > 0)
    {
        AVLTreeNode<t> *replacement;
        if (leftDepth >= rightDepth)
        {
            replacement = node->m_pLeft; //in-order predecessor
            assert(replacement);
            while (replacement->m_pRight) 
                replacement = replacement->m_pRight;
        } else
        {
            replacement = node->m_pRight; //in-order successor
            assert(replacement);
            while (replacement->m_pLeft) 
                replacement = replacement->m_pLeft;
        }
        
        node->m_Data = replacement->m_Data;
        node = replacement; //so that "node" is still the tree node that needs to be deleted
    }    
    //left to do: unreference and delete 'node'
    assert( ! node->m_pLeft || ! node->m_pRight);

    AVLTreeNode<t>* child;
    if      (node->m_pLeft)  child = node->m_pLeft;
    else if (node->m_pRight) child = node->m_pRight;
    else                     child = NULL;
    
    if (child) child->m_pParent = node->m_pParent;
    if (node->m_pParent)
    {
        if (node->m_pParent->m_pLeft == node) node->m_pParent->m_pLeft = child;
        if (node->m_pParent->m_pRight== node) node->m_pParent->m_pRight= child;
    } 
    else m_pRoot = child;

    parent = node->m_pParent; 
    delete node;
    if (parent)
        updateDepth(parent);
        
    #warning extensive debug checks
    if (m_pRoot)
        m_pRoot->check();
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
	
	updateDepth( pPos);//, NULL, NULL);
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
    num_items++;
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
void AVLTree<t>::updateDepth( AVLTreeNode<t> *pNode)
{
	int leftDepth = pNode->m_pLeft ? 1 + pNode->m_pLeft->m_dwDepth : 0;
	int rightDepth = pNode->m_pRight ? 1 + pNode->m_pRight->m_dwDepth : 0;
	int newDepth = leftDepth > rightDepth ? leftDepth : rightDepth;
	if ((newDepth != pNode->m_dwDepth) || (abs(leftDepth - rightDepth) > 1))
	{
		pNode->m_dwDepth = newDepth;
		if (abs(leftDepth - rightDepth) > 1)
		{
		    // This difference in sub-tree height violates the AVL criterion.
		    // To re-establish it, the deeper subtree needs to be rebalanced.
		    // Thus, we need to find the predecessors on that subtree.
		
		    AVLTreeNode<t>* pred1 = leftDepth >= rightDepth ? pNode->m_pLeft : pNode->m_pRight;
		    assert(pred1);
		    
	        leftDepth = pred1->m_pLeft ? 1 + pred1->m_pLeft->m_dwDepth : 0;
	        rightDepth = pred1->m_pRight ? 1 + pred1->m_pRight->m_dwDepth : 0;
		    assert( leftDepth + rightDepth > 0);
		    
		    if ( abs(leftDepth-rightDepth) > 0)
		    {
		        AVLTreeNode<t>* pred2 = leftDepth >= rightDepth ? pred1->m_pLeft : pred1->m_pRight;
		        pNode = rearrange( pNode, pred1, pred2);
	        } else
	            pNode = (pNode->m_pLeft == pred1) ? rearrangeRotateRight(pNode) : rearrangeRotateLeft(pNode);
			
			#warning extensive debug checks
			pNode->check();
		}
		
		if (pNode->m_pParent)
			updateDepth( pNode->m_pParent);
	}
}

template <class t>
AVLTreeNode<t>* AVLTree<t>::rearrangeRotateLeft( AVLTreeNode<t>* pNode)
{
/**             pNode                           succ2
 * before:     c0   succ2       after:       pNode  c2
 *                 c1   c2                  c0   c1
 *                                       
*/
    AVLTreeNode<t>* succ2 = pNode->m_pRight;
    assert( succ2 );
    if (m_pRoot == pNode)
    {
        assert( ! pNode->m_pParent);
        m_pRoot = succ2;
    } 
    else
    {
        bool isLeftChild = pNode->m_pParent->m_pLeft == pNode;
        if (isLeftChild) pNode->m_pParent->m_pLeft = succ2;
        else             pNode->m_pParent->m_pRight= succ2;
    }
    succ2->m_pParent = pNode->m_pParent;

    AVLTreeNode<t>* c1 = succ2->m_pLeft;
    
    succ2->m_pLeft = pNode;
    pNode->m_pParent = succ2;
    
    pNode->m_pRight = c1;
    if (c1) c1->m_pParent = pNode;

    int nLeft = pNode->m_pLeft ? pNode->m_pLeft-> m_dwDepth + 1 : 0;
    int nRight= pNode->m_pRight? pNode->m_pRight->m_dwDepth + 1 : 0;
    pNode->m_dwDepth = nLeft > nRight ? nLeft : nRight;
    
    nLeft = succ2->m_pLeft ? succ2->m_pLeft-> m_dwDepth + 1 : 0;
    nRight= succ2->m_pRight? succ2->m_pRight->m_dwDepth + 1 : 0;
    succ2->m_dwDepth = nLeft > nRight ? nLeft : nRight;
    
    return succ2; 
}

template <class t>
AVLTreeNode<t>* AVLTree<t>::rearrangeRotateRight( AVLTreeNode<t>* pNode)
{
/**             pNode                     succ1
 * before:  succ1   c2       after:      c0   pNode  
 *         c0   c1                           c1   c2
*/
    AVLTreeNode<t>* succ1 = pNode->m_pLeft;
    assert( succ1);
    
    if (m_pRoot == pNode)
    {
        assert( ! pNode->m_pParent);
        m_pRoot = succ1;
    } 
    else
    {
        bool isLeftChild = pNode->m_pParent->m_pLeft == pNode;
        if (isLeftChild) pNode->m_pParent->m_pLeft = succ1;
        else             pNode->m_pParent->m_pRight= succ1;
    }
    succ1->m_pParent = pNode->m_pParent;

    AVLTreeNode<t>* c1 = succ1->m_pRight;
    
    succ1->m_pRight = pNode;
    pNode->m_pParent = succ1;
    
    pNode->m_pLeft = c1;
    if (c1) c1->m_pParent = pNode;
    
    int nLeft = pNode->m_pLeft ? pNode->m_pLeft-> m_dwDepth + 1 : 0;
    int nRight= pNode->m_pRight? pNode->m_pRight->m_dwDepth + 1 : 0;
    pNode->m_dwDepth = nLeft > nRight ? nLeft : nRight;
    
    nLeft = succ1->m_pLeft ? succ1->m_pLeft-> m_dwDepth + 1 : 0;
    nRight= succ1->m_pRight? succ1->m_pRight->m_dwDepth + 1 : 0;
    succ1->m_dwDepth = nLeft > nRight ? nLeft : nRight;
    
    return succ1;
}

// rearranges the tree section containing pNode, pred1, pred2 and their children by
// - sorting the three nodes
// - sorting the four child nodes
// - using the sorted value to re-balance the sub-tree to the following layout:
//              pNodes[1]
//      pNodes[0]       pNodes[2]
//   pCh[0]   pCh[1]  pCh[2]  pCh[3]

template <class t>
AVLTreeNode<t>* AVLTree<t>::rearrange( AVLTreeNode<t>* pNode, AVLTreeNode<t>* pred1, AVLTreeNode<t>* pred2)
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
	
	pNodes[1]->m_dwDepth = 1 + (pNodes[0]->m_dwDepth > pNodes[2]->m_dwDepth ? pNodes[0]->m_dwDepth : pNodes[2]->m_dwDepth);
	return pNodes[1];

}


template <class t>
bool AVLTree<t>::contains( const t &item) const
{
	return findPos(item); // if the result of findPos points to an existing point, the tree contains the item
}

//=======================Start of code for ALVTreeNode =========================
template <class t> 
void AVLTreeNode<t>::check() //check consistency
{
	if (m_pLeft)
	{
		assert( m_pLeft->m_pParent = this);
		assert( m_pLeft->m_Data < m_Data );
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
