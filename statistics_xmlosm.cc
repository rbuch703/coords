
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <iostream>

#include <stdint.h>
#include <string.h> //for memset

#include "osmxmlparser.h"


using namespace std;

class BitArray
{
public:
    BitArray(uint64_t nElements): m_nElements(nElements)
    {
        uint64_t nBytes = (nElements+7) / 8;
        m_data = new uint8_t[nBytes];
        memset( m_data, 0, nBytes* sizeof(uint8_t) );
    }
    
    ~BitArray() { delete [] m_data;}
    
    bool operator()(uint64_t idx) { return m_data[idx /8] & ( 1 << (idx%8));}
    void set( uint64_t idx) { m_data[idx/8] |= ( 1 << (idx %8));}
    void unset( uint64_t idx) { m_data[idx/8] &= ~( 1 << (idx %8));}

private:
    uint64_t m_nElements;
    uint8_t *m_data;
};


class OsmXmlStatisticParser: public OsmXmlParser
{
public:
    OsmXmlStatisticParser(FILE * f): OsmXmlParser(f) 
    {
        nNodes = nWays = nRelations = 0;
        memset( vBits, 0, NUM_BIT_ARRAYS * sizeof(BitArray*));
    };
    virtual ~OsmXmlStatisticParser() {};
protected:

    virtual void beforeParsingNodes () { };  
    /*
    virtual void afterParsingNodes () { };
    
    virtual void beforeParsingWays () { };
    
    virtual void afterParsingWays () { };   

    virtual void beforeParsingRelations () {
    }; 
    */
    virtual void afterParsingRelations () {
        cout << nNodes << " nodes, " << nWays << " ways, " << nRelations << " relations." << endl;
        uint32_t numRefNodes = 0;
        for (uint32_t i = 0; i < NUM_BIT_ARRAYS; i++)
        {
            if (! vBits[i]) continue;
            
            for (uint32_t j = 0; j < BITS_PER_ARRAY; j++)
                if ( (*vBits[i])(j)) numRefNodes++;
            
            delete vBits[i];
        }
        
        cout << "nodes not referenced by ways: " << numRefNodes << endl;
    };


    virtual void completedNode( OSMNode &node) 
    {
        nNodes++;

        uint32_t idx = node.id / BITS_PER_ARRAY;
        if (! vBits[idx]) vBits[idx] = new BitArray(BITS_PER_ARRAY);
        vBits[idx]->set( node.id % BITS_PER_ARRAY );
    }
    
    virtual void completedWay ( OSMWay  &way) 
    {
        nWays++;

        for (list<uint64_t>::const_iterator id = way.refs.begin(); id != way.refs.end(); id++)
        {
            uint32_t idx = *id / BITS_PER_ARRAY;
            if (! vBits[idx]) vBits[idx] = new BitArray( BITS_PER_ARRAY );
            vBits[ idx ]->unset( *id % BITS_PER_ARRAY );
        }
    }
    
    virtual void completedRelation( OSMRelation &/*relation*/) 
    {
        nRelations++;

    }
    
    virtual void doneParsingNodes () { cout << "===============================================" << endl;}
private:
    uint64_t nNodes, nWays, nRelations;
    static const uint32_t NUM_BIT_ARRAYS = 2000;
    static const uint32_t BITS_PER_ARRAY = 1000000;
    BitArray *vBits[NUM_BIT_ARRAYS];
};


int main(/*int argc, char** argv*/)
{

    //int in_fd = 0; //default to standard input
    //FILE *f = fdopen(in_fd, "rb");
    FILE *f = fopen("src/isle_of_man.osm", "rb");
    
    OsmXmlStatisticParser parser( f );
    parser.parse();
    fclose(f);
	return 0;
	
}


