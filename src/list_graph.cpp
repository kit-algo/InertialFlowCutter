#include "list_graph.h"
#include "io_helper.h"
#include "multi_arc.h"
#include "id_multi_func.h"

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>

struct BinaryHeader{
	int node_count, arc_count;
};

static
void save_binary_graph_impl(
	std::ostream&out, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&node_weight, const ArrayIDFunc<int>&arc_weight
){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	assert(tail.preimage_count() == arc_count && tail.image_count() == node_count);
	assert(node_weight.preimage_count() == node_count);
	assert(arc_weight.preimage_count() == arc_count);

	BinaryHeader h = {node_count, arc_count};
	out
		.write((const char*)&h, sizeof(h))
		.write((const char*)tail.begin(), sizeof(int)*arc_count)
		.write((const char*)head.begin(), sizeof(int)*arc_count)
		.write((const char*)node_weight.begin(), sizeof(int)*node_count)
		.write((const char*)arc_weight.begin(), sizeof(int)*arc_count);
}

void save_binary_graph(
	const std::string&file_name, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&node_weight, const ArrayIDFunc<int>&arc_weight
){
	save_binary_file(file_name, save_binary_graph_impl, head, tail, node_weight, arc_weight);
}

static
void save_dimacs_graph_impl(
	std::ostream&out, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&arc_weight
){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	assert(tail.preimage_count() == arc_count && tail.image_count() == node_count);
	assert(arc_weight.preimage_count() == arc_count);

	out << "p sp "<< node_count << ' ' << arc_count << '\n';
	for(int i=0; i<arc_count; ++i)
		out << "a " << (tail(i)+1) << ' ' << (head(i)+1) << ' ' << arc_weight(i) << '\n';
}

void save_dimacs_graph(
	const std::string&file_name, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&arc_weight
){
	save_text_file(file_name, save_dimacs_graph_impl, tail, head, arc_weight);
}

static
void save_ddsg_graph_impl(
	std::ostream&out, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&arc_weight
){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	assert(tail.preimage_count() == arc_count && tail.image_count() == node_count);
	assert(arc_weight.preimage_count() == arc_count);

	out << "d\n"<< node_count << ' ' << arc_count << '\n';
	for(int i=0; i<arc_count; ++i)
		out << tail(i) << ' ' << head(i) << ' ' << arc_weight(i) <<" 1\n";
}


void save_ddsg_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight){
	save_text_file(file_name, save_ddsg_graph_impl, tail, head, arc_weight);
}


static
void save_csv_graph_impl(
	std::ostream&out, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&arc_weight
){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	assert(tail.preimage_count() == arc_count && tail.image_count() == node_count);
	assert(arc_weight.preimage_count() == arc_count);

	out << "tail,head,weight\n";	
	for(int i=0; i<arc_count; ++i)
		out << tail(i) << ',' << head(i) << ',' << arc_weight(i) << '\n';
	(void)node_count;
}

void save_csv_graph(
	const std::string&file_name, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
	const ArrayIDFunc<int>&arc_weight
){
	save_text_file(file_name, save_csv_graph_impl, head, tail, arc_weight);
}

static
void save_metis_graph_impl(
	std::ostream&out, 
	const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight
){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	bool are_all_arc_weights_one = true;
	for(auto x:arc_weight)
		if(x != 1){
			are_all_arc_weights_one = false;
			break;
		}

	auto out_arc = invert_id_id_func(tail);
			
	out << node_count << ' ' << arc_count/2 << ' ' << (are_all_arc_weights_one ? '0' : '1') << '\n';
	for(int x=0; x<node_count; ++x){
		bool first = true;
		for(auto xy:out_arc(x)){
			if(first) 
				first = false; 
			else 
				out << ' ';
			out << head(xy)+1;
			if(!are_all_arc_weights_one)
				out << ' ' << arc_weight(xy);
		}
		out << '\n';
	}
}

void save_metis_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight){
	if(!is_symmetric(tail, head))
		throw std::runtime_error("METIS graph format can only store symmetric graphs");
	save_text_file(file_name, save_metis_graph_impl, tail, head, arc_weight);
}


