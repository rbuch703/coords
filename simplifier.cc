
#include <stdio.h>
#include <assert.h>

#include <set>
#include <list>
#include <fstream>
#include <iostream>

#include "osm_types.h"
#include "polygonreconstructor.h"
#include "helpers.h"

map<string, uint32_t> zone_entries;



void dumpPolygon(string file_base, const PolygonSegment& segment)
{
    size_t pos = file_base.rfind('/');
    string directory = file_base.substr(0, pos);
    //zone = zone.substr(pos+1);
    ensureDirectoryExists(directory);
    
    int file_num = 1;
    if ( zone_entries.count(file_base) )
        file_num = ++zone_entries[file_base];
    else (zone_entries.insert( pair<string, uint32_t>(file_base, 1)));
    
    ofstream out;
    char tmp[200];
    snprintf(tmp, 200, "%s_%u.csv", file_base.c_str(), file_num);
    //string filename = zone+"_"+ file_num+".csv";
    out.open(tmp);
    for (list<Vertex>::const_iterator vertex = segment.vertices().begin(); vertex != segment.vertices().end(); vertex++)
    {
        out << vertex->x << ", " << vertex->y << endl;
    }
    out.close();
}

void handlePolygon(string file_base, PolygonSegment& segment)
{
    
    list<PolygonSegment> above, below;
    
    segment.clipHorizontally(40*10000000, above, below);
    if (above.size() == 0 || below.size() == 0) return; // no clipping took place
    
    for (list<PolygonSegment>::const_iterator it = above.begin(); it != above.end(); it++)
        dumpPolygon(file_base, *it);
/*
    for (list<PolygonSegment>::const_iterator it = below.begin(); it != below.end(); it++)
        dumpPolygon(file_base, *it);
    
    if (segment.simplifyArea(100*1000) && segment.vertices().size() > 1000)
        dumpPolygon(file_base,segment);
*/
}
/** 
 *  @brief: 
 *  @value base_name: the basic file name of the output, a serial number and the extension ".csv" are appended
 */
void extractNetwork(FILE* src, double allowedDeviation, string base_name)
{
    
    map<Vertex, list<PolygonSegment*> > borders;
    set<PolygonSegment*> border_storage;
    
    //list<Way> loops;
    //for (uint64_t i = 0; i < num_ways; i++)
    int idx = 0;
    while (! feof(src))
    {
        if (++idx % 1 == 0) std::cout << idx << " ways read, " << std::endl;
        
        //if (! way_index[i]) continue;
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);

        OSMIntegratedWay way(src);
        
        PolygonSegment s = way.toPolygonSegment();
        if (s.front() == s.back()) { 
            handlePolygon(base_name, s);
            continue; 
        }
        PolygonSegment *seg = new PolygonSegment(s);
        
        borders[seg->front()].push_back(seg);
        borders[seg->back()].push_back (seg);
        border_storage.insert(seg);
    }
    // copy of endpoint vertices from 'borders', so that we can iterate over the endpoints and still safely modify the map
    list<Vertex> endpoints; 
    for (map<Vertex, list<PolygonSegment*> >::const_iterator v = borders.begin(); v != borders.end(); v++)
        endpoints.push_back(v->first);
    
    cout << "Found " << border_storage.size() << " " << base_name <<" segments and " << zone_entries[base_name] << " loops" << endl;
    idx = 0;
    for (list<Vertex>::const_iterator it = endpoints.begin(); it!= endpoints.end(); it++)
    {
        if (++idx % 1000 == 0) cout << idx/1000 << "k segments joined" << endl;
        Vertex v = *it;
        if (borders[v].size() == 2)  //exactly two border segments meet here --> merge them
        {
            PolygonSegment *seg1 = borders[v].front();
            PolygonSegment *seg2 = borders[v].back();        
            borders[ seg1->front()].remove(seg1);
            borders[ seg1->back() ].remove(seg1);
            borders[ seg2->front()].remove(seg2);
            borders[ seg2->back() ].remove(seg2);
            border_storage.erase(seg2);

            if      (seg1->front() == seg2->front()) { seg1->reverse(); seg1->append(*seg2, true); }
            else if (seg1->back()  == seg2->front()) {                  seg1->append(*seg2, true); }
            else if (seg1->front() == seg2->back() ) { seg2->append(*seg1, true); *seg1 = *seg2;   }
            else if (seg1->back()  == seg2->back() ) { seg2->reverse(); seg1->append(*seg2, true); }
            else assert(false);
            delete seg2;
            if (seg1->front() == seg1->back())
            {
                border_storage.erase(seg1);
                handlePolygon(base_name, *seg1);
                delete seg1;
                continue; 
            }
            borders[seg1->front()].push_back(seg1);
            borders[seg1->back() ].push_back(seg1);
        }
    }
    cout << "Merged them to " << border_storage.size() << " " << base_name << " segments" << endl;
    idx = 0;
    for (set<PolygonSegment*>::iterator it = border_storage.begin(); it != border_storage.end(); it++)
    {
        if (++idx % 1000 == 0) std::cout << idx/1000 << "k segments simplified, " << std::endl;
        PolygonSegment *seg = *it;
        seg->simplifyStroke(allowedDeviation);
        assert(seg->front() != seg->back()); //if both are equal it should not be in this list
        //dumpPolygon(base_name, *seg);
        delete seg;
    }
    
    cout << "Altogether " << zone_entries[base_name] << " "<< base_name <<" boundary segments (including loops)" << endl;
    
}


void reconstructCoastline(FILE * src, string base_name)
{
    PolygonReconstructor recon;
    int idx = 0;
    while (! feof(src))
    {
        if (++idx % 10000 == 0) std::cout << idx/1000 << "k ways read, " << std::endl;
        
        //if (! way_index[i]) continue;
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);
        OSMIntegratedWay way(src);
        
        PolygonSegment s = way.toPolygonSegment();
        PolygonSegment* seg = recon.add(s);
        if (seg)
            handlePolygon(base_name, *seg);
        recon.clearPolygonList();
    }
    cout << "closing polygons" << endl;
    recon.forceClosePolygons();
    const list<PolygonSegment>& lst = recon.getClosedPolygons();
    cout << "done, found " << lst.size() << endl;
    for (list<PolygonSegment>::const_iterator seg = lst.begin(); seg != lst.end(); seg++)
    {
        PolygonSegment s = *seg;
        handlePolygon(base_name, s);
    }
    
    
}
/*
void clipNSplit( int32_t left, int32_t top, int32_t right, int32_t bottom, list<PolygonSegment> &polygons)
{
    list<PolygonSegment> left;
    list<PolygonSegment> right;
}*/

int main()
{
    double allowedDeviation = 100* 40000.0; // about 40km (~1/10000th of the earth circumference)
    //extractNetwork(fopen("countries.dump", "rb"), allowedDeviation, "output/country/country");
    //extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    //extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
    reconstructCoastline(fopen("coastline.dump", "rb")/*,allowedDeviation*/, "output/clipping/clip");
}
