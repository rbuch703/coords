
#include <stdint.h>
#include "osm_types.h"
#include <stdio.h>
#include <iostream>

#include <assert.h>

using namespace std;


void extractMagdeburgAmenities()
{

    map<string, FILE*> am_files;
    am_files.insert(pair<string, FILE*>("fuel", fopen("fuel_stations.bin", "wb")));
    am_files.insert(pair<string, FILE*>("kindergarten", fopen("kindergarten.bin", "wb")));
    am_files.insert(pair<string, FILE*>("school", fopen("school.bin", "wb")));
    am_files.insert(pair<string, FILE*>("place_of_worship", fopen("place_of_worship.bin", "wb")));
    am_files.insert(pair<string, FILE*>("fast_food", fopen("fastfood.bin", "wb")));
    am_files.insert(pair<string, FILE*>("bank", fopen("bank.bin", "wb")));
    am_files.insert(pair<string, FILE*>("post_box", fopen("postbox.bin", "wb")));
    am_files.insert(pair<string, FILE*>("pharmacy", fopen("pharmacy.bin", "wb")));

    FILE* fWayIdx =  fopen("intermediate/ways.idx", "rb");
    FILE* fWayData=  fopen("intermediate/ways_int.data", "rb");

    fseek(fWayIdx, 0, SEEK_END);
    uint64_t numWays = ftell(fWayData) / sizeof(int64_t);
    //Ways* ways = new Ways("ways.idx", "ways_int.data");
    //FILE* fFuelStation = fopen("fuel_stations_of_magdeburg.bin", "wb");

    map<string, int> amenities;
    for (uint64_t way_id = 0; way_id < numWays; way_id++)
    {
        if (way_id % 1000000 == 0)
            cout << (way_id/1000000) << "M ways read." << endl;   
        //if (! ways->exists(way_id)) continue;
        OSMIntegratedWay way(fWayIdx, fWayData, way_id);
        if (way.id != way_id)
            continue;

        if (!way.hasKey("amenity")) continue;
        
        double center_x = 0;
        double center_y = 0;
        for ( list<OSMVertex>::const_iterator v = way.vertices.begin(); v != way.vertices.end(); v++)
        {
            center_x += v->x;
            center_y += v->y;
        }
        center_x /= way.vertices.size();
        center_y /= way.vertices.size();

        if (!( center_x >= 52.07*10000000 && center_x <= 52.23 * 10000000 &&
               center_y >= 11.5 *10000000 && center_y <= 11.75 * 10000000))
               continue;
       //cout << way << endl;
       if (! amenities.count(way["amenity"])) amenities.insert( pair<string, int>(way["amenity"], 0));
       amenities[way["amenity"]] += 1;
       
       if (! am_files.count(way["amenity"])) continue;
       //if ( way["amenity"] != "fuel") continue;
       int32_t x = center_x;
       fwrite( &x, sizeof(x), 1, am_files[way["amenity"]] );
       int32_t y = center_y;
       fwrite( &y, sizeof(y), 1, am_files[way["amenity"]] );
       
    }

    fclose(fWayIdx);
    fclose(fWayData);
    //delete ways;
     
    FILE* fNodeIdx = fopen("intermediate/nodes.idx", "rb");
    FILE* fNodeData= fopen("intermediate/nodes.data", "rb");
     
    fseek(fNodeIdx, 0, SEEK_END);
    uint64_t numNodes = ftell(fNodeIdx) / sizeof(int64_t);
     
    //Nodes nodes("intermediate/nodes.idx", "intermediate/nodes.data");
    
    for (uint64_t idx = 0; idx < numNodes; idx++)
    {
        if (idx % 10000000 == 0) cout << (idx/1000000) << "M nodes read." << endl;
        
        OSMNode node(fNodeIdx, fNodeData, idx);
        if ( node.id != idx) 
            continue;
        
        if (! node.hasKey("amenity")) continue;
        
        if (!( node.lat >= 52.07*10000000 && node.lat <= 52.23 * 10000000 &&
               node.lon >= 11.5 *10000000 && node.lon <= 11.75 * 10000000))
               continue;
        
       if (! amenities.count(node["amenity"])) amenities.insert( pair<string, int>(node["amenity"], 0));
       amenities[node["amenity"]] += 1;
        //cout << node << endl;

       //if ( node["amenity"] != "fuel") continue;
   
       if (! am_files.count(node["amenity"])) continue;
               
       fwrite( &node.lat, sizeof(node.lat), 1, am_files[node["amenity"]] );
       fwrite( &node.lon, sizeof(node.lon), 1, am_files[node["amenity"]] );
        
    }
    //fclose(fFuelStation);
    for ( map<string, int>::const_iterator it = amenities.begin(); it != amenities.end(); it++)
        cout << it->first << ":\t" << it->second << endl;
    
    //FIXME: close all file pointers in am_files
}

