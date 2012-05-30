
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>  //for mkdir

#include <set>
#include <list>
#include <fstream>

#include "osm_types.h"

map<string, uint32_t> zone_entries;

void ensureDirectoryExists(string directory)
{
    struct stat dummy;
    size_t start_pos = 0;
    
    do
    {
        size_t pos = directory.find('/', start_pos);
        string basedir = directory.substr(0, pos);  //works even if no slash is present --> pos == string::npos
        if (0!= stat(basedir.c_str(), &dummy)) //directory does not yet exist
            if (0 != mkdir(basedir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
                { perror("[ERR] mkdir");}
        if (pos == string::npos) break;
        start_pos = pos+1;
    } while ( true);
}


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

/** 
 *  @brief: 
 *  @value tags: the list key-value pairs that a way has to contain in order to be part of the network
 *  @value base_name: the basic file name of the output, a serial number and the extension ".csv" are appended
 */
void extractNetwork(FILE* src, double allowedDeviation, string base_name)
{
    
    map<Vertex, list<PolygonSegment*> > borders;
    set<PolygonSegment*> border_storage;
    
    //list<Way> loops;
    //for (uint64_t i = 0; i < num_ways; i++)
    int i = 0;
    while (! feof(src))
    {
        if (++i % 10000 == 0) std::cout << i/1000 << "k ways read, " << std::endl;
        
        //if (! way_index[i]) continue;
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);

        OSMIntegratedWay way(src);
        
        //for (list<OSMKeyValuePair>::const_iterator tag = tags.begin(); tag != tags.end(); tag++)
        //if (! way.hasKey(tag->first) || way[tag->first] != tag->second) continue;  //not part of the network
        
        PolygonSegment s = way.toPolygonSegment();
        if (s.front() == s.back()) { 
            if (s.simplifyArea(allowedDeviation))
                dumpPolygon(base_name,s);
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
    for (list<Vertex>::const_iterator it = endpoints.begin(); it!= endpoints.end(); it++)
    {
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
                if (seg1->simplifyArea(allowedDeviation))
                    dumpPolygon(base_name,*seg1);
                border_storage.erase(seg1);
                delete seg1;
                continue; 
            }
            borders[seg1->front()].push_back(seg1);
            borders[seg1->back() ].push_back(seg1);
        }
    }
    
    cout << "Merged them to " << border_storage.size() << " " << base_name << " segments" << endl;
    i = 0;
    for (set<PolygonSegment*>::iterator it = border_storage.begin(); it != border_storage.end(); it++)
    {
        if (++i % 10000 == 0) std::cout << i/1000 << "k segments simplified, " << std::endl;
        PolygonSegment *seg = *it;
        seg->simplifyStroke(allowedDeviation);
        dumpPolygon(base_name, *seg);
        delete seg;
    }
    
    cout << "Altogether " << zone_entries[base_name] << " "<< base_name <<" boundary segments (including loops)" << endl;
    
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
    extractNetwork(fopen("countries.dump", "rb"), allowedDeviation, "output/country/country");
    extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
}
