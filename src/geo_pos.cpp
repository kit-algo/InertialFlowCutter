#include "geo_pos.h"
#include "io_helper.h"
#include "tiny_id_func.h"
#include "union_find.h"
#include <string>
#include <sstream>
#include <cmath>



std::vector<GeoPos> order_geo_positions_along_line(const std::vector<GeoPos>&pos){
	int point_count = pos.size();

	if(point_count <= 1)
		return pos;

	struct P{
		int p, q;
		double dist;
	};

	std::vector<P>candidate_segments;
	for(int i=0; i<point_count; ++i)
		for(int j=i+1; j<point_count; ++j)
			candidate_segments.push_back({i, j, geo_dist(pos[i], pos[j])});

	std::sort(candidate_segments.begin(), candidate_segments.end(), [](P l, P r){return l.dist < r.dist;});

	struct N{
		N():u(-1), v(-1){}
		int u, v;
		bool is_full()const{return v != -1;}
		void add(int x){assert(!is_full()); if(u == -1) u = x; else v = x;}
		int count()const{ if(u==-1) return 0; if(v==-1) return 1; return 2; }
		int other(int x)const{if(x == u) return v; else return u;}
	};

	std::vector<N>neighbors(point_count);
	UnionFind are_connected(point_count);

	int segment_count = 0;
	for(auto s:candidate_segments){
		if(!neighbors[s.p].is_full() && !neighbors[s.q].is_full() && !are_connected.in_same(s.p, s.q)){
			neighbors[s.p].add(s.q);
			neighbors[s.q].add(s.p);
			are_connected.unite(s.q, s.p);
			++segment_count;
			if(segment_count == point_count - 1)
				break;
		}
	}

	
	int x = -1;
	for(int i=0; i<point_count; ++i)
		if(neighbors[i].count() == 1){
			x = i;
			break;
		}
	assert(x != -1);
	int y = neighbors[x].u;

	std::vector<GeoPos>new_pos(point_count);

	int new_id = 0;

	for(;;){
		new_pos[new_id++] = pos[x];
		if(y == -1)
			break;
		int z = neighbors[y].other(x);
		x = y;
		y = z;
	}

	return new_pos; // NVRO
}




constexpr int dimacs_scale = 1000000;

static void save_binary_geo_pos_impl(std::ostream&out, const ArrayIDFunc<GeoPos>&geo_pos){
	int s = geo_pos.preimage_count();

	out
		.write((const char*)&s, sizeof(s))
		.write((const char*)geo_pos.begin(), s*sizeof(geo_pos[0]));
}

static void save_dimacs_geo_pos_impl(std::ostream&out, const ArrayIDFunc<GeoPos>&geo_pos){
	out << "p aux sp co "<< geo_pos.preimage_count() << '\n';
	for(int i=0; i<geo_pos.preimage_count(); ++i)
		out << "v " << (i+1) << ' ' << (int)std::round(dimacs_scale*geo_pos(i).lon) << ' ' << (int)std::round(dimacs_scale*geo_pos(i).lat) << '\n';
}

static ArrayIDFunc<GeoPos> load_binary_geo_pos_impl(std::istream&in, long long size){
	int s;
	if(!in.read((char*)&s, sizeof(s)))
		throw std::runtime_error("Could not read number of geo coordinates");
	if(size != static_cast<long long>(sizeof(GeoPos))*s + static_cast<long long>(sizeof(s)))
		throw std::runtime_error("Binary GeoPos file has an invalid size");

	ArrayIDFunc<GeoPos>f(s);
	if(!in.read((char*)f.begin(), sizeof(GeoPos)*s))
		throw std::runtime_error("Could not read GeoPos data");	
	return f; // NVRO
}

static ArrayIDFunc<GeoPos> load_dimacs_geo_pos_impl(std::istream&in){

	ArrayIDFunc<GeoPos>geo_pos;
	BitIDFunc seen;
	bool seen_header = false;
	int seen_count = 0;

	std::string line;
	int line_num = 0;
	while(std::getline(in, line)){
		++line_num;
		if(line.empty() || line[0] == 'c')
			continue;
		if(!seen_header){
			std::istringstream l(line);
			std::string b;
			if(!(l>>b) || b != "p" || !(l>>b) || b != "aux" || !(l>>b) || b != "sp" || !(l>>b) || b != "co")
				throw std::runtime_error("Header broken in line "+std::to_string(line_num));

			int s;
			if(!(l>>s))
				throw std::runtime_error("Header missing size");

			if(l >> b)
				throw std::runtime_error("The header in line "+std::to_string(line_num)+" contains extra charachters at the end of the line");

			geo_pos = ArrayIDFunc<GeoPos>(s);
			seen = BitIDFunc(s);
			seen.fill(false);
			seen_header = true;
		}else{
			std::istringstream l(line);
			std::string v;
			if(!(l>>v) || v != "v")
				throw std::runtime_error("Line "+std::to_string(line_num)+" is broken");

			long long node, lon, lat;
			if(!(l >> node >> lon >> lat))
				throw std::runtime_error("Could not read the data in line "+std::to_string(line_num));
			
			if(l >> v)
				throw std::runtime_error("Line "+std::to_string(line_num)+" contains extra charachters at the end of the line");
			--node;
			if(seen(node))
				throw std::runtime_error("Node "+std::to_string(node)+" has a second pair of coordinates in line "+std::to_string(line_num));
			seen.set(node, true);
			++seen_count;
			geo_pos[node].lon = static_cast<double>(lon) / dimacs_scale;
			geo_pos[node].lat = static_cast<double>(lat) / dimacs_scale;
		}
	}

	if(!seen_header)
		throw std::runtime_error("File is missing header");
	if(seen_count != geo_pos.preimage_count())
		throw std::runtime_error("Not every node has a coordinate");
	return geo_pos; // NVRO
}


ArrayIDFunc<GeoPos>load_binary_geo_pos(const std::string&file_name){
	return load_binary_file(file_name, load_binary_geo_pos_impl);
}

void save_binary_geo_pos(const std::string&file_name, const ArrayIDFunc<GeoPos>&geo_pos){
	save_binary_file(file_name, save_binary_geo_pos_impl, geo_pos);
}

void save_dimacs_geo_pos(const std::string&file_name, const ArrayIDFunc<GeoPos>&geo_pos){
	save_text_file(file_name, save_dimacs_geo_pos_impl, geo_pos);
}

ArrayIDFunc<GeoPos>uncached_load_dimacs_geo_pos(const std::string&file_name){
	return load_uncached_text_file(file_name, load_dimacs_geo_pos_impl);
}

ArrayIDFunc<GeoPos>load_dimacs_geo_pos(const std::string&file_name){
	return load_cached_text_file(file_name, "dimacs_coord", load_dimacs_geo_pos_impl, load_binary_geo_pos_impl, save_binary_geo_pos_impl);
}