static 
void check_header(BinaryHeader h){
	if(h.node_count < 0)
		throw std::runtime_error("Node count must not be negative");
	if(h.arc_count < 0)
		throw std::runtime_error("Arc count must not be negative");
	if(h.node_count == 0 && h.arc_count > 0)
		throw std::runtime_error("A graph without nodes can not have arcs");
}

static 
ListGraph load_binary_graph_impl(std::istream&in, long long size){
	BinaryHeader h;
	if(!in.read((char*)&h, sizeof(h)))
		throw std::runtime_error("Could not read binary header");
	check_header(h);
	if(size != static_cast<long long>(sizeof(int)*(3*h.arc_count+h.node_count+2)))
		throw std::runtime_error("binary header is corrupt; file size is wrong");

	ListGraph g(h.node_count, h.arc_count);

	// No in range check for tail and head ids for efficiency reasons
	
	if(!in.read((char*)g.tail.begin(), sizeof(int)*h.arc_count))
		throw std::runtime_error("Could not read binary tails");
	if(!in.read((char*)g.head.begin(), sizeof(int)*h.arc_count))
		throw std::runtime_error("Could not read binary heads");
	if(!in.read((char*)g.node_weight.begin(), sizeof(int)*h.node_count))
		throw std::runtime_error("Could not read binary node weights");
	if(!in.read((char*)g.arc_weight.begin(), sizeof(int)*h.arc_count))
		throw std::runtime_error("Could not read binary arc weights");
	return g; // NVRO
}

ListGraph load_binary_graph(const std::string&file_name){
	return load_binary_file(file_name, load_binary_graph_impl);
}

static
ListGraph load_dimacs_graph_impl(std::istream&in){
	ListGraph graph;
	std::string line;
	int line_num = 0;
	int next_arc = 0;

	bool was_header_read = false;
	while(std::getline(in, line)){
		++line_num;
		if(line.empty() || line[0] == 'c')
			continue;

		std::istringstream lin(line);
		if(!was_header_read){
			was_header_read = true;
			std::string p, sp;
			int node_count;
			int arc_count;
			if(!(lin >> p >> sp >> node_count >> arc_count))
				throw std::runtime_error("Can not parse header in dimacs file.");
			if(p != "p" || sp != "sp" || node_count < 0 || arc_count < 0)
				throw std::runtime_error("Invalid header in dimacs file.");
			graph = ListGraph(node_count, arc_count);
		}else{
			std::string a;
			int h, t, w;
			if(!(lin >> a >> t >> h >> w))
				throw std::runtime_error("Can not parse line num "+std::to_string(line_num)+" \""+line+"\" in dimacs file.");
			--h;
			--t;
			if(a != "a" || h < 0 || h >= graph.node_count() || t < 0 || t >= graph.node_count() || w < 0)
				throw std::runtime_error("Invalid arc in line num "+std::to_string(line_num)+" \""+line+"\" in dimacs file.");
			if(next_arc < graph.arc_count()){
				graph.head[next_arc] = h;
				graph.tail[next_arc] = t;
				graph.arc_weight[next_arc] = w;
			}
			++next_arc;
		}
	}

	if(next_arc != graph.arc_count())
		throw std::runtime_error("The arc count in the header ("+std::to_string(graph.arc_count())+") does not correspond with the actual number of arcs ("+std::to_string(next_arc)+").");

	graph.node_weight.fill(0);

	return graph; // NVRO
}


ListGraph load_dimacs_graph(const std::string&file_name){
	return load_cached_text_file(file_name, "dimacs", load_dimacs_graph_impl, load_binary_graph_impl, 
		[](std::ostream&out, const ListGraph&g){
			save_binary_graph_impl(out, g.tail, g.head, g.node_weight, g.arc_weight);
		}
	);
}

ListGraph uncached_load_dimacs_graph(const std::string&file_name){
	return load_uncached_text_file(file_name, load_dimacs_graph_impl);
}

