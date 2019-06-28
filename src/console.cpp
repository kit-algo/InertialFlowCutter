#include "geo_pos.h"
#include "timer.h"

#include "io_helper.h"
#include "id_func.h"
#include "multi_arc.h"
#include "sort_arc.h"
#include "chain.h"
#include "connected_components.h"
#include "id_multi_func.h"
#include "preorder.h"
#include "id_string.h"
#include "csv.h"
#include "min_max.h"
#include "contraction_graph.h"
#include "node_flow_cutter.h"
#include "triangle_count.h"
#include "separator.h"
#include "vector_io.h"
#include "inverse_vector.h"
#include "min_fill_in.h"
#include "inertial_flow.h"
#include "list_graph.h"

#ifdef USE_KAHIP
#include "my_kahip.h"
#endif

#include <string>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <random>
#include <functional>
#include <stack>
#include <tbb/task_scheduler_init.h>

using namespace std;

ArrayIDIDFunc tail, head;
ArrayIDFunc<int>node_weight, arc_weight;

ArrayIDIDFunc node_color, arc_color;
ArrayIDFunc<GeoPos> node_geo_pos;

ArrayIDIDFunc node_original_position;

void check_graph_consitency(){
	#ifndef NDEBUG
	const int node_count = tail.image_count(), arc_count = tail.preimage_count();
	assert(tail.preimage_count() == arc_count);
	assert(tail.image_count() == node_count);
	assert(head.preimage_count() == arc_count);
	assert(head.image_count() == node_count);
	assert(node_weight.preimage_count() == node_count);
	assert(arc_weight.preimage_count() == arc_count);
	assert(node_color.preimage_count() == node_count);
	assert(arc_color.preimage_count() == arc_count);
	assert(node_geo_pos.preimage_count() == node_count);

	assert(node_original_position.preimage_count() == node_count);

	for(auto x:node_color)
		assert(0<= x && x < node_color.image_count());
	for(auto x:arc_color)
		assert(0<= x && x < arc_color.image_count());
	#endif
}

stack<ArrayIDIDFunc>node_color_stack;

flow_cutter::Config flow_cutter_config;

bool show_arc_ids = false;
bool show_undirected = false;
bool time_commands = false;

int select_color(string str){
	if(str == "most_frequent_node_color")
		return max_histogram_id(compute_histogram(node_color));
	else if(str == "most_frequent_arc_color")
		return max_histogram_id(compute_histogram(arc_color));
	else if(str == "least_frequent_node_color")
		return min_histogram_id(compute_histogram(node_color));
	else if(str == "least_frequent_arc_color")
		return min_histogram_id(compute_histogram(arc_color));
	else
		return stoi(str);
}
/*
static
void keep_nodes_if(const BitIDFunc&node_keep_flag){
	int new_node_count = count_true(node_keep_flag);

	BitIDFunc arc_keep_flag(tail.preimage_count());
	for(int i=0; i<tail.preimage_count(); ++i)
		arc_keep_flag.set(i, node_keep_flag(tail(i)) && node_keep_flag(head(i)));
	int new_arc_count = count_true(arc_keep_flag);

	tail = keep_if(arc_keep_flag, new_arc_count, move(tail));
	head = keep_if(arc_keep_flag, new_arc_count, move(head));
	arc_weight = keep_if(arc_keep_flag, new_arc_count, move(arc_weight));
	arc_color = keep_if(arc_keep_flag, new_arc_count, move(arc_color));

	auto node_keep_perm = compute_keep_function(node_keep_flag, new_node_count);
	head = chain(std::move(head), node_keep_perm);
	tail = chain(std::move(tail), node_keep_perm);
	node_color = keep_if(node_keep_flag, new_node_count, std::move(node_color));
	node_geo_pos = keep_if(node_keep_flag, new_node_count, std::move(node_geo_pos));
	node_weight = keep_if(node_keep_flag, new_node_count, std::move(node_weight));
	node_original_position = keep_if(node_keep_flag, new_node_count, std::move(node_original_position));
}
*/
static
void permutate_nodes(const ArrayIDIDFunc&p){
	auto inv_p = inverse_permutation(p);
	head = chain(std::move(head), inv_p);
	tail = chain(std::move(tail), inv_p);

	node_color = chain(p, std::move(node_color));
	node_geo_pos = chain(p, std::move(node_geo_pos));
	node_weight = chain(p, std::move(node_weight));
	node_original_position = chain(p, std::move(node_original_position));
}

static
void keep_arcs_if(const BitIDFunc&keep_flag){
	int new_arc_count = count_true(keep_flag);
	tail = keep_if(keep_flag, new_arc_count, move(tail));
	head = keep_if(keep_flag, new_arc_count, move(head));
	arc_weight = keep_if(keep_flag, new_arc_count, move(arc_weight));
	arc_color = keep_if(keep_flag, new_arc_count, move(arc_color));

}

static
void permutate_arcs(const ArrayIDIDFunc&p){
	tail = chain(p, move(tail));
	head = chain(p, move(head));
	arc_weight = chain(p, move(arc_weight));
	arc_color = chain(p, move(arc_color));

}

struct Command{
	string name;
	int parameter_count;
	string description;
	function<void(vector<string>)>func;

	Command(string name, int parameter_count, string description, function<void(vector<string>)>func):
		name(move(name)),
		parameter_count(parameter_count),
		description(move(description)),
		func(move(func)){}

	Command(string name, string description, function<void(void)>func):
		name(move(name)),
		parameter_count(0),
		description(move(description)),
		func([=](vector<string>){func();}){
	}
};

#include "fancy_input.h"

auto w = setw(30);

