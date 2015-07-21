
#include <list>
#include <vector>
#include <iostream>

#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h> // for rand()

#include "simplify.h"

using std::list;
using std::vector;

static double squaredDistanceToLine(const OsmGeoPosition &A, const OsmGeoPosition &B, const OsmGeoPosition &P)
{
    assert( B!= A);
    double num = (B.lat - (double)A.lat)*(A.lng - (double)P.lng) - 
                 (B.lng - (double)A.lng)*(A.lat - (double)P.lat);
    num = fabs(num);
    double dx = B.lat - (double)A.lat;
    double dy = B.lng - (double)A.lng;
    double denom_sq = dx*dx+dy*dy;
    assert(denom_sq != 0);
    return (num*num) / denom_sq;
}


/* This method simplifies the line segment between (and including) vertices[firstPos] and 
 * vertices[lastPos] based on the Douglas-Peucker algorithm, and moves all vertices that are
 * kept together, so that they start at 'moveToPos' and that there is no hole between any two
 * kept vertices. The only exception is that - for algorithmic reasons - this method does *NOT*
 * move vertices[lastPos] even though it is guaranteed to be always be the final vertex of the 
 * result. Instead, it returns the position at which vertices[lastPos] has to be moved manually
 * to complete the line segment */
static uint64_t simplifySection(vector<OsmGeoPosition> &vertices, uint64_t firstPos, uint64_t lastPos, uint64_t moveToPos, double allowedDeviation)
{
    //std::cout << "simplifying section " << firstPos << " -> " << lastPos 
    //          << " [" << moveToPos << "] " << std::endl;
    assert (firstPos <= lastPos && "logic error");
    
    OsmGeoPosition A = vertices[firstPos];
    OsmGeoPosition B = vertices[lastPos];
    
    /* section consists of only one vertex, which is also shared with the next segment
     * --> do nothing */ 
    if (firstPos == lastPos)
    {
        return moveToPos;
    }
    
    /* there are no vertices between firstPos and lastPos
     * --> move the 'firstPos' vertex, keep that at 'lastPos' in place (as it is shared with
     *     the next segment*/
    if (firstPos + 1 == lastPos)  
    {
        vertices[moveToPos] = vertices[firstPos];
        return moveToPos+1;
    }
    
    double max_dist_sq = -1;
    uint64_t maxDistPos = firstPos+1;
    //list<Vertex>::iterator it  = segment_first;
    // make sure that 'it' starts from segment_first+1; segment_first must never be deleted
    //for (it++; it != segment_last; it++)
    for (uint64_t pos = firstPos+1; pos < lastPos; pos++)
    {
        double dist_sq;
        if (A == B)
        {
            double dx = vertices[pos].lat - (double)A.lat;
            double dy = vertices[pos].lng - (double)A.lng;
            dist_sq = dx*dx+dy*dy;
        } else
            dist_sq = squaredDistanceToLine(A, B, vertices[pos]);
        if (dist_sq > max_dist_sq) 
        { 
            maxDistPos = pos;
            max_dist_sq = dist_sq;
        }
    }
    //std::cout<< "    max distance: " << max_dist_sq << std::endl;
    if (max_dist_sq < allowedDeviation*allowedDeviation) 
    {
        /* no point further than 'allowedDeviation' from line A-B 
         * --> can delete/overwrite all points in-between */
        assert( moveToPos <= firstPos);
        //int64_t offset = moveToPos - (int64_t)firstPos;
        
        vertices[moveToPos] = A;
        //vertices[moveToPos+1] = B;
        return moveToPos+1;
        //for (uint64_t i = firstPos; i < lastPos; i++)
        //    vertices[i+offset] = vertices[i];

        //return lastPos + offset;
    } else  //use point with maximum deviation as additional necessary point in the simplified polygon, recurse
    {
        assert(maxDistPos > firstPos && maxDistPos < lastPos);
        uint64_t newMovePos = simplifySection(vertices, firstPos, maxDistPos, 
                                              moveToPos, allowedDeviation);
        newMovePos = simplifySection(vertices, maxDistPos, lastPos, 
                                     newMovePos, allowedDeviation);
        
        return newMovePos;
    }
}

void simplifyLine(vector<OsmGeoPosition> &vertices, double allowedDeviation)
{
    uint64_t lastPos = simplifySection(vertices, 0, vertices.size()-1, 0, allowedDeviation);

    vertices[lastPos] = vertices.back();
    /*note: vector::resize never reallocated the underlying array, so resize() does not incur
     *      the performance overhead of a memory allocation*/
    vertices.resize(lastPos+1); 
}