static
ListGraph load_color_dimacs_graph_impl(std::istream&in){
	ListGraph graph;
	std::string line;
	int line_num = 0;
	int next_arc = 0;

	bool was_header_read = false;
	while(std::getline(in, line)){
		++line_num;
		if(line.empty() || line[0] == 'c')
			continue;

		std::istringstream lin(line);
		if(!was_header_read){
			was_header_read = true;
			std::string p, sp;
			int node_count;
			int arc_count;
			if(!(lin >> p >> sp >> node_count >> arc_count))
				throw std::runtime_error("Can not parse header in color-dimacs file.");
			if(p != "p" || sp != "edge" || node_count < 0 || arc_count < 0)
				throw std::runtime_error("Invalid header in color-dimacs file.");
			graph = ListGraph(node_count, arc_count);
		}else{
			std::string a;
			int h, t;
			if(!(lin >> a >> t >> h))
				throw std::runtime_error("Can not parse line num "+std::to_string(line_num)+" \""+line+"\" in dimacs file.");
			--h;
			--t;
			if(a != "e" || h < 0 || h >= graph.node_count() || t < 0 || t >= graph.node_count())
				throw std::runtime_error("Invalid arc in line num "+std::to_string(line_num)+" \""+line+"\" in dimacs file.");
			if(next_arc < graph.arc_count()){
				graph.head[next_arc] = h;
				graph.tail[next_arc] = t;
				graph.arc_weight[next_arc] = 0;
			}
			++next_arc;
		}
	}

	if(next_arc != graph.arc_count())
		throw std::runtime_error("The arc count in the header ("+std::to_string(graph.arc_count())+") does not correspond with the actual number of arcs ("+std::to_string(next_arc)+").");

	graph.node_weight.fill(0);

	return graph; // NVRO
}

ListGraph load_color_dimacs_graph(const std::string&file_name){
	return load_cached_text_file(file_name, "color_dimacs", load_color_dimacs_graph_impl, load_binary_graph_impl, 
		[](std::ostream&out, const ListGraph&g){
			save_binary_graph_impl(out, g.tail, g.head, g.node_weight, g.arc_weight);
		}
	);
}

ListGraph uncached_load_color_dimacs_graph(const std::string&file_name){
	return load_uncached_text_file(file_name, load_dimacs_graph_impl);
}



static
ListGraph load_metis_graph_impl(std::istream&in){

	ListGraph g;

	std::string line;
	int line_num = 0;
	bool header_read = false;

	int next_node_id = 0;
	int next_arc_id = 0;

	int node_count, arc_count;

	bool has_arc_weights = false;
	bool has_node_weights = false;

	try{
		while(std::getline(in, line)){
			++line_num;
			if(!line.empty() && line.front() == '%')
				continue;
			std::istringstream line_in(line);
			if(!header_read){
				header_read = true;
			
				if(!(line_in >> node_count >> arc_count))
					throw std::runtime_error("Can not read header" );
				if(node_count < 0)
					throw std::runtime_error("node_count must be non-negative; it is "+std::to_string(node_count));
				if(arc_count < 0)
					throw std::runtime_error("half_arc_count must be non-negative; it is "+std::to_string(arc_count));
				arc_count *= 2;

				std::string has_weight_num;
				if(line_in >> has_weight_num){
					if(has_weight_num == "001" || has_weight_num == "1"){
						has_arc_weights = true;
					}else if(has_weight_num == "000" || has_weight_num == "0"){
						has_arc_weights = false;
					}else if(has_weight_num == "010"){
						has_node_weights = true;
					}else if(has_weight_num == "011"){
						has_node_weights = true;
						has_arc_weights = true;
					}else
						throw std::runtime_error("The has_weight parameter in the header must be 0 or 1.");

					std::string ignore;
					if(line_in >> ignore)
						throw std::runtime_error("Header must only contain a 2 or 3 integers");
				}

				
				g = ListGraph(node_count, arc_count);
			}else{
				if(next_node_id == node_count)
					throw std::runtime_error("More nodes than claimed in the header");
				int x;

				if(has_node_weights){
					if(!(line_in >> g.node_weight[next_node_id])){
						throw std::runtime_error("Cannot read node weight");
					}
				}

				while(line_in >> x){
					int weight = 1;
					if(has_arc_weights){
						if(!(line_in >> weight))
							throw std::runtime_error("Missing weight for arc");
					}

					if(next_arc_id == arc_count)
						throw std::runtime_error("More arcs than claimed in the header");
					
					--x;
					g.head[next_arc_id] = x;
					g.tail[next_arc_id] = next_node_id;
					g.arc_weight[next_arc_id] = weight;
					++next_arc_id;
				}
				++next_node_id;
			}
		}

	}catch(std::runtime_error& err){
		throw std::runtime_error(std::string(err.what()) + " in line "+std::to_string(line_num));
	}
	if(next_node_id != node_count)
		throw std::runtime_error("Less nodes than claimed in the header");
	if(next_arc_id != arc_count)
		throw std::runtime_error("Less arcs than claimed in the header");

	if(!has_node_weights)
		g.node_weight.fill(1);

	if(!is_symmetric(g.tail, g.head))
		throw std::runtime_error("The graph in the file is not symmetric");

	return g; // NVRO
}