vector<Command>cmd = {
	{
		"get_tmp_dir",
		"Returns the path to the temporary files directory",
		[]{
			cout << get_temp_directory_path() << endl;
		}
	},
	{
		"help",
		"It prints a list of all commands.",
		[]{
			cout << "The following commands exists: \n\n";
			for(auto&c:cmd){
				cout << " * " << c.name;
				if(c.parameter_count != 0)
					cout << " ("<<c.parameter_count << " args)";
				cout << '\n';
			}

			cout << "\nUse the details command to get a detailed description of each command."<<endl;
		}
	},
	{
		"details", 1,
		"It prints a detailed description of the command given as parameter.",
		[](vector<string>args){
			for(auto&c:cmd)
				if(c.name == args[0]){
					cout << "Command "<<c.name << " takes "<< c.parameter_count << " parameters. " << c.description <<  endl;
					return;
				}
			cout << "Command "<<args[0] << " was not found." << endl;
		}
	},
	{
		"interactive",
		"It takes command from stdin and processes them.",
		[]{
			#ifdef __OPTIMIZE__
			cout << "Compiler optimizations are on" << endl;
			#else
			cout << "Compiler optimizations are off" << endl;
			#endif
			#ifndef NDEBUG
			cout << "Asserts are on" << endl;
			#else
			cout << "Asserts are off" << endl;
			#endif


			string line;
			vector<string>cmd_list;
			for(auto&x:cmd)
				cmd_list.push_back(x.name);
			cmd_list.push_back("exit");
			set_autocomplete_command_list(move(cmd_list));

			while(get_command_line(line)){
				try{
					istringstream line_in(line);
					string command;
					line_in >> command;

					if(command == "exit")
						break;

					int c = -1;
					for(int i=0; i<(int)cmd.size(); ++i)
						if(cmd[i].name == command){
							c = i;
							break;
						}
					if(c == -1)
						throw runtime_error("Unknown command "+command);

					vector<string>args;
					string x;
					while(line_in >> x)
						args.push_back(x);
					if((int)args.size() != cmd[c].parameter_count)
						throw runtime_error("Wrong number of parameters to command "+cmd[c].name+". expected:"+to_string(cmd[c].parameter_count)+", got:"+to_string(args.size()));

					auto prev_time_commands = time_commands;
					long long time = -get_micro_time();
					cmd[c].func(move(args));
					time += get_micro_time();

					if(time_commands && prev_time_commands){
						cout << "running time : "<<time << "musec" << endl;
					}

					check_graph_consitency();
				}catch(std::exception&err){
					cout << "Exception : " << err.what() << endl;
				}
			}
			cout << endl;
		}
	},
	{
		"report_time",
		"Report the running time of every command",
		[]{
			time_commands = true;
		}
	},
	{
		"do_not_report_time",
		"Do not report the running time of every command",
		[]{
			time_commands = false;
		}
	},
	{
		"examine_chordal_supergraph",
		"Examines the chordal supergraph produced by contracting the nodes increasing by ID",
		[]{
			if(!is_symmetric(tail, head))
				throw runtime_error("Graph must be symmetric");
			const int node_count = tail.image_count();

			if(node_count <= 1)
				throw runtime_error("Graph must have at least 2 nodes");

			int super_graph_arc_count = 0;
			int current_tail = -1;
			int current_tail_up_deg = 0;
			int max_up_deg = 0;
			ArrayIDFunc<int>
				out_deg(node_count),
				parent(node_count);
			out_deg.fill(0);
			parent.fill(std::numeric_limits<int>::max());
			compute_chordal_supergraph(
				tail, head,
				[&](int x, int y){
					++out_deg[x];
					if(current_tail != x){
						current_tail = x;
						max_to(max_up_deg, current_tail_up_deg);
						current_tail_up_deg = 0;
					}
					++super_graph_arc_count;
					++current_tail_up_deg;
					min_to(parent[x], y);
				}
			);

			ArrayIDFunc<int>ancestor_count(node_count);
			ancestor_count.fill(-1);

			for(int x=0; x<node_count; ++x){
				if(ancestor_count(x) == -1){
					int n = 0;
					for(int y = x; y != std::numeric_limits<int>::max(); y = parent(y)){
						if(ancestor_count(y) == -1){
							++n;
						}else{
							n += ancestor_count(y);
							break;
						}
					}
					for(int y = x; y != std::numeric_limits<int>::max(); y = parent(y)){
						if(ancestor_count(y) != -1){
							assert(ancestor_count(y) == n);
							break;
						}else{
							ancestor_count[y] = n;
							--n;
						}
					}
				}
			}

			int max_ancestor_count = 0;
			long long ancestor_count_sum = 0;
			for(auto x:ancestor_count){
				max_to(max_ancestor_count, x);
				ancestor_count_sum += x;
			}

			ArrayIDFunc<int>arcs_in_search_space = out_deg;

			for(int x=node_count-1; x>=0; --x){
				if(parent(x) != std::numeric_limits<int>::max()){
					arcs_in_search_space[x] += arcs_in_search_space(parent(x));
				}
			}

			int max_arcs_in_search_space = 0;
			long long arcs_in_search_space_sum = 0;
			for(auto x:arcs_in_search_space){
				max_to(max_arcs_in_search_space, x);
				arcs_in_search_space_sum += x;
			}

			long long triangle_count = 0;
			for(int x=0; x<node_count; ++x){
				triangle_count += (out_deg(x)*(out_deg(x)-1))/2;
			}

			auto w = setw(35);

			cout
				<< w << "super_graph_upward_arc_count" << " : " << super_graph_arc_count << '\n'
				<< w << "upper tree width bound" << " : " << max_up_deg << '\n'
				<< w << "elimination tree height" << " : " << max_ancestor_count << '\n'
				<< w << "average elimination tree depth" << " : " << (static_cast<double>(ancestor_count_sum)/node_count) << '\n'
				<< w << "maximum arcs in search space" << " : " << max_arcs_in_search_space << '\n'
				<< w << "average arcs in search space" << " : " << (static_cast<double>(arcs_in_search_space_sum)/node_count) << '\n'
				<< w << "number of triangles in super graph" << " : " << triangle_count << endl;
		}
	},
	{
		"find_longest_elimination_tree_path",
		"Find the node IDs of the longest path leaf root path in the elimination tree",
		[]{
			if(!is_symmetric(tail, head))
				throw runtime_error("Graph must be symmetric");
			const int node_count = tail.image_count();

			if(node_count <= 1)
				throw runtime_error("Graph must have at least 2 nodes");

			ArrayIDFunc<int>parent(tail.image_count());
			parent.fill(std::numeric_limits<int>::max());

			compute_chordal_supergraph(
				tail, head,
				[&](int x, int y){
					min_to(parent[x], y);
				}
			);

			ArrayIDFunc<int>ancestor_count(tail.image_count());
			ancestor_count.fill(-1);

			for(int x=0; x<node_count; ++x){
				if(ancestor_count(x) == -1){
					int n = 0;
					for(int y = x; y != std::numeric_limits<int>::max(); y = parent(y)){
						if(ancestor_count(y) == -1){
							++n;
						}else{
							n += ancestor_count(y);
							break;
						}
					}
					for(int y = x; y != std::numeric_limits<int>::max(); y = parent(y)){
						if(ancestor_count(y) != -1){
							assert(ancestor_count(y) == n);
							break;
						}else{
							ancestor_count[y] = n;
							--n;
						}
					}
				}
			}

			int x = max_preimage_over_id_func(ancestor_count);

			BitIDFunc in_tree(node_count);
			in_tree.fill(false);

			while(parent(x) != std::numeric_limits<int>::max()){
				in_tree.set(x, true);
				x = parent(x);
			}
			in_tree.set(x, true);

			cout << make_id_string(in_tree) << endl;
		}
	},
	{
		"find_largest_clique_in_chordal_supergraph",
		"Examines the chordal supergraph produced by contracting the nodes increasing by ID",
		[]{
			if(!is_symmetric(tail, head))
				throw runtime_error("Graph must be symmetric");
			const int node_count = tail.image_count();

			if(node_count <= 1)
				throw runtime_error("Graph must have at least 2 nodes");


			BitIDFunc largest_clique(node_count);
			int largest_clique_size = 0;
			largest_clique.fill(false);

			BitIDFunc current_clique(node_count);
			int current_clique_size = 0;
			current_clique.fill(false);

			vector<int>nodes_in_current_clique;


			int current_tail = -1;
			compute_chordal_supergraph(
				tail, head,
				[&](int x, int y){
					if(current_tail != x){
						if(current_clique_size > largest_clique_size){
							largest_clique_size = current_clique_size;
							largest_clique = current_clique;
						}

						current_tail = x;
						for(auto z:nodes_in_current_clique)
							current_clique.set(z, false);
						current_clique.set(x, true);
						nodes_in_current_clique = {x};
						current_clique_size = 1;
					}
					current_clique.set(y, true);
					nodes_in_current_clique.push_back(y);
					++current_clique_size;
				}
			);
			cout << make_id_string(largest_clique) << endl;
		}
	},


	{
		"push_node_color",
		"Saves the current node colors",
		[]{
			node_color_stack.push(node_color);
		}
	},
	{
		"pop_node_color",
		"Loads the last stored node colors",
		[]{
			if(node_color_stack.empty())
				throw runtime_error("No node colors were saved");
			auto x = std::move(node_color_stack.top());
			node_color_stack.pop();
			if(x.preimage_count() != node_color.preimage_count())
				cout << "Discarding top node colors because the number of nodes do not match" << endl;
			else
				x.swap(node_color);
		}
	},
	{
		"save_node_color", 2,
		"Saves the current node colors (arg1) to file (arg2). arg1 can be \"all\"",
		[](vector<string>arg){
			BitIDFunc save_color(node_color.image_count());
			if(arg[0] == "all")
				save_color.fill(true);
			else{
				save_color.fill(false);
				forall_in_id_string(
					arg[0],
					[&](int x){
						if(x < 0 || x >= node_color.image_count())
							throw runtime_error("Node color "+to_string(x)+" is out of bounds");
						save_color.set(x, true);
					}
				);
			}
			save_text_file(
				arg[1],
				[&](ostream&out){
					out << "node_id,color\n";
					for(int i=0; i<node_color.preimage_count(); ++i)
						if(save_color(node_color(i)))
							out << i <<',' << node_color(i) << '\n';
				}
			);
		}
	},
	{
		"load_node_color", 1,
		"Loads all node colors stored in file arg1",
		[](vector<string>arg){
			io::CSVReader<2>in(arg[0]);
			in.read_header(io::ignore_extra_column, "node_id", "color");
			int node_id, color;
			while(in.read_row(node_id, color)){
				if(color < 0)
					throw runtime_error("invalid color id "+to_string(color));
				if(color > node_color.image_count())
					node_color.set_image_count(color+1);
				if(node_id < 0 || node_id >= node_color.preimage_count())
					throw runtime_error("invalid node id "+to_string(node_id));
				node_color[node_id] = color;
			}
		}
	},
	{
		"map_node_color_id", 2,
		"All nodes with color (arg1) get assigned color (arg2).",
		[](vector<string>arg){
			int new_color = select_color(arg[1]), old_color = select_color(arg[0]);
			if(new_color >= node_color.image_count())
				node_color.set_image_count(new_color+1);
			for(int i=0; i<tail.image_count(); ++i)
				if(node_color(i) == old_color)
					node_color[i] = new_color;
		}
	},
	{
		"load_binary_graph", 1,
		"Loads a graph in the binary format.",
		[](vector<string>args){
			auto graph = load_binary_graph(args[0]);
			tail = std::move(graph.tail);
			head = std::move(graph.head);
			node_weight = std::move(graph.node_weight);
			arc_weight = std::move(graph.arc_weight);

			node_color = ArrayIDIDFunc(tail.image_count(), 1);
			node_color.fill(0);
			node_geo_pos = ArrayIDFunc<GeoPos>(tail.image_count());
			node_geo_pos.fill({0.0, 0.0});
			node_original_position = identity_permutation(tail.image_count());
			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},
	{
		"load_dimacs_graph", 1,
		"Loads a graph in the DIMACS format. All node weights are set to 1.",
		[](vector<string>args){
			auto graph = load_dimacs_graph(args[0]);
			tail = std::move(graph.tail);
			head = std::move(graph.head);
			node_weight = std::move(graph.node_weight);
			arc_weight = std::move(graph.arc_weight);

			node_color = ArrayIDIDFunc(tail.image_count(), 1);
			node_color.fill(0);
			node_geo_pos = ArrayIDFunc<GeoPos>(tail.image_count());
			node_geo_pos.fill({0.0, 0.0});
			node_original_position = identity_permutation(tail.image_count());
			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},

	{
		"compute_chordal_supergraph",
		"Computes the chordal supergraph and replaces the current graph.",
		[]{

			std::vector<int>new_tail, new_head;
			compute_chordal_supergraph(
				tail, head,
				[&](int x, int y){
					new_tail.push_back(x);
					new_head.push_back(y);
				}
			);

			tail = id_id_func(new_tail.size(), tail.image_count(), [&](int x){return new_tail[x];});
			head = id_id_func(new_head.size(), head.image_count(), [&](int x){return new_head[x];});

			node_weight = id_func(tail.image_count(), [](int){return 1;});
			arc_weight = id_func(tail.preimage_count(), [](int){return 1;});

			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},
	{
		"load_color_dimacs_graph", 1,
		"Loads a graph in the DIMACS format. All node weights are set to 1.",
		[](vector<string>args){
			auto graph = load_color_dimacs_graph(args[0]);
			tail = std::move(graph.tail);
			head = std::move(graph.head);
			node_weight = std::move(graph.node_weight);
			arc_weight = std::move(graph.arc_weight);

			node_color = ArrayIDIDFunc(tail.image_count(), 1);
			node_color.fill(0);
			node_geo_pos = ArrayIDFunc<GeoPos>(tail.image_count());
			node_geo_pos.fill({0.0, 0.0});
			node_original_position = identity_permutation(tail.image_count());
			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},
	{
		"load_metis_graph", 1,
		"Loads a graph in the metis format.",
		[](vector<string>args){
			auto graph = load_metis_graph(args[0]);
			tail = std::move(graph.tail);
			head = std::move(graph.head);
			node_weight = std::move(graph.node_weight);
			arc_weight = std::move(graph.arc_weight);

			node_color = ArrayIDIDFunc(tail.image_count(), 1);
			node_color.fill(0);
			node_geo_pos = ArrayIDFunc<GeoPos>(tail.image_count());
			node_geo_pos.fill({0.0, 0.0});
			node_original_position = identity_permutation(tail.image_count());
			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},
	{
		"load_routingkit_unweighted_graph", 2,
		"Loads a graph in the RoutingKit first_out/head format",
		[](vector<string>args){
			auto first_out = load_vector<unsigned>(args[0]);
			int node_count = first_out.size()-1;
			int arc_count = first_out.back();
			auto v_tail = invert_inverse_vector(first_out);
			auto v_head = load_vector<unsigned>(args[1]);

			tail = id_id_func(arc_count, node_count, [&](int i)->int{return v_tail[i];});
			head = id_id_func(arc_count, node_count, [&](int i)->int{return v_head[i];});
			node_weight = ArrayIDFunc<int>(node_count);
			node_weight.fill(0);
			arc_weight = ArrayIDFunc<int>(arc_count);
			arc_weight.fill(0);

			node_color = ArrayIDIDFunc(tail.image_count(), 1);
			node_color.fill(0);
			node_geo_pos = ArrayIDFunc<GeoPos>(tail.image_count());
			node_geo_pos.fill({0.0, 0.0});
			node_original_position = identity_permutation(tail.image_count());
			arc_color = ArrayIDIDFunc(tail.preimage_count(), 1);
			arc_color.fill(0);
		}
	},

	{
		"save_routingkit_unweighted_graph", 2,
		"Saves a graph in the RoutingKit first_out/head format",
		[](vector<string>args){

			if(!is_sorted(tail.begin(), tail.end())){
				throw std::runtime_error("Tails must be sorted");
			}

			const unsigned node_count = tail.image_count();
			const unsigned arc_count = tail.preimage_count();

			vector<unsigned>v_tail(arc_count);
			for(unsigned i=0; i<arc_count; ++i)
				v_tail[i] = tail(i);

			vector<unsigned>v_head(arc_count);
			for(unsigned i=0; i<arc_count; ++i)
				v_head[i] = head(i);

			vector<unsigned>first_out = invert_vector(v_tail, node_count);

			save_vector(args[0], first_out);
			save_vector(args[1], v_head);
		}
	},

	{
		"load_routingkit_arc_weight", 1,
		"Loads an arc weight in the RoutingKit travel_time/geo_dist format",
		[](vector<string>args){
			auto weight = load_vector<unsigned>(args[0]);
			int arc_count = tail.preimage_count();
			if(arc_count != (int)weight.size())
				throw std::runtime_error("weight vector does not have a length equal to the number of arcs");

			for(int i=0; i<arc_count; ++i)
				arc_weight[i] = weight[i];
		}
	},

	{
		"save_routingkit_arc_weight", 1,
		"Save an arc weight in the RoutingKit travel_time/geo_dist format",
		[](vector<string>args){
			const unsigned arc_count = arc_weight.preimage_count();
			vector<unsigned>weight(arc_count);
			for(unsigned i=0; i<arc_count; ++i)
				weight[i] = arc_weight(i);
			save_vector(args[0], weight);
		}
	},
	{
		"load_routingkit_longitude", 1,
		"Loads the node longitudes from the RoutingKit format",
		[](vector<string>args){
			auto lon = load_vector<float>(args[0]);
			int node_count = tail.image_count();
			if(node_count != (int)lon.size())
				throw std::runtime_error("weight vector does not have a length equal to the number of nodes");

			for(int i=0; i<node_count; ++i)
				node_geo_pos[i].lon = lon[i];
		}
	},
	{
		"load_routingkit_latitude", 1,
		"Loads the node latitudes from the RoutingKit format",
		[](vector<string>args){
			auto lat = load_vector<float>(args[0]);
			int node_count = tail.image_count();
			if(node_count != (int)lat.size())
				throw std::runtime_error("weight vector does not have a length equal to the number of nodes");

			for(int i=0; i<node_count; ++i)
				node_geo_pos[i].lat = lat[i];
		}
	},
	{
		"save_routingkit_longitude", 1,
		"Saves the node longitudes in the RoutingKit format",
		[](vector<string>args){
			int node_count = tail.image_count();
			vector<float> lon(node_count);

			for(int i=0; i<node_count; ++i)
				lon[i] = node_geo_pos[i].lon;

			save_vector(args[0], lon);
		}
	},
	{
		"save_routingkit_latitude", 1,
		"Saves the node latitudes in the RoutingKit format",
		[](vector<string>args){
			int node_count = tail.image_count();
			vector<float> lat(node_count);

			for(int i=0; i<node_count; ++i)
				lat[i] = node_geo_pos[i].lat;

			save_vector(args[0], lat);
		}
	},
	{
		"save_routingkit_node_permutation_since_last_load", 1,
		"Save a node order in the praktikum 2015/2016 order format",
		[](vector<string>args){

			if(node_original_position.image_count() != node_original_position.preimage_count())
				throw runtime_error("Not possible because the node count got modified");

			std::vector<unsigned>order(tail.image_count());
			for(unsigned i=0; i<order.size(); ++i)
				order[i] = node_original_position[i];
			save_vector(args[0], order);
		}
	},
	{
		"load_dimacs_geo_pos", 1,
		"Loads geo positions in the DIMACS format.",
		[](vector<string>args){
			auto new_geo_pos = load_dimacs_geo_pos(args[0]);
			if(new_geo_pos.preimage_count() != node_geo_pos.preimage_count())
				throw runtime_error("The number of nodes in the geo pos file and in the current graph differ");
			node_geo_pos = std::move(new_geo_pos);
		}
	},
	{
		"load_binary_geo_pos", 1,
		"Loads geo positions in the DIMACS format.",
		[](vector<string>args){
			auto new_geo_pos = load_binary_geo_pos(args[0]);
			if(new_geo_pos.preimage_count() != node_geo_pos.preimage_count())
				throw runtime_error("The number of nodes in the geo pos file and in the current graph differ");
			node_geo_pos = std::move(new_geo_pos);
		}
	},
	{
		"permutate_nodes", 1,
		"Permutes the nodes according to a permutation stored in file arg1",
		[](vector<string>args){
			auto perm = load_permutation(args[0]);
			if(perm.preimage_count() != tail.image_count())
				throw runtime_error("Permutation does not permutate node count elements");
			permutate_nodes(perm);
		}
	},
	{
		"permutate_nodes_routingkit", 1,
		"Permutes the nodes according to a permutation stored in file arg1. Takes a binary vector (Routingkit format) file as opposed to permutate_nodes.",
		[](vector<string>args){
			auto perm_vec = load_vector<unsigned>(args[0]);
			for (const auto& x : perm_vec)
				if (x >= perm_vec.size())
					throw std::runtime_error("Invalid node order. Node ID " + std::to_string(x) + " >= number of nodes " + std::to_string(perm_vec.size()) + " found.");
			auto perm = ArrayIDIDFunc((int)perm_vec.size(), (int)perm_vec.size());
			for (int i = 0; i < (int)perm_vec.size(); ++i)
				perm[i] = static_cast<int>(perm_vec[i]);
			if(perm.preimage_count() != tail.image_count())
				throw runtime_error("Permutation does not permutate node count elements");
			//permutate_nodes(inverse_permutation(perm));
			permutate_nodes(perm);
		}
	},
	{
			"text_to_binary_vector", 2,
			"Loads a vector of unsigned ints in text format [first arg] and converts it to binary format [second arg = new filename]",
			[](vector<string>args){
				std::vector<unsigned> input;
				load_uncached_text_file(
						args[0],
						[&](std::istream&in){
							unsigned x;
							while (in >> x) {
								input.push_back(x);
							}
						}
				);
				for (auto i : input) { if (i >= input.size()) throw std::runtime_error("too large node id"); }
				save_vector<unsigned>(args[1], input);
			}
	},
	{
			"binary_to_text_vector", 2,
			"Loads a vector of unsigned ints in binary format [first arg] and converts it to text format [second arg = new filename]",
			[](vector<string>args){
				std::vector<unsigned> input = load_vector<unsigned>(args[0]);
				save_text_file(
						args[1],
						[&](ostream&out){
							for(auto i : input)
								out << i << "\n";
							out << std::flush;
						}
				);
			}
	},
	{
		"save_permutation_of_nodes_since_last_file_load", 1,
		"Save the permutation of the nodes sind the last time a graph was loaded into arg1",
		[](vector<string>args){
			if(node_original_position.image_count() != node_original_position.preimage_count())
				throw runtime_error("Not possible because the node count got modified");
			save_permutation(args[0], node_original_position);
		}
	},


	{
		"swap_geo_pos",
		"Exchanges the geo_pos coordinates",
		[]{
			for(int i=0; i<tail.image_count(); ++i)
				std::swap(node_geo_pos[i].lon, node_geo_pos[i].lat);
		}
	},
	{
		"save_binary_graph", 1,
		"Saves a graph in the binary format.",
		[](vector<string>args){
			save_binary_graph(args[0], tail, head, node_weight, arc_weight);
		}
	},
	{
		"save_dimacs_graph", 1,
		"Saves a weighted graph in the DIMACS format.",
		[](vector<string>args){
			save_dimacs_graph(args[0], tail, head, arc_weight);
		}
	},
	{
		"save_pace_graph", 1,
		"Saves an unweighted graph in the PACE'16 format.",
		[](vector<string>args){
			save_pace_graph(args[0], tail, head);
		}
	},
	{
		"save_ddsg_graph", 1,
		"TODO: write me",
		[](vector<string>args){
			save_ddsg_graph(args[0], tail, head, arc_weight);
		}
	},
	{
		"save_metis_graph", 1,
		"Saves a graph in the metis graph format.",
		[](vector<string>args){
			save_metis_graph(args[0], tail, head, arc_weight);
		}
	},
	{
		"save_dimacs_geo_pos", 1,
		"Saves geo positions in the DIMACS format.",
		[](vector<string>args){
			save_dimacs_geo_pos(args[0], node_geo_pos);
		}
	},
	{
		"save_binary_geo_pos", 1,
		"Saves geo positions in the DIMACS format.",
		[](vector<string>args){
			save_binary_geo_pos(args[0], node_geo_pos);
		}
	},
	{
		"save_csv_graph", 1,
		"Saves a weighted graph in the CSV format.\nWarning: The node count is lost!\nWarning: All node weights are lost!",
		[](vector<string>args){
			save_csv_graph(args[0], tail, head, arc_weight);
		}
	},

	/*

	{
		"save_kml_places", 2,
		"Saves a KML document (arg2) with several nodes (arg1) highlighted.",
		[](vector<string>arg){
			save_text_file(
				arg[1],
				[&](ostream&out){
					out <<
						"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
						"<Document>\n";

					forall_in_id_string(
						arg[0],
						tail.image_count(),
						[&](int x){
							out <<
									"<Placemark><name>node_id = "<<x<<"</name><Point><coordinates>"
									<<node_geo_pos(x).lon << ","<<node_geo_pos(x).lat << ",0"
									"</coordinates></Point></Placemark>\n";
						}
					);

					out << "</Document></kml>\n";
				}

			);
		}
	},
	{
		"save_kml_graph", 1,
		"Saves a graph using KML.",
		[](vector<string>arg){
			save_text_file(
				arg[0],
				[](ostream&out){
					out <<
						"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
						"<Document>\n";
					for(int i=0; i<tail.preimage_count(); ++i)
						out <<
							"<Placemark><name>arc_id = "<<i<<"</name><LineString><coordinates>\n"
							<<node_geo_pos(tail(i)).lon << ","<<node_geo_pos(tail(i)).lat << ",0\n"
							<<node_geo_pos(head(i)).lon << ","<<node_geo_pos(head(i)).lat << ",0\n"
							"</coordinates></LineString></Placemark>\n";
					out << "</Document></kml>\n";
				}

			);
		}
	},
	{
		"save_kml_cut2", 2,
		"Saves a cut (arg1) using KML in file arg2",
		[](vector<string>arg){
			std::vector<GeoPos>points;
			forall_in_id_string(arg[0],
				[&](int xy){
					if(xy < 0 || xy >= tail.preimage_count())
						throw runtime_error("arc id "+to_string(xy)+" out of bounds");
					points.push_back(mid_geo_pos(node_geo_pos[tail(xy)], node_geo_pos[head(xy)]));
				}
			);

			points = order_geo_positions_along_line(std::move(points));


			save_text_file(
				arg[1],
				[&](ostream&out){
					out <<
						"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
						"<Document><name>cut_size = " << points.size() << "</name>\n"
						"<Placemark><LineString><coordinates>\n";
					for(auto p:points)
							out << p.lon << "," << p.lat << ",0\n";
					out <<
						"</coordinates></LineString></Placemark>\n"
						"</Document></kml>\n";
				}

			);
		}
	},
	{
		"kml_show_cut_fronts", 2,
		"Saves a sequence of cuts in file arg1 using KML in file arg2",
		[](vector<string>arg){


			io::CSVReader<1>in(arg[0]);
			in.read_header(io::ignore_extra_column, "cut");


			save_text_file(
				arg[1],
				[&](ostream&out){
					out <<
						"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
						"<Document>\n";

					std::string cut;
					while(in.read_row(cut)){
						std::vector<GeoPos>points;
						forall_in_id_string(cut,
							[&](int xy){
								if(xy < 0 || xy >= tail.preimage_count())
									throw runtime_error("arc id "+to_string(xy)+" out of bounds");
								points.push_back(mid_geo_pos(node_geo_pos[tail(xy)], node_geo_pos[head(xy)]));
							}
						);

						points = order_geo_positions_along_line(std::move(points));

						out << "<Placemark><LineString><coordinates>\n";
						for(auto p:points)
								out << p.lon << "," << p.lat << ",0\n";
						out << "</coordinates></LineString></Placemark>\n";
					}
					out <<
						"</Document></kml>\n";
				}

			);
		}
	},
	{
		"save_kml_cut", 2,
		"Saves a cut (arg1) using KML in file arg2",
		[](vector<string>arg){
			struct CutLine{
				CutLine(){}
				CutLine(GeoPos a, GeoPos b, int arc_id):
					arc_id(arc_id){
					if(b.lat < a.lat)
						std::swap(a, b);
					left_end = a;
					right_end = b;
					mid = {(a.lat+b.lat)/2, (a.lon+b.lon)/2};
				}

				GeoPos left_end, right_end, mid;
				int arc_id;

			};

			std::vector<CutLine>cut_line;
			forall_in_id_string(arg[0],
				[&](int xy){
					if(xy < 0 || xy >= tail.preimage_count())
						throw runtime_error("arc id "+to_string(xy)+" out of bounds");
					cut_line.push_back({node_geo_pos[tail(xy)], node_geo_pos[head(xy)], xy});
				}
			);
			std::sort(cut_line.begin(), cut_line.end(),
				[](CutLine l, CutLine r){
					return l.mid.lon < r.mid.lon;
				}
			);
			cut_line.erase(std::unique(cut_line.begin(), cut_line.end(),
				[](CutLine l, CutLine r){
					return l.mid.lon == r.mid.lon;
				}), cut_line.end()
			);

			save_text_file(
				arg[1],
				[&](ostream&out){
					out <<
						"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
						"<Document><name>cut_size = " << cut_line.size() << "</name>";
					for(auto l:cut_line)
						out << "<Placemark><name>arc_id = " << l.arc_id << "</name><Point><coordinates>" << l.mid.lon << "," << l.mid.lat << ",0</coordinates></Point></Placemark>\n";
					for(auto l:cut_line)
						out <<
							"<Placemark><LineString><coordinates>\n"
							<<l.left_end.lon << ","<<l.left_end.lat << ",0\n"
							<<l.right_end.lon << ","<<l.right_end.lat << ",0\n"
							"</coordinates></LineString></Placemark>\n";
					out << "</Document></kml>\n";
				}

			);
		}
	},
	 */
	{
		"assign_constant_arc_weights", 1,
		"Sets all arc weights to a constant",
		[](vector<string>args){
			arc_weight.fill(stoi(args[0]));
		}
	},
	{
		"assign_triangle_arc_weights",
		"Sets all arc weights to a constant",
		[]{
			arc_weight = count_arc_triangles(tail, head);
		}
	},
	{
		"assign_geo_distance_arc_weights",
		"Assigns to every arc the geographic straigt line distance between its end points.",
		[]{
			for(int i=0; i<arc_weight.preimage_count(); ++i)
				arc_weight[i] = geo_dist(node_geo_pos(tail(i)), node_geo_pos(head(i)));
		}
	},
	{
		"assign_constant_node_weights", 1,
		"Sets all arc weights to a constant",
		[](vector<string>args){
			node_weight.fill(stoi(args[0]));
		}
	},
	{
		"stats",
		"Prints basic stats about the current graph",
		[]{
			cout
				<< w << "node_count" << " : " << tail.image_count() << endl
				<< w << "arc_count" << " : " << tail.preimage_count() << endl
				<< w << "node_color_count" << " : " << node_color.image_count() << endl
				<< w << "arc_color_count" << " : " << arc_color.image_count() << endl;
		}
	},
	{
		"reverse",
		"Reverses all arcs",
		[]{
			head.swap(tail);
		}
	},
	{
		"remove_loops",
		"Removes all loops",
		[]{
			keep_arcs_if(id_func(tail.preimage_count(), [](int i){return head(i) != tail(i);}));
		}
	},
	{
		"remove_multi_arcs",
		"Removes all multi arcs",
		[]{
			keep_arcs_if(identify_non_multi_arcs(tail, head));
		}
	},
	{
		"sort_arcs",
		"Sort arcs first by tail then by head, reassigning all IDs. The relative order of multi arcs is preserved.",
		[]{
			permutate_arcs(sort_arcs_first_by_tail_second_by_head(tail, head));
		}
	},
	{
		"add_back_arcs",
		"Adds a back arc for each arc that does not have one. Existing arcs retain their IDs. New arcs get higher IDs.",
		[]{
			auto extended_tail = id_id_func(
				2*tail.preimage_count(), tail.image_count(),
				[&](int i){
					if(i < tail.preimage_count())
						return tail(i);
					else
						return head(i - tail.preimage_count());
				}
			);
			auto extended_head = id_id_func(
				2*tail.preimage_count(), tail.image_count(),
				[&](int i){
					if(i < tail.preimage_count())
						return head(i);
					else
						return tail(i - tail.preimage_count());
				}
			);

			auto extended_arc_weight = id_func(
				2*tail.preimage_count(),
				[&](int i){
					if(i < tail.preimage_count())
						return arc_weight(i);
					else
						return arc_weight(i - tail.preimage_count());
				}
			);

			auto extended_arc_color = id_id_func(
				2*tail.preimage_count(),
				arc_color.image_count(),
				[&](int i){
					if(i < tail.preimage_count())
						return arc_color(i);
					else
						return arc_color(i - tail.preimage_count());
				}
			);

			auto keep_flag = identify_non_multi_arcs(extended_tail, extended_head);

			for(int i=0; i<tail.preimage_count(); ++i)
				keep_flag.set(i, true);

			int new_arc_count = count_true(keep_flag);
			ArrayIDIDFunc new_tail = keep_if(keep_flag, new_arc_count, extended_tail);
			ArrayIDIDFunc new_head = keep_if(keep_flag, new_arc_count, extended_head);
			ArrayIDFunc<int> new_arc_weight = keep_if(keep_flag, new_arc_count, extended_arc_weight);
			ArrayIDIDFunc new_arc_color = keep_if(keep_flag, new_arc_count, extended_arc_color);

			tail = move(new_tail);
			head = move(new_head);
			arc_weight = move(new_arc_weight);
			arc_color = move(new_arc_color);
		}
	},
	{
		"add_symmetric_node", 1,
		"arg1 is a list of all neighbor vertices. Arcs are added in both directions. The new node has color 1.",
		[](vector<string>args){
			const int old_node_count = tail.image_count();
			const int old_arc_count = tail.preimage_count();

			int neighbor_count = 0;

			forall_in_id_string(
				args[0], old_node_count,
				[&](int){
					++neighbor_count;
				}
			);

			int new_node = old_node_count;

			tail.set_image_count(old_node_count+1);
			head.set_image_count(old_node_count+1);


			node_color = add_preimage_at_end(std::move(node_color), 1, 0);
			node_geo_pos = add_preimage_at_end(std::move(node_geo_pos), 1, GeoPos{0.0, 0.0});
			node_original_position = add_preimage_at_end(std::move(node_original_position), 1, 0);
			node_weight = add_preimage_at_end(std::move(node_weight), 1, 0);


			tail = add_preimage_at_end(std::move(tail), neighbor_count*2);
			head = add_preimage_at_end(std::move(head), neighbor_count*2);

			int i = old_arc_count;
			forall_in_id_string(
				args[0], old_node_count,
				[&](int x){
					head[i] = x;
					tail[i] = new_node;
					++i;
				}
			);
			forall_in_id_string(
				args[0], old_node_count,
				[&](int x){
					tail[i] = x;
					head[i] = new_node;
					++i;
				}
			);

			arc_weight = add_preimage_at_end(std::move(arc_weight), neighbor_count*2, 0);
			arc_color = add_preimage_at_end(std::move(arc_color), neighbor_count*2, 0);

			cout << w << "new node" << " : " << new_node << endl;
		}
	},
	{
		"list_back_arcs", 1,
		"Lists the back arc of every arc",
		[](vector<string>args){
			auto back_arc = compute_back_arc_permutation(tail, head);
			save_text_file(
				args[0],
				[&](ostream&out){
					out << "forward_arc_id,backward_arc_id\n";
					for(int i=0; i<tail.preimage_count(); ++i)
						out << i << ',' << back_arc(i) << '\n';
				}
			);
		}
	},
	{
		"order_arcs_increasing_by_weight",
		"Orders the arcs increasing by their weight",
		[]{
			auto p = identity_permutation(tail.preimage_count());
			std::sort(p.begin(), p.end(), [&](int l, int r){return arc_weight(l) < arc_weight(r);});
			permutate_arcs(p);
		}
	},
	{
		"order_arcs_decreasing_by_weight",
		"Orders the arcs decreasing by their weight",
		[]{
			auto p = identity_permutation(tail.preimage_count());
			std::sort(p.begin(), p.end(), [&](int l, int r){return arc_weight(l) > arc_weight(r);});
			permutate_arcs(p);
		}
	},
	{
		"node_color_histogram", 1,
		"Computes a histogram of node colors",
		[](vector<string>args){
			auto h = compute_histogram(node_color);
			save_text_file(
				args[0],
				[&](std::ostream&out){
					out << "node_color,frequency\n";
					for(int i=0; i<h.preimage_count(); ++i)
						if(h(i) != 0)
							out << i << ',' << h(i) << '\n';
				}
			);
		}
	},
	{
		"arc_color_histogram", 1,
		"Computes a histogram of arc colors",
		[](vector<string>args){
			auto h = compute_histogram(arc_color);
			save_text_file(
				args[0],
				[&](std::ostream&out){
					out << "arc_color,frequency\n";
					for(int i=0; i<h.preimage_count(); ++i)
						if(h(i) != 0)
							out << i << ',' << h(i) << '\n';
				}
			);
		}
	},
	{
		"color_id", 1,
		"Get the color ID of a color selector",
		[](vector<string>arg){
			cout << select_color(arg[0]) << endl;
		}
	},
	{
		"color_node_in_geo_region", 4,
		"Colors all nodes in a geographic region with 1 and all other with 0. arg1 = min_lat, arg2 = max_lat, arg3 = min_lon, arg4 = max_lon",
		[](vector<string>arg){
			double
				min_lat = stof(arg[0]),
				max_lat = stof(arg[1]),
				min_lon = stof(arg[2]),
				max_lon = stof(arg[3]);

			if(max_lat < min_lat)
				throw runtime_error("max_lat must be smaller than min_lat");

			if(max_lon < min_lon)
				throw runtime_error("max_lon must be smaller than min_lon");


			node_color.set_image_count(2);
			for(int i=0; i<tail.image_count(); ++i)
				node_color.set(i, min_lat <= node_geo_pos(i).lat && node_geo_pos(i).lat <= max_lat && min_lon <= node_geo_pos(i).lon && node_geo_pos(i).lon <= max_lon);
		}
	},
	/*
	{
		"color_node_in_bag_of_tree_decomposition", 2,
		"arg1 = pace tree decomposition file, arg2 = i # colors the nodes in i-th largest bag with 1",
		[](vector<string>arg){

			load_uncached_text_file(
				arg[0],
				[&](istream&in){
					const int node_count = tail.image_count();
					vector<vector<int>>bag_list;

					string line;
					while(getline(in, line)){
						if(!line.empty() && line[0] == 'b'){
							istringstream lin(line);
							string ignore;
							lin >> ignore >> ignore;
							vector<int>bag;
							int node;
							while(lin >> node){
								--node;
								if(node >= node_count)
									throw runtime_error("Node-ID out of bounds");
								bag.push_back(node);
							}
							bag_list.push_back(move(bag));
						}
					}

					node_color.set_image_count(2);
					for(int i=0; i<node_count; ++i)
						node_color[i] = 0;
					unsigned to_highlight = stoi(arg[1])-1;
					if(to_highlight >= bag_list.size())
						throw runtime_error("Bag-ID out of bounds");
					for(auto x:bag_list[to_highlight])
						node_color[x] = 1;
				}
			);
		}
	},
	{
		"color_node_incident_to_geo_region", 4,
		"Colors all nodes in a geographic region with 1 and all other with 0. arg1 = min_lat, arg2 = max_lat, arg3 = min_lon, arg4 = max_lon",
		[](vector<string>arg){
			double
				min_lat = stof(arg[0]),
				max_lat = stof(arg[1]),
				min_lon = stof(arg[2]),
				max_lon = stof(arg[3]);

			auto in_region = id_func(
				tail.image_count(),
				[&](int i){
					return
						min_lat <= node_geo_pos(i).lat &&
						node_geo_pos(i).lat <= max_lat &&
						min_lon <= node_geo_pos(i).lon &&
						node_geo_pos(i).lon <= max_lon;
				}
			);
			node_color.set_image_count(2);
			node_color.fill(0);
			for(int i=0; i<tail.image_count(); ++i)
				node_color.set(i, min_lat <= node_geo_pos(i).lat && node_geo_pos(i).lat <= max_lat && min_lon <= node_geo_pos(i).lon && node_geo_pos(i).lon <= max_lon);
			for(int i=0; i<tail.preimage_count(); ++i)
				if(in_region(head(i))||in_region(tail(i))){
					node_color.set(tail(i), 1);
					node_color.set(head(i), 1);
				}
		}
	},
	{
		"compute_geo_bounding_box",
		"Computes min lat, max lat, min lon, max lon in that order",
		[]{
			cout
				<< "min_lat max_lat min_lon max_lon\n"
				<< min_over_id_func(id_func(node_geo_pos.preimage_count(), [](int i){return node_geo_pos(i).lat;})) << ' '
				<< max_over_id_func(id_func(node_geo_pos.preimage_count(), [](int i){return node_geo_pos(i).lat;})) << ' '
				<< min_over_id_func(id_func(node_geo_pos.preimage_count(), [](int i){return node_geo_pos(i).lon;})) << ' '
				<< max_over_id_func(id_func(node_geo_pos.preimage_count(), [](int i){return node_geo_pos(i).lon;})) << endl;
		}
	},
	 */
	{
		"parse_id_list", 1,
		"Parses an id list and prints all ids (mainly useful for debugging the parsing function).",
		[](vector<string>arg){
			forall_in_id_string(
				arg[0],
				[&](int x){
					cout << x << endl;
				}
			);
		}
	},
	{
		"color_arc_list", 2,
		"Colors a list of arc ids (arg2) separated by a specific color (arg1)",
		[](vector<string>arg){
			int color = stoi(arg[0]);
			if(color < 0)
				throw runtime_error("Colors must be non-negative integers");

			arc_color.set_image_count(std::max(arc_color.image_count(), color+1));

			forall_in_id_string(
				arg[1],
				tail.preimage_count(),
				[&](int x){arc_color[x] = color;}
			);
		}
	},
	{
		"color_node_list", 2,
		"Colors a list of node ids (arg2) separated by a specific color (arg1)",
		[](vector<string>arg){
			int color = stoi(arg[0]);
			if(color < 0)
				throw runtime_error("Colors must be non-negative integers");

			node_color.set_image_count(std::max(node_color.image_count(), color+1));

			forall_in_id_string(
				arg[1],
				tail.image_count(),
				[&](int x){node_color[x] = color;}
			);
		}
	},
	{
		"color_node_replace", 2,
		"Recolors all nodes with color arg1 to be of color arg2",
		[](vector<string>arg){
			int c = select_color(arg[0]);
			if(c < 0 || c > node_color.image_count())
				throw runtime_error("Valid colors are in the range [0,"+to_string(node_color.image_count())+ "). The color "+to_string(c)+" is therefore invalid");
			int r = stoi(arg[1]);
			if(r < 0)
				throw runtime_error("Colors must be non-negative integers");

			node_color.set_image_count(std::max(node_color.image_count(), r+1));
			for(auto&x:node_color)
				if(x == c)
					x = r;
		}
	},
	{
		"color_arc_replace", 2,
		"Recolors all arcs with color arg1 to be of color arg2",
		[](vector<string>arg){
			int c = select_color(arg[0]);
			if(c < 0 || c > arc_color.image_count())
				throw runtime_error("Valid colors are in the range [0,"+to_string(arc_color.image_count())+ "). The color "+to_string(c)+" is therefore invalid");
			int r = stoi(arg[1]);
			if(r < 0)
				throw runtime_error("Colors must be non-negative integers");

			arc_color.set_image_count(std::max(node_color.image_count(), r+1));
			for(auto&x:arc_color)
				if(x == c)
					x = r;
		}
	},
	{
		"set_node_color_count", 1,
		"Sets the node color count to arg1. All nodes with a color outside of this range are set to color 0.",
		[](vector<string>arg){
			int n = stoi(arg[0]);
			if(n <= 0)
				throw runtime_error("Color count must be a positive integer");

			for(auto&x:node_color)
				if(x >= n)
					x = 0;
			node_color.set_image_count(n);
		}
	},
	{
		"set_arc_color_count", 1,
		"Sets the arc color count to arg1. All arcs with a color outside of this range are set to color 0.",
		[](vector<string>arg){
			int n = stoi(arg[0]);
			if(n <= 0)
				throw runtime_error("Color count must be a positive integer");

			for(auto&x:arc_color)
				if(x >= n)
					x = 0;
			arc_color.set_image_count(n);
		}
	},
	{
		"color_node_out_degree",
		"Colors all nodes by their out degree.",
		[]{
			node_color.fill(0);
			node_color.set_image_count(1);
			for(int i=0; i<tail.preimage_count(); ++i){
				++node_color[tail(i)];
				node_color.set_image_count(std::max(node_color.image_count(), node_color[tail(i)]+1));
			}
		}
	},
	{
		"color_node_in_degree",
		"Colors all nodes by their in degree.",
		[]{
			node_color.fill(0);
			node_color.set_image_count(1);
			for(int i=0; i<tail.preimage_count(); ++i){
				++node_color[head(i)];
				node_color.set_image_count(std::max(node_color.image_count(), node_color[head(i)]+1));
			}
		}
	},
	{
		"color_node_incident_to_arc_color", 1,
		"Colors all nodes incident to a certain arc color with 1 and the rest with 0.",
		[](vector<string>arg){
			int c = select_color(arg[0]);
			if(c < 0 || c > arc_color.image_count())
				throw runtime_error("Valid colors are in the range [0,"+to_string(arc_color.image_count())+ "). The keep color "+to_string(c)+" is therefore invalid");
			node_color.set_image_count(2);
			node_color.fill(0);
			for(int i=0; i<tail.preimage_count(); ++i){
				if(arc_color(i) == c){
					node_color[head(i)] = 1;
					node_color[tail(i)] = 1;
				}
			}
		}
	},
	{
		"color_higher_node_incident_to_arc_color", 1,
		"Colors all nodes incident to a certain arc color with 1 and the rest with 0.",
		[](vector<string>arg){
			int c = select_color(arg[0]);
			if(c < 0 || c > arc_color.image_count())
				throw runtime_error("Valid colors are in the range [0,"+to_string(arc_color.image_count())+ "). The keep color "+to_string(c)+" is therefore invalid");
			node_color.set_image_count(2);
			node_color.fill(0);
			for(int i=0; i<tail.preimage_count(); ++i){
				if(arc_color(i) == c){
					if(head(i)> tail(i))
						node_color[head(i)] = 1;
					else
						node_color[tail(i)] = 1;
				}
			}
		}
	},
	{
		"color_arc_incident_to_node_color", 1,
		"Colors all arcs incident to a certain node color with 1 and the rest with 0.",
		[](vector<string>arg){
			int c = select_color(arg[0]);
			if(c < 0 || c > node_color.image_count())
				throw runtime_error("Valid colors are in the range [0,"+to_string(node_color.image_count())+ "). The keep color "+to_string(c)+" is therefore invalid");
			arc_color.set_image_count(2);
			for(int i=0; i<tail.preimage_count(); ++i)
				arc_color[i] = node_color(tail(i)) == c || node_color(head(i)) == c;
		}
	},
	{
		"color_arc_with_weight", 1,
		"Colors all arcs with a certain weight with color 1 and the rest with 0.",
		[](vector<string>arg){
			int w = stoi(arg[0]);
			arc_color.set_image_count(2);
			for(int i=0; i<tail.preimage_count(); ++i)
				arc_color[i] = arc_weight(i) == w;
		}
	},
	{
		"compute_distances", 2,
		"Computes all shortest path distances from a node (arg1) and store the distances to file arg2",
		[](vector<string>arg){
			int s = stoi(arg[0]);
			if(s < 0 || s >= tail.image_count())
				throw std::runtime_error("s is out of bounds");

			auto out_arc = invert_id_id_func(tail);
			auto dist = compute_distances(out_arc, head, arc_weight, s);


			save_text_file(
				arg[1],
				[&](std::ostream&o){
					o << "node_id,distance" << endl;
					for(int i=0; i<dist.preimage_count(); ++i)
						o << i << ',' << dist(i) << endl;

				}
			);
		}
	},
	{
		"compute_distance_tree", 2,
		"Computes the shortest path trees from a source node (arg1) and store the tree as a labeled parenthesis list to file arg2",
		[](vector<string>arg){
			int s = stoi(arg[0]);
			if(s < 0 || s >= tail.image_count())
				throw std::runtime_error("s is out of bounds");

			auto out_arc = invert_id_id_func(tail);


			save_text_file(
				arg[1],
				[&](std::ostream&o){

					auto print = [&](int x){
						o << x << ' ';
					};
					depth_first_traverse_shortest_path_tree(
						out_arc, head, arc_weight,
						s,
						print, print
					);
					o << '\n';
				}
			);
		}
	},
	{
		"compute_all_pair_distance_tree", 1,
		"Computes the shortest path trees stores them as labeled parenthesis list to file arg2. The i-th line corresponds to source node i.",
		[](vector<string>arg){
			auto out_arc = invert_id_id_func(tail);
			save_text_file(
				arg[0],
				[&](std::ostream&o){
					auto print = [&](int x){
						o << x << ' ';
					};

					for(int s=0; s<tail.image_count(); ++s){
						depth_first_traverse_shortest_path_tree(
							out_arc, head, arc_weight,
							s,
							print, print
						);
						o << '\n';
					}
				}
			);
		}
	},
	{
		"list_neighbors", 1,
		"Lists all neighbor nodes of each node and prints the result to arg1.",
		[](vector<string>args){
			auto succ = compute_successor_function(tail, head);
			save_text_file(
				args[0],
				[&](std::ostream&out){
					for(int x=0; x<tail.image_count(); ++x){
						out << setw(3) << x << " :";
						for(auto y:succ(x))
							out << ' ' << setw(3) << y;
						out << '\n';
					}
				}
			);
		}
	},
	{
		"list_sorted_out_arcs", 1,
		"Lists all out arcs of a graph where the arcs are sorted by tail.",
		[](vector<string>args){
			if(!is_sorted(tail.begin(), tail.end()))
				throw runtime_error("tails must be sorted");
			auto out = invert_sorted_id_id_func(tail);

			save_text_file(
				args[0],
				[&](std::ostream&o){
					for(int x=0; x<tail.image_count(); ++x){
						o << setw(3) << x << " :";
						for(auto y:out(x))
							o << ' ' << setw(3) << y;
						o << '\n';
					}
				}
			);
		}
	},
	{
		"flow_cutter_config",
		"Prints the flow cutter configuration to the console.",
		[]{
			cout << flow_cutter_config.get_config() << flush;
		}
	},
	
    {
        "flow_cutter_accelerated_enum_cuts", 1,
        "Enumerates balanced cuts.",
        [](vector<string>args){
            int node_count = tail.image_count();
            int arc_count = tail.preimage_count();

            if(!is_sorted(tail.begin(), tail.end()))
                throw runtime_error("arc tails must be sorted");
            if(!is_symmetric(tail, head))
                throw runtime_error("graph must be symmetric");
            if(!is_connected(tail, head))
                throw runtime_error("graph must be connected");
            if(flow_cutter_config.source < -1 || flow_cutter_config.source >= node_count)
                throw std::runtime_error("source node ID out of bounds");
            if(flow_cutter_config.target < -1 || flow_cutter_config.target >= node_count)
                throw std::runtime_error("target node ID out of bounds");

            if(flow_cutter_accelerated::requires_non_negative_weights(flow_cutter_config)){
                for(int i=0; i<arc_count; ++i)
                    if(arc_weight(i) < 0)
                        throw std::runtime_error("arc weights must be non-negative");
            }

            auto out_arc = invert_sorted_id_id_func(tail);
            auto back_arc = compute_back_arc_permutation(tail, head);

            auto graph = flow_cutter_accelerated::make_graph(
                    make_const_ref_id_id_func(tail),
                    make_const_ref_id_id_func(head),
                    make_const_ref_id_id_func(back_arc),
                    make_const_ref_id_func(arc_weight),
                    ConstIntIDFunc<1>(arc_count), // capacity
                    make_const_ref_id_func(out_arc)
            );


			tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
            save_text_file(
                    args[0],
                    [&](std::ostream&out){
                        auto w = std::setw(8);
                        out
                                << w << "time" << ','
                                << w << "cutter_instance" << ','
                                // << w << "source_node" << ','
                                // << w << "target_node" << ','
                                << w << "small_side_size" << ','
                                << w << "large_side_size" << ','
                                << w << "cut_size";

                        if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
                            out << ", cut";
                        if(flow_cutter_config.dump_state == flow_cutter::Config::DumpState::yes)
                            out << ", source_assimilated, target_assimilated, source_reachable, target_reachable, flow";
                        out << endl;

                        auto start_time = get_micro_time();

						flow_cutter_accelerated::CutterFactory factory(flow_cutter_config);
						auto cutter = factory(graph);

                        auto terminal_info = factory.select_source_target_pairs(
														 node_count, node_geo_pos, flow_cutter_config.cutter_count, flow_cutter_config.random_seed);

                        cutter.init(std::move(terminal_info), flow_cutter_config.random_seed, node_geo_pos);

			cutter.enum_cuts(
					 /* shall_continue*/
					 [&](const auto& cutter) {
						 int next_cut = cutter.get_current_flow_intensity();
						 if (cutter.cut_available()) {
							 ++next_cut;
						 }

						 return next_cut <= flow_cutter_config.max_cut_size;
					 },
					 /* report_cut */
					 [&](const auto& cutter, int cutter_id) {
						out
							<< w << (get_micro_time() - start_time) << ','
							<< w << cutter_id << ','
							// << w << node_orders[cutter_id].source << ','
							// << w << node_orders[cutter_id].target << ','
							<< w << cutter.get_current_smaller_cut_side_size() << ','
							<< w << tail.image_count() - cutter.get_current_smaller_cut_side_size() << ','
							<< w << cutter.get_current_cut().size();
						if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
						    out << ", " << make_id_string_from_list_with_back_arcs(cutter.get_current_cut(), back_arc);

						if(flow_cutter_config.dump_state == flow_cutter::Config::DumpState::yes){
						    auto dump = cutter.dump_state();
						    out << ','
							<< ' ' << make_id_string(dump.source_assimilated) << ','
							<< ' ' << make_id_string(dump.target_assimilated) << ','
							<< ' ' << make_id_string(dump.source_reachable) << ','
							<< ' ' << make_id_string(dump.target_reachable) << ','
							<< ' ' << make_id_string(dump.flow);
						}
						out << endl;
					 },
					 true);
                    }
            );
        }
    },
	{
		"flow_cutter_set", 2,
		"Sets a flow cutter configuration variable (arg1) to a value (arg2). To get a list of options type flow_cutter_set ? ?",
		[](vector<string>args){
			flow_cutter_config.set(args[0], args[1]);
		}
	},
	{
		"flow_cutter_enum_cuts", 1,
		"Enumerates balanced cuts.",
		[](vector<string>args){
			int node_count = tail.image_count();
			int arc_count = tail.preimage_count();

			if(!is_sorted(tail.begin(), tail.end()))
				throw runtime_error("arc tails must be sorted");
			if(!is_symmetric(tail, head))
				throw runtime_error("graph must be symmetric");
			if(!is_connected(tail, head))
				throw runtime_error("graph must be connected");
			if(flow_cutter_config.source < -1 || flow_cutter_config.source >= node_count)
				throw std::runtime_error("source node ID out of bounds");
			if(flow_cutter_config.target < -1 || flow_cutter_config.target >= node_count)
				throw std::runtime_error("target node ID out of bounds");

			if(flow_cutter::requires_non_negative_weights(flow_cutter_config)){
				for(int i=0; i<arc_count; ++i)
					if(arc_weight(i) < 0)
						throw std::runtime_error("arc weights must be non-negative");
			}


			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);

			auto graph = flow_cutter::make_graph(
				make_const_ref_id_id_func(tail),
				make_const_ref_id_id_func(head),
				make_const_ref_id_id_func(back_arc),
				make_const_ref_id_func(arc_weight),
				ConstIntIDFunc<1>(arc_count), // capacity
				make_const_ref_id_func(out_arc)
			);

		save_text_file(
			args[0],
			[&](std::ostream&out){
				auto w = std::setw(8);
				out
					<< w << "time" << ','
					<< w << "cutter_instance" << ','
					<< w << "source_node" << ','
					<< w << "target_node" << ','
					<< w << "small_side_size" << ','
					<< w << "large_side_size" << ','
					<< w << "cut_size";

				if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
					out << ", cut";
				if(flow_cutter_config.dump_state == flow_cutter::Config::DumpState::yes)
					out << ", source_assimilated, target_assimilated, source_reachable, target_reachable, flow";
				out << endl;

				flow_cutter::CutterFactory factory(flow_cutter_config);


				long long start_time = get_micro_time();

				auto cutter = factory(graph);

				std::vector<flow_cutter::SourceTargetPair>pairs;
				if(flow_cutter_config.source != -1 && flow_cutter_config.target != -1){
					for(int i=0; i<flow_cutter_config.cutter_count; ++i)
						pairs.push_back({flow_cutter_config.source, flow_cutter_config.target});
				}else if(flow_cutter_config.source != -1 || flow_cutter_config.target != -1){
					pairs = factory.select_source_target_pairs(node_count, flow_cutter_config.cutter_count, flow_cutter_config.random_seed);
					if(flow_cutter_config.source != -1){
						for(auto&x:pairs){
							if(x.target == flow_cutter_config.source)
								x.target = x.source;
							x.source = flow_cutter_config.source;
						}
					}else{
						for(auto&x:pairs){
							if(x.source == flow_cutter_config.target)
								x.source = x.target;
							x.target = flow_cutter_config.target;
						}
					}
				}else{
					pairs = factory.select_source_target_pairs(node_count, flow_cutter_config.cutter_count, flow_cutter_config.random_seed);
				}

				cutter.init(pairs, flow_cutter_config.random_seed);
				do{
					out
						<< w << (get_micro_time() - start_time) << ','
						<< w << cutter.get_current_cutter_id() << ','
						<< w << pairs[cutter.get_current_cutter_id()].source << ','
						<< w << pairs[cutter.get_current_cutter_id()].target << ','
						<< w << cutter.get_current_smaller_cut_side_size() << ','
						<< w << tail.image_count() - cutter.get_current_smaller_cut_side_size() << ','
						<< w << cutter.get_current_cut().size();
					if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
						out << ", " << make_id_string_from_list_with_back_arcs(cutter.get_current_cut(), back_arc);

					if(flow_cutter_config.dump_state == flow_cutter::Config::DumpState::yes){
						auto dump = cutter.dump_state();
						out << ','
							<< ' ' << make_id_string(dump.source_assimilated) << ','
							<< ' ' << make_id_string(dump.target_assimilated) << ','
							<< ' ' << make_id_string(dump.source_reachable) << ','
							<< ' ' << make_id_string(dump.target_reachable) << ','
							<< ' ' << make_id_string(dump.flow);
					}
					out << endl;
				}while((int)cutter.get_current_cut().size() < flow_cutter_config.max_cut_size && cutter.advance());
			}
		);
	}
},
	/*
{
	"flow_cutter_enum_separators", 1,
	"Enumerates balanced separators.",
	[](vector<string>args){
		int node_count = tail.image_count();
		int arc_count = tail.preimage_count();

		if(!is_sorted(tail.begin(), tail.end()))
			throw runtime_error("arc tails must be sorted");
		if(!is_symmetric(tail, head))
			throw runtime_error("graph must be symmetric");
		if(!is_connected(tail, head))
			throw runtime_error("graph must be connected");
		if(flow_cutter_config.source < -1 || flow_cutter_config.source >= node_count)
			throw std::runtime_error("source node ID out of bounds");
		if(flow_cutter_config.target < -1 || flow_cutter_config.target >= node_count)
			throw std::runtime_error("target node ID out of bounds");

		if(flow_cutter::requires_non_negative_weights(flow_cutter_config)){
			for(int i=0; i<arc_count; ++i)
				if(arc_weight(i) < 0)
					throw std::runtime_error("arc weights must be non-negative");
		}

		auto out_arc = invert_sorted_id_id_func(tail);
		auto back_arc = compute_back_arc_permutation(tail, head);

		auto expanded_graph = flow_cutter::expanded_graph::make_graph(
			make_const_ref_id_id_func(tail),
			make_const_ref_id_id_func(head),
			make_const_ref_id_id_func(back_arc),
			make_const_ref_id_id_func(arc_weight),
			make_const_ref_id_func(out_arc)
		);

		save_text_file(
			args[0],
			[&](std::ostream&out){
				auto w = std::setw(3);
				out
					<< w << "time" << ','
					<< w << "cutter_instance" << ','
					<< w << "source_node" << ','
					<< w << "target_node" << ','
					<< w << "small_side_size" << ','
					<< w << "large_side_size" << ','
					<< w << "separator_size" << ','
					<< w << "mixed_cut_size";
				if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
					out << ", separator";
				out << endl;

				flow_cutter::CutterFactory factory(flow_cutter_config);

				long long start_time = get_micro_time();
				auto cutter = factory(expanded_graph);

				std::vector<flow_cutter::SourceTargetPair>pairs;
				if(flow_cutter_config.source != -1 && flow_cutter_config.target != -1){
					pairs = {{flow_cutter_config.source, flow_cutter_config.target}};
				}else if(flow_cutter_config.source != -1 || flow_cutter_config.target != -1){
					pairs = factory.select_source_target_pairs(node_count, flow_cutter_config.cutter_count, flow_cutter_config.cutter_count);
					if(flow_cutter_config.source != -1){
						for(auto&x:pairs){
							if(x.target == flow_cutter_config.source)
								x.target = x.source;
							x.source = flow_cutter_config.source;
						}
					}else{
						for(auto&x:pairs){
							if(x.source == flow_cutter_config.target)
								x.source = x.target;
							x.target = flow_cutter_config.target;
						}
					}
				}else{
					pairs = factory.select_source_target_pairs(node_count, flow_cutter_config.cutter_count, flow_cutter_config.random_seed);
				}

				cutter.init(flow_cutter::expanded_graph::expand_source_target_pair_list(pairs), flow_cutter_config.random_seed);
				do{
					auto sep = flow_cutter::expanded_graph::extract_original_separator(tail, head, cutter);

					out
						<< w << (get_micro_time() - start_time) << ','
						<< w << cutter.get_current_cutter_id() << ','
						<< w << pairs[cutter.get_current_cutter_id()].source << ','
						<< w << pairs[cutter.get_current_cutter_id()].target << ','
						<< w << sep.small_side_size << ','
						<< w << (tail.image_count() - sep.small_side_size - cutter.get_current_cut().size()) << ','
						<< w << sep.sep.size() << ','
						<< w << cutter.get_current_cut().size();
					if(flow_cutter_config.report_cuts == flow_cutter::Config::ReportCuts::yes)
						out
							<< ", "
							<< make_id_string_from_list(sep.sep);
					out << endl;
				}while((int)cutter.get_current_cut().size() < flow_cutter_config.max_cut_size && cutter.advance());
			}
		);
	}
},

	 */
{
	"flow_cutter_expand_graph",
	"Expands the currently loaded graph.",
	[]{
		int
			node_count = tail.image_count(),
			arc_count = tail.preimage_count();

		ArrayIDIDFunc
			new_head(flow_cutter::expanded_graph::head(node_count, arc_count, make_const_ref_id_id_func(head))),
			new_tail(flow_cutter::expanded_graph::tail(node_count, arc_count, make_const_ref_id_id_func(tail))),
			new_arc_color(flow_cutter::expanded_graph::capacity(node_count, arc_count), 2);
		ArrayIDFunc<int>
			new_arc_weight(flow_cutter::expanded_graph::arc_weight(node_count, arc_count, make_const_ref_id_func(arc_weight)));

		head.swap(new_head);
		tail.swap(new_tail);
		arc_color.swap(new_arc_color);
		arc_weight.swap(new_arc_weight);

		node_count = tail.image_count();
		arc_count = tail.preimage_count();

		node_color = ArrayIDIDFunc(node_count, 1);
		node_color.fill(0);

		node_weight = ArrayIDFunc<int>(node_count);
		node_weight.fill(0);

		node_geo_pos = ArrayIDFunc<GeoPos>(node_count);
		node_geo_pos.fill({0,0});
	}
},

{
	"is_symmetric",
	"Checks whether a graph is symmetric",
	[]{
		cout << w << "is symmetric?" << " : " << boolalpha << is_symmetric(tail, head) << endl;
	}
},
{
	"is_connected",
	"Checks whether a graph is connected",
	[]{
		cout << w << "is connected?" << " : " << boolalpha << is_connected(tail, head) << endl;
	}
},
{
	"is_loop_free",
	"Checks whether a graph has loops",
	[]{
		cout << w << "is loop free?" << " : " << boolalpha << is_loop_free(tail, head) << endl;
	}
},
{
	"has_multi_arcs",
	"Checks whether a graph has multi arcs",
	[]{
		cout << w << "has multi-arcs?" << " : " << boolalpha << has_multi_arcs(tail, head) << endl;
	}
},
{
	"clear_node_color",
	"Sets all node colors to color.",
	[]{
		node_color.set_image_count(1);
		node_color.fill(0);
	}
},

	
{
	"load_node_color_partition", 1,
	"Loads a partition file into the node colors",
	[](vector<string>arg){
		node_color.set_image_count(1);
		load_uncached_text_file(
			arg[0],
			[&](std::istream&in){
				for(int i=0; i<node_color.preimage_count(); ++i){
					int x;
					in >> x;
					node_color.set_image_count(std::max(x+1, node_color.image_count()));
					node_color[i] = x;
				}
			}
		);
	}
},
{
	"save_node_color_partition", 1,
	"Saves a partition derived from the node colors.",
	[](vector<string>arg){

		node_color.set_image_count(1);
		save_text_file(
			arg[0],
			[&](std::ostream&out){
				if(tail.image_count() != 0){
					out << node_color(0);
					for(int i=1; i<node_color.preimage_count(); ++i)
						out << ' ' << node_color(i);
				}
				out << endl;
			}
		);
	}
},
{
	"list_arcs_between_node_colors",
	"Lists all arcs whose endpoints have different node colors.",
	[]{
		const int arc_count = tail.preimage_count();
		bool first = true;
		for(int i=0; i<arc_count; ++i){
			if(node_color(tail(i)) != node_color(head(i))){
				if(first)
					first = false;
				else
					cout << ':';
				cout << i;
			}
		}
		cout << endl;
	}
},
{
	"list_nodes_of_color", 1,
	"Lists all nodes of a certain color.",
	[](vector<string>arg){
		int c = select_color(arg[0]);
		if(c < 0 || c > node_color.image_count())
			throw runtime_error("Valid colors are in the range [0,"+to_string(node_color.image_count())+ "). The color "+to_string(c)+" is therefore invalid");

		cout << make_id_string(id_func(tail.image_count(), [&](int x){return node_color(x) == c;})) << endl;
	}
},
{
	"list_arcs_of_color", 1,
	"Lists all arcs of a certain color.",
	[](vector<string>arg){
		int c = select_color(arg[0]);
		if(c < 0 || c > arc_color.image_count())
			throw runtime_error("Valid colors are in the range [0,"+to_string(arc_color.image_count())+ "). The color "+to_string(c)+" is therefore invalid");

		cout << make_id_string(id_func(tail.preimage_count(), [&](int x){return arc_color(x) == c;})) << endl;
	}
},
{
	"examine_node_color_cut",
	"Examines the cut induced by the node coloring",
	[]{
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();

		if(node_color.image_count() != 2)
			throw runtime_error("nodes may only be colored using two cuts");
		UnionFind left(node_count), right(node_count);
		int cut_size = 0;
		for(int i=0; i<arc_count; ++i){
			int t = tail(i), h = head(i);
			if(node_color(t) == 0 && node_color(h) == 0)
				left.unite(t, h);
			else if(node_color(t) == 1 && node_color(h) == 1)
				right.unite(t, h);
			else
				++cut_size;
		}
		int left_size = 0;
		int right_size = 0;
		for(int i=0; i<node_count; ++i)
			if(node_color(i) == 0)
				++left_size;
			else
				++right_size;
		cout << "left_side_size : " << left_size << endl;
		cout << "right_side_size : " << right_size << endl;
		cout << "node_count : " << node_count << endl;
		cout << "edge_count : " << arc_count/2 << endl;
		cout << "cut_size : " << cut_size/2 << endl;
		cout << "left components : " << left.component_count() - right_size << endl;
		cout << "right components : " << right.component_count() - left_size << endl;

		cout << "alpha : " << std::fixed << static_cast<double>(std::max(left_size, right_size)) / static_cast<double>(node_count) << endl;
		if (std::abs(left_size - right_size) <= 1)
			cout << "epsilon : " << std::fixed << 0.0 << std::endl;
		else
			cout << "epsilon : " << std::fixed << 2*(static_cast<double>(std::max(left_size, right_size)) / static_cast<double>(node_count))-1 << endl;

	}
},

{
	"inertial_flow_cut", 1,
	"Runs the inertial cut algorithm. The argument is the minimum size of the smaller side, a value between 0.0 and 0.5",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		const int node_count = tail.image_count();

		double min_balance = stof(args[0]);
		if(min_balance < 0 || min_balance > 0.5)
			throw runtime_error("min balance parameter must be between 0.0 and 0.5");
		auto c = inertial_flow::compute_inertial_flow_cut(tail, head, node_geo_pos, min_balance, true);
		node_color.set_image_count(2);
		for(int i=0; i<node_count; ++i)
			node_color[i] = c.is_on_smaller_side(i);
		//cout << "smaller_side_size : " << c.smaller_side_size << endl;
		//cout << "cut_size : " << c.cut_size << endl;

	}
},
{
	"inertial_flow_cut_ford_fulkerson", 1,
	"Runs the inertial cut algorithm with Ford Fulkerson's flow algorithm. The argument is the minimum size of the smaller side, a value between 0.0 and 0.5",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		const int node_count = tail.image_count();

		double min_balance = stof(args[0]);
		if(min_balance < 0 || min_balance > 0.5)
			throw runtime_error("min balance parameter must be between 0.0 and 0.5");
		auto c = inertial_flow::compute_inertial_flow_cut(tail, head, node_geo_pos, min_balance, false);
		node_color.set_image_count(2);
		for(int i=0; i<node_count; ++i)
			node_color[i] = c.is_on_smaller_side(i);
		//cout << "smaller_side_size : " << c.smaller_side_size << endl;
		//cout << "cut_size : " << c.cut_size << endl;

	}
},
{
	"inertial_flow_separator", 1,
	"Runs the inertial cut algorithm. The argument is the minimum size of the smaller side, a value between 0.0 and 0.5",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		double min_balance = stof(args[0]);
		if(min_balance < 0 || min_balance > 0.5)
			throw runtime_error("min balance parameter must be between 0.0 and 0.5");
		cout << make_id_string_from_list(inertial_flow::compute_inertial_flow_separator(tail, head, node_geo_pos, min_balance, true)) << endl;
	}
},



{
	"echo", 1,
	"Prints a string to stdout",
	[](vector<string>args){
		cout << args[0] << endl;
	}
},

{
	"reorder_nodes_in_input_order",
	"Reorders all nodes in nested dissection order.",
	[]{
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");

		const int node_count = head.image_count();

		ArrayIDIDFunc order(node_count, node_count);
		stable_sort_copy_by_id(
			CountIterator{0}, CountIterator{node_count},
			order.begin(),
			node_original_position.image_count(),
			node_original_position
		);

		permutate_nodes(order);
	}
},

#ifdef USE_KAHIP
{
        "kahip_cut", 1,
        "Runs kahip's cut algorithm. The argument is KaHiP's minimum imbalance.",
        [](vector<string>args){
            if(!is_symmetric(tail, head))
                throw runtime_error("Graph must be symmetric");
            if(has_multi_arcs(tail, head))
                throw runtime_error("Graph must not have multi arcs");
            if(!is_loop_free(tail, head))
                throw runtime_error("Graph must not have loops");
            const int node_count = tail.image_count();

            double min_balance = stof(args[0]);
            if(min_balance < 0 || min_balance >= 1.0)
                throw runtime_error("min balance parameter must be between 0.0 and 0.5");
            auto c = my_kahip::compute_my_kahip_cut(tail, head, min_balance);
            node_color.set_image_count(2);
            for(int i=0; i<node_count; ++i)
                node_color[i] = c.is_on_smaller_side(i);
            //cout << "smaller_side_size : " << c.smaller_side_size << endl;
            //cout << "cut_size : " << c.cut_size << endl;

        }
},

{
	"reorder_nodes_in_kahip_cch_order", 1,
	"Reorders all nodes in nested dissection order.",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(
			cch_order::compute_cch_graph_order(
				tail, head, arc_weight,
				my_kahip::ComputeSeparator(epsilon)
			)
		);
	}
},

{
	"reorder_nodes_in_kahip_nested_dissection_order", 1,
	"Reorders all nodes in nested dissection order. epsilon is arg1",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");
		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(cch_order::compute_nested_dissection_graph_order(tail, head, arc_weight, my_kahip::ComputeSeparator(epsilon)));
	}
},

