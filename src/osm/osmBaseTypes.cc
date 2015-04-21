
#include "osmBaseTypes.h"

OsmRelationMember::OsmRelationMember( OSM_ENTITY_TYPE member_type, 
                                      uint64_t member_ref, 
                                      std::string member_role):
    type(member_type), ref(member_ref), role(member_role) 
{ 

}

bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b) 
{ return a.lat == b.lat && a.lng == b.lng; }

bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b) 
{ return a.lat != b.lat || a.lng != b.lng; }

bool operator< (const OsmGeoPosition &a, const OsmGeoPosition &b) 
{ return a.lat < b.lat || (a.lat == b.lat && a.lng < b.lng);}