ListGraph load_metis_graph(const std::string&file_name){
	return load_cached_text_file(file_name, "metis", load_metis_graph_impl, load_binary_graph_impl, 
		[](std::ostream&out, const ListGraph&g){
			save_binary_graph_impl(out, g.tail, g.head, g.node_weight, g.arc_weight);
		}
	);
}

ListGraph uncached_load_metis_graph(const std::string&file_name){
	return load_uncached_text_file(file_name, load_metis_graph_impl);
}


static
ListGraph load_pace_graph_impl(std::istream&in){
	ListGraph graph;
	std::string line;
	int line_num = 0;
	int next_arc = 0;

	bool was_header_read = false;
	while(std::getline(in, line)){
		++line_num;
		if(line.empty() || line[0] == 'c')
			continue;

		std::istringstream lin(line);
		if(!was_header_read){
			was_header_read = true;
			std::string p, sp;
			int node_count;
			int arc_count;
			if(!(lin >> p >> sp >> node_count >> arc_count))
				throw std::runtime_error("Can not parse header in pace file.");
			if(p != "p" || sp != "tw" || node_count < 0 || arc_count < 0)
				throw std::runtime_error("Invalid header in pace file.");
			graph = ListGraph(node_count, 2*arc_count);
		}else{
			int h, t;
			if(!(lin >> t >> h))
				throw std::runtime_error("Can not parse line num "+std::to_string(line_num)+" \""+line+"\" in pace file.");
			--h;
			--t;
			if(h < 0 || h >= graph.node_count() || t < 0 || t >= graph.node_count())
				throw std::runtime_error("Invalid arc in line num "+std::to_string(line_num)+" \""+line+"\" in pace file.");
			if(next_arc < graph.arc_count()){
				graph.head[next_arc] = h;
				graph.tail[next_arc] = t;
				graph.arc_weight[next_arc] = 0;
			}
			++next_arc;
			if(next_arc < graph.arc_count()){
				graph.head[next_arc] = t;
				graph.tail[next_arc] = h;
				graph.arc_weight[next_arc] = 0;
			}
			++next_arc;
		}
	}

	if(next_arc != graph.arc_count())
		throw std::runtime_error("The arc count in the header ("+std::to_string(graph.arc_count())+") does not correspond with the actual number of arcs ("+std::to_string(next_arc)+").");

	graph.node_weight.fill(0);

	return graph; // NVRO
}

ListGraph load_pace_graph(const std::string&file_name){
	return load_cached_text_file(file_name, "color_dimacs", load_pace_graph_impl, load_binary_graph_impl, 
		[](std::ostream&out, const ListGraph&g){
			save_binary_graph_impl(out, g.tail, g.head, g.node_weight, g.arc_weight);
		}
	);
}


ListGraph uncached_load_pace_graph(const std::string&file_name){
	return load_uncached_text_file(file_name, load_pace_graph_impl);
}


static
void save_pace_graph_impl(std::ostream&out, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head){
	int node_count = head.image_count(), arc_count = head.preimage_count();

	auto out_arc = invert_id_id_func(tail);
			
	out <<"p tw "<< node_count << ' ' << arc_count/2 << '\n';
	for(int xy=0; xy<arc_count; ++xy){
		int x = tail(xy), y = head(xy);	
		if(x <= y)
			out << (x+1) << ' ' << (y+1) << '\n';
	}
}

void save_pace_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head){
	if(!is_symmetric(tail, head))
		throw std::runtime_error("Pace graph format can only store symmetric graphs");
	save_text_file(file_name, save_pace_graph_impl, tail, head);
}