{
	"reorder_nodes_in_kahip_cch_order_with_separator_stats", 2,
	"Reorders all nodes in nested dissection order. epsilon is arg1 and the separator stats are in arg2",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");
		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		save_text_file(args[1],
			[&](std::ostream&out){
				permutate_nodes(
					cch_order::compute_cch_graph_order(
						tail, head, arc_weight,
						separator::report_separator_statistics(out, my_kahip::ComputeSeparator(epsilon))
					)
				);
			}
		);
	}
},
{
	"reorder_nodes_in_kahip2_nested_dissection_order", 1,
	"Reorders all nodes in nested dissection order. epsilon is arg1",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");
		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(cch_order::compute_nested_dissection_graph_order(tail, head, arc_weight, my_kahip::ComputeSeparator2(epsilon)));
	}
},
{
	"reorder_nodes_in_kahip2_nested_dissection_order_with_separator_stats", 2,
	"Reorders all nodes in nested dissection order. epsilon is arg1 and the separator stats are in arg2",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		save_text_file(args[1],
			[&](std::ostream&out){
				std::mt19937 rng(flow_cutter_config.random_seed);

				permutate_nodes(
					cch_order::compute_nested_dissection_graph_order(
						tail, head, arc_weight,
						separator::report_separator_statistics(out, my_kahip::ComputeSeparator2(epsilon))
					)
				);
			}
		);
	}
},
{
	"reorder_nodes_in_kahip2_cch_order", 1,
	"Reorders all nodes in nested dissection order.",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(
			cch_order::compute_cch_graph_order(
				tail, head, arc_weight,
				my_kahip::ComputeSeparator2(epsilon)
			)
		);
	}
},
{
	"reorder_nodes_in_kahip2_cch_order_with_separator_stats", 2,
	"Reorders all nodes in nested dissection order. epsilon is arg1 and the separator stats are in arg2",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		if(!is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("Tails must be sorted");
		double epsilon = stof(args[0]);
		if(epsilon < 0 || epsilon > 1)
			throw runtime_error("epsilon parameter must be between 0.0 and 1.0");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		save_text_file(args[1],
			[&](std::ostream&out){
				permutate_nodes(
					cch_order::compute_cch_graph_order(
						tail, head, arc_weight,
						separator::report_separator_statistics(out, my_kahip::ComputeSeparator2(epsilon))
					)
				);
			}
		);
	}
},
#endif
{
	"reorder_nodes_in_inertial_flow_cch_order", 1,
	"Reorders all nodes in nested dissection order. min_balance is arg1",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		double min_balance = stof(args[0]);
		if(min_balance < 0 || min_balance > 0.5)
			throw runtime_error("min balance parameter must be between 0.0 and 0.5");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(cch_order::compute_cch_graph_order(tail, head, arc_weight, inertial_flow::ComputeSeparator(node_geo_pos, min_balance, true)));
	}
},
{
	"reorder_nodes_in_inertial_flow_ford_fulkerson_cch_order", 1,
	"Reorders all nodes in nested dissection order using inertial flow with the Ford Fulkerson flow algorithm. min_balance is arg1",
	[](vector<string>args){
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		double min_balance = stof(args[0]);
		if(min_balance < 0 || min_balance > 0.5)
			throw runtime_error("min balance parameter must be between 0.0 and 0.5");

		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(cch_order::compute_cch_graph_order(tail, head, arc_weight, inertial_flow::ComputeSeparator(node_geo_pos, min_balance, false)));
	}
},
{
	"clear_arc_color",
	"Sets all arc colors to color.",
	[]{
		arc_color.set_image_count(1);
		arc_color.fill(0);
	}
},
{
	"color_connected_components",
	"Colors all nodes different in different components.",
	[](){
		node_color = compute_connected_components(tail, head);
	}
},
{
	"color_strongly_connected_components",
	"Colors all nodes different in different strongly connected components.",
	[](){
		node_color = compute_strongly_connected_components(compute_successor_function(tail, head));
	}
},
{
	"test_depth_first_search",
	"Tests the symmetric depth first search.",
	[]{
		if(!is_symmetric(tail, head))
			throw runtime_error("This DFS variant only works on symmetric graphs");
		symmetric_depth_first_search(
			invert_id_id_func(tail),
			head,
			[](int v){
				cout << "First visit to root " << v << endl;
			},
			[](int v){
				cout << "Last visit to root " << v << endl;
			},
			[](int u, int uv, int v){
				cout << "Tree down " << uv << " "<<u << "->" << v << endl;
			},
			[](int u, int uv, int v){
				cout << "Tree up " << uv << " "<<u << "->" << v << endl;
			},
			[](int u, int uv, int v){
				cout << "Non-Tree " << uv << " "<<u << "->" << v << endl;
			}
		);
	}
},
{
	"color_biconnected_components",
	"Colors the two connected components by coloring the arcs.",
	[]{
		if(!is_symmetric(tail, head))
			throw runtime_error("Can only color the 2-connected components of a symmetric graph");
		arc_color = compute_biconnected_components(invert_id_id_func(tail), head, compute_back_arc_permutation(tail, head));
	}
},
{
	"reorder_nodes_in_preorder",
	"Reorders all nodes according to a dfs rooted at an arbitrary node.",
	[]{
		permutate_nodes(compute_preorder(compute_successor_function(tail, head)).first);
	}
},

