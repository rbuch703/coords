
#include "geometric_types.h"


double Vertex::distanceToLine(const Vertex A, const Vertex B) const
{
    assert( B!= A);
    double num = fabs( ((double)B.x-A.x)*((double)A.y-y) - ((double)B.y-A.y)*((double)A.x-x));
    double denom = sqrt( ((double)B.x-A.x)*((double)B.x-A.x) + ((double)B.y-A.y)*((double)B.y-A.y));
    assert(denom != 0);
    return num / denom;
}

int64_t Vertex::pseudoDistanceToLine(const Vertex start, const Vertex end) const
{
    assert( end!= start);
    /** this computation should never overflow on OSM data, because its longitude (y-coordinate)
        needs only 31 bits to be stored */
        
    /* x-difference: 32bits plus sign bit; y-difference: 31bits plus sign bit
     * product of both: 31+32 = 63 bits plus sign bit*/
    return (end.x-start.x)*(start.y-y) - (end.y-start.y)*(start.x-x);
}


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}

/** =================================================*/

void PolygonSegment::append(const PolygonSegment &other, bool shareEndpoint)
{
    if (shareEndpoint) 
    {
        assert(back() == other.front());
        m_vertices.insert( m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
    } else
    {
        assert(back() != other.front()); //otherwise we would have two consecutive identical vertices
        cout << "concatenating line segments that are " 
             << sqrt( (back() -other.front()).squaredLength() )/100.0 << "m apart"<< endl;
        m_vertices.insert( m_vertices.end(),   other.m_vertices.begin(), other.m_vertices.end());
    }
    
    //std::cout << "Merged: " << res
    //return res;
}

bool PolygonSegment::simplifyArea(double allowedDeviation)
{
    assert( m_vertices.front() == m_vertices.back() && "Not a Polygon");
    int nVertices = m_vertices.size();
    /*
    int64_t min_lat = m_vertices.front().x;
    int64_t max_lat = min_lat;
    int64_t min_lon = m_vertices.front().y;
    int64_t max_lon = min_lon;

    for (std::list<Vertex>::const_iterator vertex = m_vertices.begin(); vertex != m_vertices.end(); vertex++, nVertices++)
    {
        if ( vertex->x < min_lat) min_lat = vertex->x;
        if ( vertex->y < min_lon) min_lon = vertex->y;

        if ( vertex->x > max_lat) max_lat = vertex->x;
        if ( vertex->y > max_lon) max_lon = vertex->y;

    }*/
    // since first and last vertex are identical, we need at least four vertices to define any area
    //if (nVertices < 4) return false;

    //uint64_t area = (max_lat - min_lat)*(max_lon - min_lon);
    //cout << "Area (Equatorial equivalent): " << area/(100000.0*100000.0) << "km²" << std::endl;
    
    // if the whole polygon (area) is smaller than our measure of allowed deviation
    /*if ( area < allowedDeviation*allowedDeviation) 
    {   return false;
    }*/
    //return true;
    //int mid_point = nVertices/2;
    //list<Vertex>::iterator it_mid = m_vertices.begin();
    //while (mid_point--) it_mid++;
    list<Vertex>::iterator last = m_vertices.end();
    last--;
    simplifySection( m_vertices.begin(), last, allowedDeviation);
    
    //list<Vertex>::iterator it_last = m_vertices.end();
    //it_last--;
    //simplifySection( it_mid, it_last, allowedDeviation);

    // Need three vertices to form an area; four since first and last are identical
    if (m_vertices.size() < 4) return false;

    cout << nVertices << " --> " << m_vertices.size() << " vertices"<< endl;
    return true;
}

void PolygonSegment::simplifyStroke(double allowedDeviation)
{
    list<Vertex>::iterator last = m_vertices.end();
    last--;
    simplifySection( m_vertices.begin(), last, allowedDeviation);
}

void PolygonSegment::simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation)
{
    list<Vertex>::iterator it_max;
    uint64_t max_dist = 0;
    Vertex A = *segment_first;
    Vertex B = *segment_last;
    
    list<Vertex>::iterator it  = segment_first;
    // make sure that 'it' starts from segment_first+1; segment_first must never be deleted
    for (it++; it != segment_last; it++)
    {
        uint64_t dist = (A == B) ? sqrt( (*it-A).squaredLength()) : it->distanceToLine(A, B);
        if (dist > max_dist) { it_max = it; max_dist = dist;}
    }
    if (max_dist < allowedDeviation) 
    {
        // no point further than 'allowedDeviation' from line A-B --> can delete all points in between
        segment_first++;
        // erase range includes first iterator but excludes second one
        m_vertices.erase( segment_first, segment_last);
    } else  //use point with maximum deviation as additional necessary point in the simplified polygon, recurse
    {
        simplifySection(segment_first, it_max, allowedDeviation);
        simplifySection(it_max, segment_last, allowedDeviation);
    }
}

/*void clipHorizontally( uint32_t height, list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out)
{
    bool onTop;

//TODO: if (front() != back())
}*/


std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg)
{
    for (list<Vertex>::const_iterator it = seg.vertices().begin(); it != seg.vertices().end(); it++)
        os << *it << endl;
    return os;
}

/*
bool intersect (const Vertex l1_start, const Vertex l1_end, const Vertex l2_start, const Vertex l2_end)
{
    Vertex dir1 = l1_end - l1_start;
    Vertex dir2 = l2_end - l2_start;
    assert( dir1 != Vertex(0,0) && dir2!= Vertex(0,0));
    
    if (dir1.parallelTo(dir2))
    {
        
    } else
    {
        
    }
}*/
/*
void findIntersections(list<Vertex> path, list<Intersection> intersections_out)
{
    uint32_t left, right, top , bottom;
    left = right = path.front().x;
    top = bottom = path.front().y;
    for (list<Vertex>::const_iterator vertex = path.begin(); vertex!= path.end(); vertex++)
    {
        if (vertex->x > right) right = vertex->x;
        if (vertex->x < left)  left = vertex->x;
        if (vertex->y < top)   top = vertex->y;
        if (vertex->y > bottom) bottom = vertex->y;
    }
    
    if (right - left < 2 && bottom-top < 2) //area too small for further subdivision, test each segment against every other segment
    {
        for (list<Vertex>::const_iterator it = path.begin(); it!= path.end(); it++)
        {
            Vertex l1_start = *it;
            list<Vertex>::const_iterator next = it;
            next++;
            Vertex l1_end = *next;
            for (list<Vertex>::const_iterator it2 = it, it2++; it2!= path.end(); it2++)
            {
                Vertex l2_start = *it2;
                next = it2;
                next++;
                Vertex l2_end = *next;
                
                
            }
            
        }
    
    
    }
    
    if (right - left > bottom-top) //vertical split
    {
        
    
    } else //horizontal split
    {
    }
}*/


void Polygon::canonicalize()
{
    assert( m_vertices[0] == m_vertices[m_num_vertices-1]);
    
    /** TODO: ensure polygon is oriented counter-clockwise 
              (i.e. the inside of the polygon is to the left of each line segment)
      * TODO: ensure that there are no self-intersections */
    // note that these goals cannot be solved independently:
    // - while self-intersections exist, there is no consistent orientation of the polygon
    // - while there is no consistent orientation, it is very hard to resolve self-intersections correctly
    
    
}

