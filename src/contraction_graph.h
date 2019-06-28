#ifndef CONTRACTION_GRAPH_H
#define CONTRACTION_GRAPH_H

#include "array_id_func.h"
#include "tiny_id_func.h"
#include "min_max.h"
#include "multi_arc.h"
#include <cassert>
#include <algorithm>

class EdgeContractionGraph{
public:
	void rewire_arcs_from_second_to_first(int u, int v){
		union_find_parent[v] = u;
		std::swap(next_adjacency_in_ring[u], next_adjacency_in_ring[v]);
	}

	template<class F>
	void forall_nodes_in_last_computed_neighborhood(const F&f){
		for(int i=0; i<neighborhood_size; ++i)
			f(neighborhood(i));
	}

	void compute_neighborhood_of(int v){
		for(int i=0; i<neighborhood_size; ++i)
			in_neighborhood.set(neighborhood(i), false);
		neighborhood_size = 0;

		if(union_find_parent[v] == v){
			const int initial_adjacency = v;
			int current_adjacency = v;
			do{
				// Iterate over the adjacency
				{
					int arc_in_begin = out_arc_begin[current_adjacency];
					int arc_in_end = out_arc_end[current_adjacency];

					int arc_out_begin = out_arc_begin[current_adjacency];

					while(arc_in_begin != arc_in_end){
						// Compress union find path
						{
							int x = arc_head[arc_in_begin];
							while(union_find_parent[x] != x)
								x = union_find_parent[x];
							int y = arc_head[arc_in_begin];
							while(union_find_parent[y] != y){
								int z = union_find_parent[y];
								union_find_parent[y] = x;
								y = z;
							}

						}

						// Replace arc head by representative from union find
						arc_head[arc_in_begin] = union_find_parent[arc_head[arc_in_begin]];

						assert(union_find_parent[arc_head[arc_in_begin]] == arc_head[arc_in_begin]);

						// Only keep the nodes that are not the heads of loops or multi arcs
						if(!in_neighborhood(arc_head[arc_in_begin]) && arc_head[arc_in_begin] != v){
							arc_head[arc_out_begin] = arc_head[arc_in_begin];
							++arc_out_begin;
							in_neighborhood.set(arc_head[arc_in_begin], true);
							neighborhood[neighborhood_size++] = arc_head[arc_in_begin];
						}

						++arc_in_begin;
					}

					out_arc_end[current_adjacency] = arc_out_begin;
				}

				// Goto next non-empty adjacency in the ring, and rewire the ring pointer to skip them in future
				int next_adjacency = next_adjacency_in_ring[current_adjacency];
				while(out_arc_begin[next_adjacency] == out_arc_end[next_adjacency] && next_adjacency != initial_adjacency)
					next_adjacency = next_adjacency_in_ring[next_adjacency];
				next_adjacency_in_ring[current_adjacency] = next_adjacency;
				current_adjacency = next_adjacency;
			}while(current_adjacency != initial_adjacency);
		}
	}

	template<class Tail, class Head>
	EdgeContractionGraph(const Tail&tail, const Head&head):
		next_adjacency_in_ring(tail.image_count()),
		union_find_parent(tail.image_count()),
		out_arc_begin(tail.image_count()),
		out_arc_end(tail.image_count()),
		arc_head(tail.preimage_count()),
		in_neighborhood(tail.image_count()),
		neighborhood(tail.image_count()),
		neighborhood_size(0)
	{
		assert(is_symmetric(tail, head));
		for(int i=0; i<tail.image_count(); ++i){
			next_adjacency_in_ring.set(i, i);
			union_find_parent.set(i, i);
		}

		in_neighborhood.fill(false);

		out_arc_end.fill(0);
		for(int i=0; i<tail.preimage_count(); ++i){
			int t = tail(i);
			out_arc_end.set(t, out_arc_end(t)+1);
		}
		out_arc_begin.set(0, 0);
		for(int i=1; i<tail.image_count(); ++i){
			out_arc_begin.set(i, out_arc_end(i-1));
			out_arc_end.set(i, out_arc_end(i) + out_arc_begin(i));
		}
		assert(out_arc_end(tail.image_count()-1) == tail.preimage_count());

		for(int i=0; i<tail.preimage_count(); ++i){
			int t = tail(i);
			arc_head.set(out_arc_begin(t), head(i));
			out_arc_begin.set(t, out_arc_begin(t)+1);
		}
		for(int i=0; i<tail.preimage_count(); ++i){
			int t = tail(i);
			out_arc_begin.set(t, out_arc_begin(t)-1);
		}
	}

private:
	ArrayIDFunc<int> next_adjacency_in_ring;
	ArrayIDFunc<int> union_find_parent;
	ArrayIDFunc<int> out_arc_begin;
	ArrayIDFunc<int> out_arc_end;
	ArrayIDFunc<int> arc_head;
	BitIDFunc in_neighborhood;
	ArrayIDFunc<int> neighborhood;
	int neighborhood_size;
};

class NodeContractionGraph{
public:
	template<class Tail, class Head>
	NodeContractionGraph(const Tail&tail, const Head&head):
		g(tail, head), is_virtual(tail.image_count()){
		assert(is_symmetric(tail, head));
		is_virtual.fill(false);
	}

	template<class F>
	void forall_neighbors_then_contract_node(int v, const F&callback){
		g.compute_neighborhood_of(v);
		g.forall_nodes_in_last_computed_neighborhood(
			[&](int u){
				if(is_virtual(u))
					g.rewire_arcs_from_second_to_first(v, u);
			}
		);
		is_virtual.set(v, true);
		g.compute_neighborhood_of(v);
		g.forall_nodes_in_last_computed_neighborhood(callback);
	}

private:
	EdgeContractionGraph g;
	BitIDFunc is_virtual;
};

template<class Tail, class Head, class OnNewArc>
int compute_chordal_supergraph(const Tail&tail, const Head&head, const OnNewArc&on_new_arc){
	assert(is_symmetric(tail, head));

	NodeContractionGraph g(tail, head);
	int max_upward_degree = 0;
	for(int x=0; x<tail.image_count()-1; ++x){
		int upward_degree = 0;
		g.forall_neighbors_then_contract_node(
			x,
			[&](int y){
				on_new_arc(x, y);
				++upward_degree;
			}
		);
		max_to(max_upward_degree, upward_degree);
	}
	return max_upward_degree;
}

#endif
