
#ifndef QUADTREE_H
#define QUADTREE_H

#include <list>
#include "geometric_types.h"
#include "vertexchain.h"

void moveIntersectionsToIntegerCoordinates3(list<LineSegment> &segments);
list<VertexChain> getPolygons(map<Vertex,set<Vertex> > &graph); //only required for geo_unit_tests

list<VertexChain> toSimplePolygons(const list<Vertex> &polygon);

#endif

