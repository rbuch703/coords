
#include <iostream>
#include <map>


#include <string>

#include "osm/osmTypes.h"
#include "containers/chunkedFile.h"


using namespace std;

int main()
{
    uint64_t numLocal    = 0;
    uint64_t numGlobal   = 0;
    map<string, uint64_t> hist;
    cerr << "parsing nodes" << endl;

    uint64_t numRead = 0;
    for (const Chunk &chunk : ChunkedFile("intermediate/nodes.data"))
    {
        OsmNode node( chunk.getDataPtr());
        if (++numRead % 5000000 == 0)
            cerr << (numRead/1000000) << "M nodes read, " << (hist.size() / 1000000) << "M strings" << endl;
        map<string, string> tags(node.tags.begin(), node.tags.end());

        /*
        if (tags.count("place"))
        {
            numPlaces += 1;
            continue;
        } */
        
        if (tags.count("addr:housenumber") || 
            tags.count("power") ||
            tags.count("highway") || tags.count("railway") || tags.count("barrier") ||
            tags.count("natural") || tags.count("waterway") ||
            tags.count("amenity") || tags.count("shop") || 
            tags.count("tourism") || tags.count("leisure") || tags.count("historic") ||
            tags.count("man_made") ||
            tags.count("entrance") ||
            tags.count("building") ||
            tags.count("emergency") ||
            tags.count("ele");
            )
        {
            numLocal += 1;
            continue;
        }
        
        if (tags.count("place"))
        {
            string type = tags.at("place");
            if (type == "locality" || type == "village" || type == "hamlet" || 
                type == "suburb"   || type == "county"  || type == "town" ||
                type == "neighbourhood" || type == "farm"  || type == "plot" ||
                type == "isolated_dwelling" || type == "islet" || type == "allotments")
            {
                numLocal += 1;
                continue;
            }
            
            // place whose names may be displayed at a global scale
            if (type == "city" || type == "state" || type == "country" ||
                type == "province" || type == "island" )
            {
                numGlobal += 1;
                continue;
            }
            
            //unspecified and explicitly deprecated --> just ignore them
            if (type == "region")
                continue;
        }
        /*
        if (tags.count("ele"))
        {
            numElevation += 1;
            continue;
        }*/
        
        /*if (!tags.count("place"))
            continue;
        
        auto type = tags.at("place");
        if (!hist.count(type)) hist.insert(make_pair(type, 0));
        hist[type] += 1;*/
        
        
        for (const pair<string, string> kv : node.tags)
        {
            if (! hist.count(kv.first))   hist.insert(make_pair( kv.first, 0));
            hist[kv.first] += 1;
            
            //if (! hist.count(kv.second))  hist.insert(make_pair( kv.second, 0));
            //hist[kv.second] += 1;
        }
        
    }
    cerr << "local features: " << (numLocal/1000000) << "M" << endl;
    cerr << "global features: " << (numGlobal/1000) << "k" << endl;
    for (pair<string, uint64_t> kv : hist)
    {
	//bytes saved through symbolic encoding: 
	// #occurrences * (string length + 1 byte zero termination - 1byte symbolic encoding)
	// = #occurrences * string length
        cout << kv.second << "\t" << kv.first << endl;
    }

    return EXIT_SUCCESS;
}
