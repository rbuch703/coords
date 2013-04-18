
#ifndef QUADTREE_H
#define QUADTREE_H

#include <list>
#include "geometric_types.h"
#include "vertexchain.h"

void moveIntersectionsToIntegerCoordinates3(list<LineSegment> &segments);
list<VertexChain> getPolygons(map<Vertex,vector<Vertex> > &graph);                    // only exported 
map<Vertex,vector<Vertex> > getConnectivityGraph(const list<LineSegment> &segments ); // for geo_unit_tests

list<VertexChain> toSimplePolygons(VertexChain &polygon/*, bool canDestroyInput = false*/);

#endif

