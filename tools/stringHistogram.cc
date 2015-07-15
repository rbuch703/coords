
#include <iostream>
#include <map>


#include <string>

#include "osm/osmMappedTypes.h"
#include "containers/osmNodeStore.h"
#include "containers/osmWayStore.h"
#include "containers/osmRelationStore.h"


using namespace std;

int main()
{
    map<string, uint64_t> hist;
    LightweightWayStore wayStore("intermediate/ways");
    
    uint64_t numRead = 0;
    cerr << "parsing ways" << endl;
    
    for (const OsmLightweightWay way : wayStore)
    {
    
        if (++numRead % 1000000 == 0)
            cerr << (numRead/1000000) << "M ways read, " << (hist.size() / 1000000) << "M strings" << endl;

            
        for (const pair<string, string> kv : way.getTags())
        {
            if (! hist.count(kv.first))   hist.insert(make_pair( kv.first, 0));
            hist[kv.first] += 1;
            
            if (! hist.count(kv.second))  hist.insert(make_pair( kv.second, 0));
            hist[kv.second] += 1;
        }
    }
    
    cerr << "parsing nodes" << endl;

    numRead = 0;    
    NodeStore nodeStore("intermediate/nodes");
    for (const OsmNode node : nodeStore)
    {
        if (++numRead % 1000000 == 0)
            cerr << (numRead/1000000) << "M nodes read, " << (hist.size() / 1000000) << "M strings" << endl;
            
        for (const pair<string, string> kv : node.tags)
        {
            if (! hist.count(kv.first))   hist.insert(make_pair( kv.first, 0));
            hist[kv.first] += 1;
            
            if (! hist.count(kv.second))  hist.insert(make_pair( kv.second, 0));
            hist[kv.second] += 1;
        }
        
    }
 
    cerr << "parsing relations" << endl;

    numRead = 0;    
    RelationStore relationStore("intermediate/relations");
    for (const OsmRelation relation : relationStore)
    {
        if (++numRead % 1000000 == 0)
            cerr << (numRead/1000000) << "M nodes read, " << (hist.size() / 1000000) << "M strings" << endl;
            
        for (const pair<string, string> kv : relation.tags)
        {
            if (! hist.count(kv.first))   hist.insert(make_pair( kv.first, 0));
            hist[kv.first] += 1;
            
            if (! hist.count(kv.second))  hist.insert(make_pair( kv.second, 0));
            hist[kv.second] += 1;
        }
        
    }
 
    
    for (pair<string, uint64_t> kv : hist)
    {
	//bytes saved through symbolic encoding: 
	// #occurrences * (string length + 1 byte zero termination - 1byte symbolic encoding)
	// = #occurrences * string length
        cout << (kv.second * (kv.first.length())) << "\t" << kv.first << endl;
    }

    return EXIT_SUCCESS;
}
