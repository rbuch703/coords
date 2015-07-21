
#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include <string.h> //for memset()

/*  The template class RadixTree implements a specialized dictionary that maps 
    strings to values of arbitrary type (specified at compile time as the 
    template argument). In additional, it allows for the specialized call
    containsPrefixOf(x), which returns whether any key stored in the radix tree
    is a prefix of 'x'. This is done in O(len(x)) time, irrespective of how many
    entrys the radix tree has.
    
    Internally, the radix tree consists of nodes that each have up to 256 children,
    and optionally have a value. Each node but the root implicitly has a "char" value,
    given by the position of its pointer in the child list of its parent (e.g. if a
    node is the 65th child of its parent, its value is '65', or - equivalently - the
    letter 'A' in ASCII. Based on this, each node also implicitly has a key, given
    by the sequence of values of all its parents, from the root node down to itself.
    
   
*/

template<typename t> class RadixTreeNode;   //forward

template<typename t>
class RadixTree {

public: 
    ~RadixTree();
    static void deleteChildren(RadixTreeNode<t> &node);
    bool containsPrefixOf(const char* keyName) const;
    void insert(const char* keyName, const t& value);
    const t* at(const char* keyName) const;
//    void traverse(RadixTreeNode<t> *pos, std::vector<char> &path, int depth = 0);

private:
    RadixTreeNode<t> root;  
};

template<typename t>
class RadixTreeNode {
public:
    RadixTreeNode(): value(nullptr) {
        memset(children, 0, 256*sizeof(RadixTreeNode*));
        
    }

    RadixTreeNode* children[256];
    t*    value;
};

template<typename t>
RadixTree<t>::~RadixTree() {
    deleteChildren(root);
    if (root.value)
        delete root.value;
}


template<typename t>
void RadixTree<t>::deleteChildren(RadixTreeNode<t> &node)
{
    
    for (int i = 0; i < 256; i++)
    {
        if (! node.children[i])
            continue;
            
        deleteChildren(*node.children[i]);
        delete node.children[i];
        node.children[i] = nullptr;
    }
    delete node.value;
}

template<typename t>
bool RadixTree<t>::containsPrefixOf(const char* keyName) const
{
    const unsigned char* key = (const unsigned char*)keyName;

    const RadixTreeNode<t> *pos = &root;
        
    while (*key)
    {
        if (pos->value)
            return true;

        RadixTreeNode<t>* const &nextPos = pos->children[(int)*key];
        
        if (!nextPos)
            return false;
            

        pos = nextPos;
        key++;
    }
    
    return pos->value;    
}

template<typename t>
void RadixTree<t>::insert(const char* keyName, const t& value)
{
    const unsigned char* key = (const unsigned char*)keyName;

    RadixTreeNode<t> *pos = &root;
    while (*key)
    {
        RadixTreeNode<t>* &nextPos = pos->children[(int)*key];
        if (!nextPos)
        {
            nextPos = new RadixTreeNode<t>();
        }
        pos = nextPos;
        key++;
    }
    if (!pos->value)
        pos->value = new t();
    
    *(pos->value) = value;
}

/*
template<typename t>
void RadixTree<t>::traverse(RadixTreeNode<t> *pos, std::vector<char> &path, int depth)
{
    for (int i = 0; i < 256; i++)
        if (pos->children[i])
    {
        path.push_back(i);
        if (pos->children[i]->value)
        {
            for (char ch : path)
                std::cout << ch;
            std::cout << " -> " << (*pos->children[i]->value) << std::endl;
        }
        
        traverse(pos->children[i], path, depth+1);
        path.pop_back();
    }  
}*/

template<typename t>
const t* RadixTree<t>::at(const char* keyName) const
{
    const unsigned char* key = (const unsigned char*)keyName;
    const RadixTreeNode<t> *pos = &root;
    while (*key)
    {
        if (!pos->children[(int)*key])
            return nullptr;
            
        pos = pos->children[(int)*key];
        key++;
    }
    
    if (!pos->value)
        return nullptr;
    return pos->value;
}


#endif

