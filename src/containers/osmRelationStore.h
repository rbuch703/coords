
#ifndef OSM_RELATION_STORE_H
#define OSM_RELATION_STORE_H

#include <string>
#include "osm/osmTypes.h"

class RelationStore {
public:
    RelationStore(const char* indexFileName, const char* dataFileName);
    RelationStore(std::string baseName);
    OsmRelation operator[](uint64_t relationId) const;    
    bool exists(uint64_t relationId) const;
    uint64_t getMaxNumRelations() const;
private:
    class RelationIterator;
public:
    
    RelationIterator begin();
    RelationIterator end();

private:
    class RelationIterator {
    public:
        RelationIterator( RelationStore &host, uint64_t pos);        
        RelationIterator& operator++();
        OsmRelation operator *();        
        bool operator !=( RelationIterator &other) const;    
    private:
        void advanceToNextRelation();    
        RelationStore &host;
        uint64_t pos;
    };

    mmap_t mapRelationIndex;
    mmap_t mapRelationData;

};


#endif