{
	"swap_nodes", 2,
	"Exchanges two nodes",
	[](vector<string>args){
		const int node_count = tail.image_count();
		int x = stoi(args[0]), y = stoi(args[1]);
		if(x < 0 || x >= node_count)
			throw runtime_error("First node ID is invalid");
		if(y < 0 || y >= node_count)
			throw runtime_error("Second node ID is invalid");

		if(x != y){
			ArrayIDIDFunc order = identity_permutation(node_count);
			order[x] = y;
			order[y] = x;
			permutate_nodes(order);
		}
	}
},

{
	"reorder_nodes_in_flow_cutter_cch_order",
	"Reorders all nodes in nested dissection order.",
	[]{
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");
		tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
		permutate_nodes(
			cch_order::compute_cch_graph_order(
				tail, head, arc_weight,
				flow_cutter::ComputeSeparator<flow_cutter::CutterFactory, ArrayIDFunc<GeoPos>>(node_geo_pos, flow_cutter_config)
			)
		);
	}
},
{
	"reorder_nodes_in_accelerated_flow_cutter_cch_order",
	"Reorders all nodes in nested dissection order using flow_cutter accelerated.",
	[]{
		if(!is_symmetric(tail, head))
			throw runtime_error("Graph must be symmetric");
		if(has_multi_arcs(tail, head))
			throw runtime_error("Graph must not have multi arcs");
		if(!is_loop_free(tail, head))
			throw runtime_error("Graph must not have loops");

		ArrayIDIDFunc order;


		//omp_set_nested(true);
		//#pragma omp parallel num_threads(flow_cutter_config.thread_count)
		//#pragma omp single nowait
		{
			tbb::task_scheduler_init scheduler(flow_cutter_config.thread_count);
			order = cch_order::compute_cch_graph_order(tail, head, arc_weight, flow_cutter::ComputeSeparator<flow_cutter_accelerated::CutterFactory, ArrayIDFunc<GeoPos>>(node_geo_pos, flow_cutter_config));
		}
		permutate_nodes(order);
	}
},

