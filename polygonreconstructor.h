
#ifndef POLYGONRECONSTRUCTOR_H
#define POLYGONRECONSTRUCTOR_H

#include "vertexchain.h"
#include <map>
#include <list>

class PolygonReconstructor
{
public:
    ~PolygonReconstructor() {clear();} 
    
    /** adds a PolygonSegment to the Reconstructor state. 
      * if this addition results in the generation of a new closed polygon, a pointer to said polygon is returned
      * otherwise, NULL is returned
      * Returns true 
    */
    PolygonSegment* add(const PolygonSegment &s);
    
    /** uses heuristics to create polygons from the PolygonSegments in openEndPoints, even if
        the segments do not connect directly to each other */
    void forceClosePolygons();
        
    void clear();
    
    void clearPolygonList() { res.clear();}
    const list<PolygonSegment>& getClosedPolygons() const {return res;}

private:
    map<Vertex, PolygonSegment*> openEndPoints;
    list<PolygonSegment> res;
    
};


#endif

