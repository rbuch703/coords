
#include "triangulatepolygon.h"

#include <boost/foreach.hpp>

/**
    Data Structures:
        - priority queue of events (new segment, end of segment, intersection), 
            - sorted by sweep direction (first by x-coordinate, then by y-coordinate)
            - ability to remove elements in O( log(n) ) (mostly intersections that did not materialize)
            
        - list of active edges (those polygon edges that intersect the current sweep line), 
            - sorted by current y-coordinate
            - ability to swap adjacent edges in O(1) (when they intersect)
            - ability to find edges in O(log(n)) (e.g. the active edge matching the current event)
            - ability to remove edges in O(log(n)) (when they no longer intersec the sweep line)
*/




