
#include "osm/osm_tags.h"

const char * ignore_key_prefixes[]={
    "AND_",     // annotations from a Netherlands database
    "AND:",
    "bag:",
    "b5m:",
    "bak:", 
    "building:ruian:",
    "canvec:", //"Canadian database by Natural Resources Canada."
    "cladr:",  //annotations from Russian CLADR database
    "CLC:",    // data source ID from Corine Land Cover
    "comment:", //localized comments
    "dcgis:", 
    "description:",
    "dibavod:",
    "educamadrid:",
    "ewmapa:",
    "fixme:",
    "FIXME:",
    "geobase:",
    "GeoBaseNHN:",
    "gns_",
    "gns:",
    "GNS:",
    "GNIS:", //annotations from import from US Geographic Names Information System
    "gnis:",
    "gst:",
    "ideewfs:",
    "it:fvg:",
    "it:lo:",
    "it:pv:",
    "kms:",
    "KSJ2:", //annotations from import from Japanese KSJ2 database
    "lbcs:",
    "LINZ:",
    "LINZ2OSM:",
    "linz:",
    "maaamet:",
    "massgis:",      // annotations from MassGIS (Massachusetts, US)
    "mvdgis:",
    "naptan:", // annotations from import from UK NaPTAN database
    "ngbe:",
    "NHD_", //The National Hydrography Dataset (NHD) is a United States-
    "NHD-",
    "NHD:",
    "nhd_",
    "nhd-",
    "nhd:", 
    "note:",    // localized notes
    "openGeoDB:",
    "opengeodb:",
    "osak:",    //Danish OSAK database
    "rednap:",
    "ref:ruian",
    "ref:FR:",
    "ref:opendataparis:",
    "semnav:",
    "siruta:",
    "surrey:",
    "source:",              // various import information
    "source_ref:",
    "teryt:",               // from Polish TERYT database
    "tiger:",
    "TIGER:",
    "TMC:cid_58:",          //data source from German TMC system
    "USGS-LULC:",      //US geological survey?
    "VRS:",     // annotations of a single local German public transport system
    "WroclawGIS:",
    "yh:"
};

uint32_t num_ignore_key_prefixes = sizeof(ignore_key_prefixes)/ sizeof(const char *);

const char * ignore_keys[]={
    "created_by", // author of node/way/relation
    "converted_by",
    "note",       //notes are usually information for fellow mappers, but not relevant for viewers;
    "comment",
    "source",      // generic data source
    "FIXME",
    "fixme",
    
    "AND",      // annotations from a Netherlands database
    "attribution",
    "addr:city:simc",
    "addr:street:simc",
    "addr:street:sym_ul",
    "addr:postcode:source",
    "build",
    "building:type:pl",
    "building:usage:pl",
    "building:use:pl",  
    "chicago:building_id",
    "cladr:code",   //annotations from Russian CLADR database
    "cxx:code",
    "cxx:id",
    "de:amtlicher_gemeindeschluessel",
    "description",
    "dcgis",
    "dibavod:id",
    "educamadrid",
    "FLATE",
    "fresno_APN",       //Fresno_County_GIS 
    "fresno_lot_width",
    "fresno_lot_depth",
    "fresno_lot_area",
    "fresno_TX_area_CD",
    "geonames_id", 
    "gnis", //annotations from import from US Geographic Names Information System
    "GoeVB:id",  // ???
    "gtfs_id",
    "IBNR",
    "ID:tobwen",
    "ID_SLH_IMPORT",
    "idee:name",
    "import",
    "import_uuid",
    "koatuu",        //???
    "linz2osm:objectid",
    "lot_description",  
    "lojic:bgnum",
    "mvdgis:padron",
    "mvdgis:cod_nombre",
    "nycdoitt:bin",
    "odbl",
    "onkz",      //German telephone area codes ("Ortskennzahl")
    "project:eurosha_2012",
    "ref:bag",      //reference number for the BAG database of properties in the Netherlands. 
    "ref:bagid",
    "ref:INSEE",    //annotations from French INSEE database
    "ref:SIREN",
    "ref:UrbIS",
    "ref:old",
    "SK53_bulk:load",
    "source_ref",   //generic name of the data source
    "statscan:rbuid",
//    "start_date",
    "tiger 2010:way_id", // annotations from the US census bureau TIGER road database
    "tiger.source:tlid",
    "tiger name",
    "tiger_base",
    "tiger_name_type",
    "tiger-reviewed",        
    "uic_ref",   //unique international train station id; not needed right now TODO: white-list this later?
    "uir_adr:ADRESA_KOD",
    "us.fo:Adressutal", //annotations from Umhvørvisstovan (http://www.us.fo); the Faroese National Survey and cadastre
    "usar_addr:edit_date",
    };
    
uint32_t num_ignore_keys = sizeof(ignore_keys)/ sizeof(const char *);


