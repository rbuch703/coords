
#ifndef QUADTREE_H
#define QUADTREE_H

#include <list>
#include "geometric_types.h"

void moveIntersectionsToIntegerCoordinates3(list<LineSegment> &segments);
 
class QuadTreeNode
{
public:
    QuadTreeNode(AABoundingBox _box);
    ~QuadTreeNode();
    void insertIntoHierarchy( LineSegment edge1, list<LineSegment> &createdSegments);
    void printHierarchy(int depth = 0);
    void exportSegments(list<LineSegment> &segments_out);
private:
    bool addToQuadrant(LineSegment seg);    
    bool intersectedByRecursive( LineSegment edge1, list<LineSegment> &createdSegments);
    int  coversQuadrants( LineSegment edge, bool &tl, bool &tr, bool &bl, bool &br) const;
    void subdivide();
    static const unsigned int SUBDIVISION_THRESHOLD = 20; //determined experimentally
    
    QuadTreeNode *tl, *tr, *bl, *br;
    list<LineSegment> segments;
    AABoundingBox     box;
    BigInt            mid_x, mid_y;
};


#endif

