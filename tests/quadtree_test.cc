
#include "../geometric_types.h"
#include "../vertexchain.h"
#include "../quadtree.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[1;30m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

BigInt getRandom()
{
    BigInt a = 0;
    int i = 8;
    do
    {
        a = (a << 16)  | (rand() % 0xFFFF);
    } while (--i);
    
    return rand() % 2 ? a : -a;
    
}

int main(int, char** )
{
    VertexChain p; 

    //TODO: perform multiple polygon simplifications for medium-sized polygons (~500 vertices) under changing random seeds
    srand(24);
    for (int i = 0; i < 200; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

    list<LineSegment> segs;
    const vector<Vertex> &verts = p.data();
    
    vector<Vertex>::const_iterator it2 = verts.begin();
    for (vector<Vertex>::const_iterator it = it2++; it2 != verts.end(); it++, it2++)
        segs.push_back( LineSegment(*it, *it2) );
    
    moveIntersectionsToIntegerCoordinates3(segs);
    TEST( intersectionsOnlyShareEndpoint(segs) );
    map<Vertex,set<Vertex> > graph = getConnectivityGraph(segs);
    
    int numEdges = 0;
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
/*           for (set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
                cout << "#\t" << it->first << "," << *it2 << endl;
                */
        numEdges += it->second.size();
    }
    std::cout << "Connectivity graph consists of " << graph.size() << " vertices and " << numEdges << " edges." << std::endl;

    list<VertexChain> polygons = getPolygons(graph);

    std::cout << "generated " << polygons.size() << " polygons" << endl;
    
}




