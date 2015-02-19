
#include "osm/osm_tags.h"

const char * ignore_key_prefixes[]={
    "note:",    // localized notes
    "CLC:",     // data source ID from Corine Land Cover
    "source:",  // various import information
    "VRS:",     // annotations of a single local German public transport system
    "AND_",     // annotations from a Netherlands database
    "AND:",
    "b5m:",
    "bak:", 
    "cladr:",   //annotations from Russian CLADR database
    "canvec:", //"CanVec is a digital cartographic reference product produced by Natural Resources Canada."
    "dcgis:", 
    "educamadrid:",
    "geobase:",
    "GeoBaseNHN:",
    "gns_",
    "gns:",
    "GNS:",
    "GNIS:", //annotations from import from US Geographic Names Information System
    "gnis:",
    "ideewfs:",
    "it:fvg:",
    "kms:",
    "KSJ2:", //annotations from import from Japanese KSJ2 database
    "massgis:",      // annotations from MassGIS (Massachusetts, US)
    "naptan:", // annotations from import from UK NaPTAN database
    "ngbe:",
    "NHD_", //The National Hydrography Dataset (NHD) is a United States-
    "NHD-",
    "NHD:",
    "nhd_",
    "nhd-",
    "nhd:",    
    "openGeoDB:",
    "opengeodb:",
    "rednap:",
    "semnav:",
    "siruta:"
    "teryt:", // annotations from Polish TERYT database
    "tiger:",
    "TIGER:",
    "TMC:cid_58:",   //data source from German TMC system
    "USGS-LULC:",      //US geological survey?
    "WroclawGIS:",
};

uint32_t num_ignore_key_prefixes = sizeof(ignore_key_prefixes)/ sizeof(const char *);

const char * ignore_keys[]={
    "created_by", // author of node/way/relation
    "converted_by",
    "note",       //notes are usually information for fellow mappers, but not relevant for viewers;
    "source",      // generic data source
    "3dshapes:ggmodelk",
    "FIXME",
    "fixme",
    "GoeVB:id",  // ???
    "IBNR",
    "ID:tobwen",
    "ID_SLH_IMPORT",
    "attribution",
    "odbl",
    "build",
    "import",
    "import_uuid",
    "note:import-bati",
    "uic_ref",   //unique international train station id; not needed right now TODO: white-list this later?
    "onkz",      //German telephone area codes ("Ortskennzahl")
    "AND",      // annotations from a Netherlands database
    "cladr:code",   //annotations from Russian CLADR database
    "canvec",   //"CanVec is a digital cartographic reference product produced by Natural Resources Canada."
    "canvec tile",
    "cxx:code",
    "cxx:id",
    "de:amtlicher_gemeindeschluessel",
    "dcgis",
    "dibavod:id",
    
    "educamadrid",
    
    "fresno_APN",       //Fresno_County_GIS 
    "lot_description",  //part of the former
    "fresno_TX_area_CD",
    "fresno_lot_width",
    "fresno_lot_depth",
    "fresno_lot_area",
    "geonames_id", 
    "gnis", //annotations from import from US Geographic Names Information System
    "gtfs_id",
    "idee:name",
    "koatuu",        //???
    "LINZ:source_version",
    "mvdgis:padron",
    "mvdgis:cod_nombre",
    "osak:identifier",
    "osak:municipality_no",
    "osak:revision",
    "osak:street",
    "osak:street_no",
    "osak:subdivision",
    "uir_adr:ADRESA_KOD",
    "GNS:id",
    "openGeocodeDB:postal_codes",
    "surrey:addrid",
    "source_ref",   //generic name of the data source
    "statscan:rbuid",
    "tiger 2010:way_id", // annotations from the US census bureau TIGER road database
    "tiger.source:tlid",
    "tiger name",
    "tiger_base",
    "tiger_name_type",
    "tiger-reviewed",        
    "us.fo:Adressutal", //annotations from Umhvørvisstovan (http://www.us.fo); the Faroese National Survey and cadastre
    "usar_addr:edit_date",
    "ref:INSEE",    //annotations from French INSEE database
    "ref:SIREN",

    "yh:STRUCTURE",
    "yh:TOTYUMONO",
    "yh:TYPE",
    "yh:WIDTH",
    "yh:WIDTH_RANK",
    "yh:LINE_NAME",
    "yh:LINE_NUM",

    "www.prezzibenzina.it"
    };
    
uint32_t num_ignore_keys = sizeof(ignore_keys)/ sizeof(const char *);


