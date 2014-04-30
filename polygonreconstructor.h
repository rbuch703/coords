
#ifndef POLYGONRECONSTRUCTOR_H
#define POLYGONRECONSTRUCTOR_H

#include "vertexchain.h"
#include <map>
#include <list>

class PolygonReconstructor
{
public:
    ~PolygonReconstructor() {clear();} 
    
    /** adds a VertexChain to the Reconstructor state. 
      * if this addition results in the generation of a new closed polygon, a pointer to said polygon is returned
      * otherwise, NULL is returned
      * Returns true 
    */
    VertexChain* add(const VertexChain &s);
    
    /** uses heuristics to create polygons from the VertexChains in openEndPoints, even if
        the segments do not connect directly to each other */
    void forceClosePolygons();
        
    void clear();
    
    void clearPolygonList() { res.clear();}
    const list<VertexChain>& getClosedPolygons() const {return res;}
    int getNumOpenEndpoints() const { return openEndPoints.size();}
private:
    map<Vertex, VertexChain*> openEndPoints;
    list<VertexChain> res;
    
};


#endif

