
#include "osmConsumerIdRemapper.h"
#include <iostream>
#include <assert.h>

using namespace std;

OsmConsumerIdRemapper::OsmConsumerIdRemapper(std::string destinationDirectory, OsmBaseConsumer *innerConsumer): 
    innerConsumer(innerConsumer)
{
    MUST(destinationDirectory.length() > 0, "empty destination given");
    if (destinationDirectory.back() != '/' && destinationDirectory.back() != '\\')
        destinationDirectory += "/";

    nodeMap    = new SerializableMap<uint64_t, uint64_t, 10000000>((destinationDirectory + "mapNodes.idx").c_str()    );
    wayMap     = new SerializableMap<uint64_t, uint64_t, 10000000>((destinationDirectory + "mapWays.idx").c_str()     ); 
    relationMap= new SerializableMap<uint64_t, uint64_t, 10000000>((destinationDirectory + "mapRelations.idx").c_str());

    nodeIdsRemapped = nodeMap->getHighestValue();
    wayIdsRemapped = wayMap->getHighestValue();
    relationIdsRemapped = relationMap->getHighestValue();
           
}

OsmConsumerIdRemapper::~OsmConsumerIdRemapper ()
{
    cout << "relocation tables have " 
         << nodeIdsRemapped     << "/" 
         << wayIdsRemapped      << "/"
         << relationIdsRemapped << " entries." << endl;
         
     delete nodeMap;
     delete wayMap;
     delete relationMap;
}



void OsmConsumerIdRemapper::consumeNode( OsmNode &node)
{
    if (!nodeMap->count(node.id))
        nodeMap->insert( std::make_pair( node.id, ++nodeIdsRemapped));

    node.id = (*nodeMap)[node.id];

    innerConsumer->consumeNode(node);
}

void OsmConsumerIdRemapper::consumeWay( OsmWay  &way) 
{ 
    if (!wayMap->count(way.id))
        wayMap->insert( std::make_pair( way.id, ++wayIdsRemapped));
        
    way.id = (*wayMap)[way.id];
    
    for (OsmGeoPosition &pos : way.refs)
    {
        if (!nodeMap->count(pos.id))
            nodeMap->insert( std::make_pair(pos.id, ++nodeIdsRemapped));
        pos.id = (*nodeMap)[pos.id];
    }

    //cout << way << endl;
    innerConsumer->consumeWay(way);
}

void OsmConsumerIdRemapper::consumeRelation( OsmRelation &relation) 
{ 

    if (!relationMap->count(relation.id))
        relationMap->insert( std::make_pair( relation.id, ++relationIdsRemapped));
    
    relation.id = (*relationMap)[relation.id];


    for (OsmRelationMember &mbr : relation.members)
        switch (mbr.type)
        {
            case NODE: 
                if (! nodeMap->count(mbr.ref))
                    nodeMap->insert( std::make_pair(mbr.ref, ++nodeIdsRemapped));
                    
                mbr.ref = (*nodeMap)[mbr.ref];
                break;
            case WAY: 
                if (!wayMap->count(mbr.ref))
                    wayMap->insert( std::make_pair( mbr.ref, ++wayIdsRemapped));
                
                mbr.ref = (*wayMap)[mbr.ref];
                break;
            case RELATION: 
                if (!relationMap->count(mbr.ref))
                    relationMap->insert( std::make_pair( mbr.ref, ++relationIdsRemapped));
                
                mbr.ref = (*relationMap)[mbr.ref];
                break;
            default: assert(false && "invalid member type"); break;
        }

    //consume only after remapping!
    innerConsumer->consumeRelation(relation);

};


