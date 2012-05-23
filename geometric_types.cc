
#include "geometric_types.h"


double Vertex::distanceToLine(const Vertex A, const Vertex B) const
{
    assert( B!= A);
    double num = fabs( ((double)B.x-A.x)*((double)A.y-y) - ((double)B.y-A.y)*((double)A.x-x));
    double denom = sqrt( ((double)B.x-A.x)*((double)B.x-A.x) + ((double)B.y-A.y)*((double)B.y-A.y));
    return num / denom;
}

uint64_t Vertex::squaredDistanceTo(const Vertex other) const
{ 
    return (uint64_t)((int64_t)x - other.x)*(x-other.x) + ((int64_t)y-other.y)*(y-other.y); 
}


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}

/** =================================================*/

void PolygonSegment::append(const PolygonSegment &other, bool exactMatch)
{
    if (exactMatch) 
    {
        assert(back() == other.front());
        m_vertices.insert( m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
    } else
    {
        assert(back() != other.front()); //otherwise we would have two consecutive identical vertices
        cout << "concatenating line segments that are " 
             << sqrt( back().squaredDistanceTo(other.front()))/100.0 << "m apart"<< endl;
        m_vertices.insert( m_vertices.end(),   other.m_vertices.begin(), other.m_vertices.end());
    }
    
    //std::cout << "Merged: " << res
    //return res;
}

bool PolygonSegment::simplify(double allowedDeviation)
{
    int nVertices = 0;

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

    }
    // since first and last vertex are identical, we need at least four vertices to define any area
    if (nVertices < 4) return false;

    //uint64_t area = (max_lat - min_lat)*(max_lon - min_lon);
    //cout << "Area (Equatorial equivalent): " << area/(100000.0*100000.0) << "km²" << std::endl;
    
    // if the whole polygon (area) is smaller than our measure of allowed deviation
    /*if ( area < allowedDeviation*allowedDeviation) 
    {   return false;
    }*/
    //return true;
    int mid_point = nVertices/2;
    list<Vertex>::iterator it_mid = m_vertices.begin();
    while (mid_point--) it_mid++;
    simplifySection( m_vertices.begin(), it_mid, allowedDeviation);
    
    list<Vertex>::iterator it_last = m_vertices.end();
    it_last--;
    simplifySection( it_mid, it_last, allowedDeviation);
    if (m_vertices.size() > 3)
        cout << nVertices << " --> " << m_vertices.size() << " vertices"<< endl;
    
    return m_vertices.size() > 3; //since start==end, a polygon with three vertices would be a line
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
        uint64_t dist = it->distanceToLine(A, B);
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


std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg)
{
    for (list<Vertex>::const_iterator it = seg.vertices().begin(); it != seg.vertices().end(); it++)
        os << *it << endl;
    return os;
}


