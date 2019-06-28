#ifndef GEO_POS_H
#define GEO_POS_H

#include <cmath>
#include <vector>

struct GeoPos{
	double lat, lon;
};

//! Returns the distance between two latlon in meters.
//! 
//! Code adapted from http://www.movable-type.co.uk/scripts/latlong.html
inline
double geo_dist(GeoPos a, GeoPos b){

	const double pi = 3.14159265359;
	const double R = 6371000.0;

	a.lat /= 180;
	a.lat *= pi;
	b.lat /= 180;
	b.lat *= pi;
	a.lon /= 180;
	a.lon *= pi;
	b.lon /= 180;
	b.lon *= pi;
	
	double dlat = b.lat - a.lat;
	double dlon = b.lon - a.lon;


	double a_ = sin(dlat/2.0) * sin(dlat/2.0) + sin(dlon/2.0) * sin(dlon/2.0) * cos(a.lat) * cos(b.lat);
	double c = 2 * atan2(sqrt(a_), sqrt(1-a_));
	return R * c;

/*var R = 6371; // km
var dLat = (b.lat-a.lat).toRad();
var dLon = (b.lon-a.lon).toRad();
var a.lat = a.lat.toRad();
var b.lat = b.lat.toRad();

var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
        Math.sin(dLon/2) * Math.sin(dLon/2) * Math.cos(a.lat) * Math.cos(b.lat); 
var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
var d = R * c;*/

}

inline GeoPos mid_geo_pos(GeoPos a, GeoPos b){
	return GeoPos {(a.lat+b.lat)/2, (a.lon+b.lon)/2};
}

std::vector<GeoPos> order_geo_positions_along_line(const std::vector<GeoPos>&pos);


#include "array_id_func.h"
#include <string>

ArrayIDFunc<GeoPos>load_binary_geo_pos(const std::string&file_name);
void save_binary_geo_pos(const std::string&file_name, const ArrayIDFunc<GeoPos>&geo_pos);
void save_dimacs_geo_pos(const std::string&file_name, const ArrayIDFunc<GeoPos>&geo_pos);
ArrayIDFunc<GeoPos>uncached_load_dimacs_geo_pos(const std::string&file_name);
ArrayIDFunc<GeoPos>load_dimacs_geo_pos(const std::string&file_name);

#endif
