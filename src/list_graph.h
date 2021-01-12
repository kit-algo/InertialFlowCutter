#ifndef LIST_GRAPH_H
#define LIST_GRAPH_H

#include "array_id_func.h"

#include <tuple>
#include <string>

struct ListGraph{
	ListGraph()=default;
	ListGraph(int node_count, int arc_count)
		:head(arc_count, node_count), tail(arc_count, node_count), arc_weight(arc_count), node_weight(node_count){}

	int node_count()const{ return head.image_count(); }
	int arc_count()const{ return head.preimage_count(); }

	ArrayIDIDFunc head, tail;
	ArrayIDFunc<int>arc_weight, node_weight;
};

void save_binary_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&node_weight, const ArrayIDFunc<int>&arc_weight);
ListGraph load_binary_graph(const std::string&file_name);

void save_csv_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight);

void save_dimacs_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight);
ListGraph load_dimacs_graph(const std::string&file_name);
ListGraph uncached_load_dimacs_graph(const std::string&file_name);

void save_ddsg_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight);

void save_metis_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const ArrayIDFunc<int>&arc_weight);
ListGraph load_metis_graph(const std::string&file_name);
ListGraph uncached_load_metis_graph(const std::string&file_name);

ListGraph load_color_dimacs_graph(const std::string&file_name);
ListGraph uncached_load_dimacs_graph(const std::string&file_name);

ListGraph load_pace_graph(const std::string&file_name);
ListGraph uncached_load_pace_graph(const std::string&file_name);
void save_pace_graph(const std::string&file_name, const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head);

#endif
