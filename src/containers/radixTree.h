
#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include <vector>
#include <string.h>
#include <iostream>

using namespace std;

template<typename t>
class RadixTreeNode {
public:
    RadixTreeNode(): value(NULL) {
        memset(children, 0, 256*sizeof(RadixTreeNode*));
        
    }

    RadixTreeNode* children[256];
    t*    value;
};

template<typename t>
class RadixTree {

public: 

~RadixTree() {
    deleteChildren(root);
    if (root.value)
        delete root.value;
}

static void deleteChildren(RadixTreeNode<t> &node)
{
    
    for (int i = 0; i < 256; i++)
    {
        if (! node.children[i])
            continue;
            
        deleteChildren(*node.children[i]);
        delete node.children[i];
        node.children[i] = NULL;
    }
    delete node.value;
}

bool containsPrefixOf(const char* keyName) const
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

void insert(const char* keyName, const t& value)
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

void traverse(RadixTreeNode<t> *pos, vector<char> &path, int depth = 0)
{
    for (int i = 0; i < 256; i++)
        if (pos->children[i])
    {
        path.push_back(i);
        if (pos->children[i]->value)
        {
            for (char ch : path)
                cout << ch;
            cout << " -> " << (*pos->children[i]->value) << endl;
        }
        
        traverse(pos->children[i], path, depth+1);
        path.pop_back();
    }
    
}

const t* at(const char* keyName) const
{
    const unsigned char* key = (const unsigned char*)keyName;
    const RadixTreeNode<t> *pos = &root;
    while (*key)
    {
        if (!pos->children[(int)*key])
            return NULL;
            
        pos = pos->children[(int)*key];
        key++;
    }
    
    if (!pos->value)
        return NULL;
    return pos->value;
}

    RadixTreeNode<t> root;  
};

#endif

