
#ifndef OSM_NODE_STORE_H
#define OSM_NODE_STORE_H

//#include <string>
#include "osm/osmTypes.h"

class NodeStore {
public:
    NodeStore(const char* indexFileName, const char* dataFileName);
    NodeStore(std::string baseName);
    OsmNode operator[](uint64_t relationId) const;    
    bool exists(uint64_t relationId) const;
    uint64_t getMaxNumNodes() const;
private:
    class NodeIterator;
public:
    
    NodeIterator begin();
    NodeIterator end();

private:
    class NodeIterator {
    public:
        NodeIterator( NodeStore &host, uint64_t pos);        
        NodeIterator& operator++();
        OsmNode operator *();        
        bool operator !=( NodeIterator &other) const;    
    private:
        void advanceToNextNode();
        NodeStore &host;
        uint64_t pos;
    };

    mmap_t mapIndex;
    mmap_t mapData;

};


#endif