{
	"reorder_nodes_at_random",
	"Reorders all nodes according to a dfs rooted at an arbitrary node.",
	[]{
		ArrayIDIDFunc perm(tail.image_count(), tail.image_count());
		std::mt19937 rng(flow_cutter_config.random_seed);
		for(int i=0; i<tail.image_count(); ++i)
			perm[i] = i;
		std::shuffle (perm.begin(), perm.end(), rng);
		permutate_nodes(perm);
	}
},

{
	"set_show_undirected",
	"The next graphs will be shown as undirected graphs. Note that the graph must be symmetric for this to work.",
	[]{
		show_undirected = true;
	}
},
{
	"set_show_directed",
	"The next graphs will be shown using directed graphs",
	[]{
		show_undirected = false;
	}
},
{
	"set_show_arc_id",
	"The next graphs will be shown with arc ids.",
	[]{
		show_arc_ids = true;
	}
},
{
	"set_hide_arcs_id",
	"The next graphs will be shown without arc ids.",
	[]{
		show_arc_ids = false;
	}
},

{
	"find_closest_node", 2,
	"Finds the node closest to a geographic position.",
	[](vector<string>arg){
		GeoPos p = {stof(arg[0]), stof(arg[1])};
		cout << min_preimage_over_id_func(id_func(tail.image_count(), [&](int x){return geo_dist(p, node_geo_pos(x));})) << endl;
	}
}
};

int main(int argc, char*argv[]){
	try{
		if(argc == 1){
			cout << "FlowCutter; partition graphs into two parts. For details type:\n\n\t"<<argv[0] << " help\n" << endl;
			return 0;
		}

		sort(std::begin(cmd), std::end(cmd),
			[](const Command&l, const Command&r){
				return l.name < r.name;
			}
		);

		int arg_pos = 1;
		while(arg_pos != argc){
			for(auto&c:cmd)
				if(c.name == argv[arg_pos]){
					if(arg_pos+1+c.parameter_count > argc)
						throw runtime_error("Not enough parameters to command "+c.name);

					vector<string>args(argv+arg_pos+1, argv+arg_pos+1+c.parameter_count);
					++arg_pos;
					arg_pos += c.parameter_count;

					auto prev_time_commands = time_commands;
					long long time = -get_micro_time();
					c.func(move(args));
					time += get_micro_time();

					if(time_commands && prev_time_commands){
						cout << "running time : "<<time << "musec" << endl;
					}

					goto while_continue;
				}
			throw runtime_error(string("Unknown command ")+argv[arg_pos]);
		while_continue:;
		}

	}catch(exception&err){
		cerr << "Exception : " << err.what() << endl;
	}
}