void extractMagdeburgRoads()
{
    FILE *f = fopen("intermediate/roads.dump", "rb");
    FILE* f_out = fopen("street_of_magdeburg.bin", "wb");
    int i = 0;
    int ch;
    
    map<string,int32_t> vMaxSpeedValues;
    vMaxSpeedValues.insert( pair<string, int32_t>("3", 3));
    vMaxSpeedValues.insert( pair<string, int32_t>("5", 5));
    vMaxSpeedValues.insert( pair<string, int32_t>("7", 7));
    vMaxSpeedValues.insert( pair<string, int32_t>("10", 10));
    vMaxSpeedValues.insert( pair<string, int32_t>("20", 20));
    vMaxSpeedValues.insert( pair<string, int32_t>("30", 30));
    vMaxSpeedValues.insert( pair<string, int32_t>("50", 50));
    vMaxSpeedValues.insert( pair<string, int32_t>("60", 60));
    vMaxSpeedValues.insert( pair<string, int32_t>("70", 70));
    vMaxSpeedValues.insert( pair<string, int32_t>("80", 80));
    vMaxSpeedValues.insert( pair<string, int32_t>("100", 100));
    vMaxSpeedValues.insert( pair<string, int32_t>("120", 120));
    vMaxSpeedValues.insert( pair<string, int32_t>("none", 130));
    vMaxSpeedValues.insert( pair<string, int32_t>("signals", 130));
    vMaxSpeedValues.insert( pair<string, int32_t>("walk", 5));
    vMaxSpeedValues.insert( pair<string, int32_t>("", -1));
    
    
    map<string, int32_t> road_classification;
    static const int PED = 1;
    static const int BIKE= 2;
    static const int CAR = 4;
    //legend: OR-combination of the following:
    /* 1: usable by pedestrians; 
       2: usable by bicycles;
       4: usable by cars
    */

    road_classification.insert(pair<string, int32_t>("construction",    0));    //under construction, impassable
    road_classification.insert(pair<string, int32_t>("proposed",        0));    
    road_classification.insert(pair<string, int32_t>("raceway",         0));    
    road_classification.insert(pair<string, int32_t>("bridleway",       PED|BIKE));
    road_classification.insert(pair<string, int32_t>("bus_stop",        PED|BIKE));
    road_classification.insert(pair<string, int32_t>("cycleway",        PED|BIKE));    
    road_classification.insert(pair<string, int32_t>("footway",         PED|BIKE));    
    road_classification.insert(pair<string, int32_t>("steps",           PED|BIKE));    
    road_classification.insert(pair<string, int32_t>("path",            PED|BIKE));    
    road_classification.insert(pair<string, int32_t>("pedestrian",      PED|BIKE));    
    road_classification.insert(pair<string, int32_t>("motorway",                 CAR));
    road_classification.insert(pair<string, int32_t>("motorway_link",            CAR));    
    road_classification.insert(pair<string, int32_t>("trunk",                    CAR));    
    road_classification.insert(pair<string, int32_t>("trunk_link",               CAR));    
    road_classification.insert(pair<string, int32_t>("living_street",   PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("primary",         PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("primary_link",    PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("residential",     PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("road",            PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("secondary",       PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("secondary_link",  PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("service",         PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("tertiary",        PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("track",           PED|BIKE|CAR));    
    road_classification.insert(pair<string, int32_t>("unclassified",    PED|BIKE|CAR));    


    while ( (ch = fgetc(f)) != EOF )
    {
        ungetc(ch, f);
        //int d = fputc(ch, f);
        //perror("fputc");
        //assert(d == ch);
        if (++i % 1000000 == 0) cerr << (i/1000000) << "M roads read" << endl;
        OSMIntegratedWay way(f, -1);
        
        bool inMagdeburg = false;
        for (list<OSMVertex>::const_iterator v = way.vertices.begin(); v != way.vertices.end(); v++)
            inMagdeburg |= 
                v->x >= 52.07*10000000 && v->x <= 52.23 * 10000000 &&
                v->y >= 11.5 *10000000 && v->y <= 11.75 * 10000000;

        if (!inMagdeburg) continue;
        assert( road_classification.count(way["highway"]));

        
        uint32_t nVertices = way.vertices.size();
        fwrite( &nVertices, sizeof(nVertices), 1, f_out);
        if ( !vMaxSpeedValues.count( way["maxspeed"]) )
        {
            cerr << "unknown maxspeed value '" << way["maxspeed"] << "'" << endl;
            assert(false);
        }
        
        uint32_t maxSpeed  = vMaxSpeedValues[ way["maxspeed"]] ;
        fwrite( &maxSpeed, sizeof(maxSpeed), 1, f_out);
        
        uint32_t classification = road_classification[ way["highway"]];
        fwrite( &classification, sizeof(classification), 1, f_out);
        
        
        for (list<OSMVertex>::const_iterator v = way.vertices.begin(); v != way.vertices.end(); v++)
        {
            fwrite(&(v->x), sizeof(v->x), 1, f_out);
            fwrite(&(v->y), sizeof(v->y), 1, f_out);
        }

            //cout << way << endl;
        
        //tmp.isClockwise();
        //cout << way << endl;        
    }
    fclose(f);
    fclose(f_out);
     
     /*
    cout << "list of highway types follows:" << endl;
    for (map<string, int>::const_iterator s = highway_types.begin(); s != highway_types.end(); s++)
        cout << (s->first) << ":\t" << s->second << endl;*/
    //clipRecursive( "output/coast/building", "", poly_storage);      
}

int main() {
    extractMagdeburgRoads();
    extractMagdeburgAmenities();

    return 0;
}

