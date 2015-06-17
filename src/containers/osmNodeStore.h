
#ifndef OSM_NODE_STORE_H
#define OSM_NODE_STORE_H

#include <stdint.h>
#include <string>
#include "osm/osmTypes.h"
#include "misc/mem_map.h"

class OsmNodeStore {

public:
    OsmNodeStore(const char* indexFileName, const char* dataFileName, bool optimizeForStreaming = false);
    OsmNodeStore(const std::string &baseName, bool optimizeForStreaming = false);
    OsmNode operator[](uint64_t nodeId);
    bool exists(uint64_t nodeId) const;
    //void syncRange(uint64_t lowWayId, uint64_t highWayId) const ;

private:
    class NodeIterator;

public:
    
    NodeIterator begin();
    NodeIterator end();

    /** \returns:
     *       an upper bound for the number of nodes, which is also an upper bound for the
     *       highest nodeId present in the data.  
    */
    uint64_t getMaxNumNodes() const;

private:
    class NodeIterator {

    public:
        NodeIterator( OsmNodeStore &host, uint64_t pos);
        NodeIterator& operator++();
        OsmNode operator *();
        bool operator !=( NodeIterator &other) const;
    
    private:
        void advanceToNextNode();
        OsmNodeStore &host;
        uint64_t pos;
    };

    mmap_t mapNodeIndex;
    mmap_t mapNodeData;
};


#endif

