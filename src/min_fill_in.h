#ifndef MIN_FILL_IN_H
#define MIN_FILL_IN_H

#include "tree_node_ranking.h"
#include "connected_components.h"
#include "tiny_id_func.h"
#include "min_max.h"
#include "histogram.h"
#include "array_id_func.h"
#include "permutation.h"
#include "filter.h"
#include "multi_arc.h"
#include "id_multi_func.h"
#include "preorder.h"
#include <vector>

#ifndef NDEBUG
#include "union_find.h"
#endif


#include <tbb/task_group.h>
#include <tbb/parallel_for_each.h>
#include <functional>
#include <tbb/tick_count.h>
#include <tbb/parallel_invoke.h>



namespace cch_order{
	static constexpr int TASK_SPAWN_CUTOFF = 800;


	inline
	bool is_valid_partial_order(const ArrayIDIDFunc&partial_order){
		return max_over_id_func(compute_histogram(partial_order)) <= 1;
	}

	// Computes an optimal order for a graph consisting of only a path
	template<class InputNodeID>
	ArrayIDIDFunc compute_path_graph_order(int node_count, const InputNodeID&input_node_id){
		ArrayIDIDFunc order(node_count, input_node_id.image_count());
		int pos = 0;
		for(int i=1; i<=node_count; i*=2){
			for(int j=i-1; j<node_count; j+=i,j+=i){
				order[pos++] = input_node_id(j);
			}
		}
		assert(is_valid_partial_order(order));
		return order; // NVRO
	}

	template<class Tail, class Head, class InputNodeID>
	ArrayIDIDFunc compute_tree_graph_order(const Tail&tail, const Head&head, const InputNodeID&input_node_id){
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();

		assert(is_connected(tail, head));
		assert(2*(node_count-1) == arc_count);

		(void)arc_count; // no warning

		ArrayIDIDFunc order(node_count, input_node_id.image_count());

		auto level = compute_tree_node_ranking(compute_successor_function(tail, head));
		auto identity = count_range(node_count);
		stable_sort_copy_by_id(std::begin(identity), std::end(identity), std::begin(order), node_count, level);
		for(auto&x:order)
			x = input_node_id(x);

		assert(is_valid_partial_order(order));
		return order; // NVRO
	}

	template<class Tail, class Head>
	ArrayIDIDFunc compute_tree_graph_order(const Tail&tail, const Head&head){
		return compute_tree_graph_order(
			tail, head,
			IdentityIDIDFunc(tail.image_count())
		);
	}

	// Computes an optimal order for a trivial graph. If the input graph is not trivial, then the task is forwarded to the compute_non_trivial_graph_order functor parameter.
	// A graph is trivial if it is a clique or a tree.
	//	
	// Precondition: the graph is connected
	template<class ComputeNonTrivialGraphOrder>
	ArrayIDIDFunc compute_trivial_graph_order_if_graph_is_trivial(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDIDFunc input_node_id,
		ArrayIDFunc<int>arc_weight, const ComputeNonTrivialGraphOrder&compute_non_trivial_graph_order
	){
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();

		assert(is_connected(tail, head));

		bool 
			is_clique = (static_cast<long long>(node_count)*static_cast<long long>(node_count-1) == static_cast<long long>(arc_count)),
			has_no_arcs = (arc_count == 0),
			is_tree = (arc_count == 2*(node_count-1));

		ArrayIDIDFunc order;


		if(is_clique || has_no_arcs){
			order = id_id_func(node_count, input_node_id.image_count(), [&](int x){return input_node_id(x);});
		}else if(is_tree){
			order = compute_tree_graph_order(std::move(tail), std::move(head), std::move(input_node_id));
		}else {
			order = compute_non_trivial_graph_order(std::move(tail), std::move(head), std::move(input_node_id), std::move(arc_weight));
		}

		assert(is_valid_partial_order(order));
		return order; // NVRO
	}

	// This function internally reorders the nodes in preorder, then recurses on each component of the graph.
	// should_place_node_at_the_end_of_the_order is called with the id of some node of the component and the function should decide
	// whether this component is placed at the end of the order or at the front.
	// If the relative component order does not matter, then let should_place_node_at_the_end_of_the_order always return false.
	//
	// compute_connected_graph_order should order the nodes in each component. The order should map node IDs in the graph that is given to input node IDs.
	template<class ComputeConnectedGraphOrder, class ShouldPlaceNodeAtTheEndOfTheOrder>
	ArrayIDIDFunc reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDIDFunc input_node_id,
		ArrayIDFunc<int> arc_weight,
		const ComputeConnectedGraphOrder&compute_connected_graph_order,
		const ShouldPlaceNodeAtTheEndOfTheOrder&should_place_node_at_the_end_of_the_order
	){

		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();

		// We first reorder the graph nodes in preorder
		auto x = compute_preorder(compute_successor_function(tail, head));
		auto& preorder = x.first;
		auto& num_components = x.second;
		//TODO if preorder returns just one component skip all of the reordering stuff. actually we should figure out  a way to do biconnected immediately and never do connected components. saves another second

		if (num_components == 1) {
			return compute_trivial_graph_order_if_graph_is_trivial(std::move(tail), std::move(head), std::move(input_node_id), std::move(arc_weight), compute_connected_graph_order);
		}

		{
			auto inv_preorder = inverse_permutation(preorder);
			tail = chain(std::move(tail), inv_preorder);
			head = chain(std::move(head), inv_preorder);
			input_node_id = chain(preorder, std::move(input_node_id));
		}

		// We then sort the arcs accordingly

		{
			auto p = sort_arcs_first_by_tail_second_by_head(tail, head);
			tail = chain(p, std::move(tail));
			head = chain(p, std::move(head));
			arc_weight = chain(p, std::move(arc_weight));
		}

		assert(is_symmetric(tail, head));

		ArrayIDIDFunc order(node_count, input_node_id.image_count());
		int order_begin = 0;
		int order_end = node_count;
		
		auto get_sub_order_begin = [&](int node_begin, int node_end) {
			int sub_node_count = node_end - node_begin;
#ifndef NDEBUG
			{
				bool r = should_place_node_at_the_end_of_the_order(preorder(node_begin));
				for (int x = node_begin; x < node_end; ++x) {
					assert(r == should_place_node_at_the_end_of_the_order(preorder(x)));
				}
			}
#endif

			int sub_order_begin = order_begin;
			if (should_place_node_at_the_end_of_the_order(preorder(node_begin))) {
				order_end -= sub_node_count;
				sub_order_begin = order_end;
				assert(order_begin <= order_end);
			}
			else {
				assert(order_begin + sub_node_count <= order_end);
				order_begin += sub_node_count;
			}
			return sub_order_begin;
		};


		struct SubProblem {
			int node_begin, node_end, arc_begin, arc_end, sub_order_begin;
			int node_count() const { return node_end - node_begin; }
		};
		std::vector<SubProblem> big, small;


		// By reordering the nodes in preorder, we can guarentee, that the nodes of every component are from a coninous range.
		// As we sorted the arcs this is also true for the arcs.
		// We identify components by marking the node in each component with the minimum ID.
		// We do this using the following observation, if an arc (u,v) exists with u<v then v is not such a node
		BitIDFunc component_begin(node_count);
		component_begin.fill(true);
		for(int i=0; i<arc_count; ++i)
			if(head(i) < tail(i))
				component_begin.set(tail(i), false);

		{
			int node_begin = 0;
			int arc_begin = 0;
			for (int node_end = 1; node_end < node_count; ++node_end) {
				if (component_begin(node_end)) {
					int arc_end = arc_begin;
					while (arc_end < arc_count && tail(arc_end) < node_end) {
						++arc_end;
					}
					int sub_order_begin = get_sub_order_begin(node_begin, node_end);
					SubProblem sp = {node_begin, node_end, arc_begin, arc_end, sub_order_begin};
					if (sp.node_count() > TASK_SPAWN_CUTOFF) big.push_back(sp); else small.push_back(sp);
					node_begin = node_end;
					arc_begin = arc_end;
				}
			}

			int sub_order_begin = get_sub_order_begin(node_begin, node_count);
			SubProblem sp = {node_begin, node_count, arc_begin, arc_count, sub_order_begin};
			if (sp.node_count() > TASK_SPAWN_CUTOFF) big.push_back(sp); else small.push_back(sp);
		}


#ifndef NDEBUG
			std::vector<bool> nodes_covered((unsigned long) node_count, false);
			std::vector<bool> order_covered((unsigned long) node_count, false);
			auto cover = [&](const SubProblem sp) {
				for (int u = sp.node_begin; u < sp.node_end; ++u) {
					assert(!nodes_covered[u]);
					nodes_covered[u] = true;
				}
				int sub_node_count = sp.node_end - sp.node_begin;
				for (int u = sp.sub_order_begin; u < sp.sub_order_begin + sub_node_count; ++u) {
					assert(!order_covered[u]);
					order_covered[u] = true;
				}
			};
			for (const SubProblem sp : big)
				cover(sp);
			for (const SubProblem sp : small)
				cover(sp);
			assert(std::all_of(nodes_covered.begin(), nodes_covered.end(), [](const bool& x) { return x; }));
			assert(std::all_of(order_covered.begin(), order_covered.end(), [](const bool& x) { return x; }));
#endif

		//auto on_new_component = [&tail, &head, &input_node_id, &arc_weight, &order, &compute_connected_graph_order](SubProblem& sub_problem){
		auto on_new_component = [&](SubProblem sub_problem){
			int node_begin = sub_problem.node_begin; int node_end = sub_problem.node_end;
			int arc_begin = sub_problem.arc_begin; int arc_end = sub_problem.arc_end;
			int sub_order_begin = sub_problem.sub_order_begin;
			auto sub_node_count = node_end - node_begin;
			auto sub_arc_count = arc_end - arc_begin;

			auto sub_tail = id_id_func(
					sub_arc_count, sub_node_count,
					[&](int x){
						return tail(arc_begin + x) - node_begin;
					}
			);
			auto sub_head = id_id_func(
					sub_arc_count, sub_node_count,
					[&](int x){
						return head(arc_begin + x) - node_begin;
					}
			);
			auto sub_input_node_id = id_id_func(
					sub_node_count, input_node_id.image_count(),
					[&](int x){
						return input_node_id(node_begin + x);
					}
			);
			auto sub_arc_weight = id_func(
					sub_arc_count,
					[&](int x){
						return arc_weight(x + arc_begin);
					}
			);
			assert(is_symmetric(sub_tail, sub_head));
			assert(!has_multi_arcs(sub_tail, sub_head));
			assert(is_loop_free(sub_tail, sub_head));

			auto sub_order = compute_trivial_graph_order_if_graph_is_trivial(sub_tail, sub_head, sub_input_node_id, sub_arc_weight, compute_connected_graph_order);
			for (int i = 0; i < sub_node_count; ++i) {
				order[sub_order_begin + i] = sub_order(i);
			}
		};



		tbb::task_group tg;
		if (big.size() == 1 && small.size() < 200000) {
			small.push_back(big.front());
			big.clear();
		}
		//std::sort(big.begin(), big.end(), [](const auto& a, const auto& b) { return a.node_count() > b.node_count(); });
		for (const SubProblem sp : big) {
			tg.run(std::bind(on_new_component, sp));
		}
		tg.run_and_wait([&]() {
			std::for_each(small.begin(), small.end(), on_new_component);
		});
		assert(order_begin == order_end);
		assert(is_valid_partial_order(order));
		return order; // NVRO
	}

	template<class ComputeCut>
	ArrayIDIDFunc compute_nested_dissection_expanded_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDIDFunc input_node_id,
		ArrayIDIDFunc input_arc_id,
		ArrayIDFunc<int> arc_weight, 
		const ComputeCut& compute_cut
	){
		assert(is_connected(tail, head));
		int node_count = tail.image_count();
		int arc_count = tail.preimage_count();
		if (arc_count == 0) return {};

		std::deque<int> cut;
		if (arc_count + 1 == node_count) {
			//handle trees (and paths)
			//this is in O(n^2) for stars but should work fine...

			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);

			std::vector<int> children(node_count, 1);
			std::vector<pair<int, int>> order = {{0, -1}};

			for (unsigned i = 0; i < order.size(); i++) {
				const int cur = order[i].first;
				const int prev = order[i].second;

				for (int j = out_arc.range_begin[cur]; j < out_arc.range_begin[cur + 1]; j++) {
					if (head(j) == prev) continue;
					order.push_back({head(j), cur});
				}
			}
			assert((int)order.size() == node_count);
			for (int i = (int)order.size() - 1; i >= 0; i--) {
				const int cur = order[i].first;
				const int prev = order[i].second;

				for (int j = out_arc.range_begin[cur]; j < out_arc.range_begin[cur + 1]; j++) {
					if (head(j) == prev) continue;
					children[cur] += children[head(j)];
				}
			}
			int best_arc = 0;
			int best_score = -1;
			for (int cur_arc = 0; cur_arc < arc_count; cur_arc++) {
				int cur_score = std::min(children[head(cur_arc)], tail(cur_arc));
				cur_score = min(cur_score, node_count - cur_score);
				if (cur_score >= best_score) {
					best_score = cur_score;
					best_arc = cur_arc;
				}
			}
			cut = {best_arc, back_arc(best_arc)};
		} else {
			cut = compute_cut(tail, head, input_node_id, arc_weight);
		}
		assert(cut.size() > 0);

		ArrayIDIDFunc order(arc_count, input_arc_id.image_count());
		int order_begin = 0;
		int order_end = arc_count;

		BitIDFunc keep_arc_flag(arc_count);
		keep_arc_flag.fill(true);
		for(auto x : cut) {
			order[order_begin] = input_arc_id(x);
			order_begin++;
			keep_arc_flag.set(x, false);
		}

		int new_arc_count = count_true(keep_arc_flag);
		input_arc_id = keep_if(keep_arc_flag, new_arc_count, std::move(input_arc_id));
		tail = keep_if(keep_arc_flag, new_arc_count, std::move(tail));
		head = keep_if(keep_arc_flag, new_arc_count, std::move(head));
		arc_weight = keep_if(keep_arc_flag, new_arc_count, std::move(arc_weight));
		arc_count = new_arc_count;

		auto x = compute_preorder(compute_successor_function(tail, head));
		auto& preorder = x.first;

		{
			auto inv_preorder = inverse_permutation(preorder);
			tail = chain(std::move(tail), inv_preorder);
			head = chain(std::move(head), inv_preorder);
			input_node_id = chain(preorder, std::move(input_node_id));
		}

		{
			auto p = sort_arcs_first_by_tail_second_by_head(tail, head);
			tail = chain(p, std::move(tail));
			head = chain(p, std::move(head));
			arc_weight = chain(p, std::move(arc_weight));
			input_arc_id = chain(p, std::move(input_arc_id));
		}

		auto get_sub_order_begin = [&](int arc_begin, int arc_end) {
			int sub_arc_count = arc_end - arc_begin;

			int sub_order_begin = order_begin;
			assert(order_begin + sub_arc_count <= order_end);
			order_begin += sub_arc_count;
			return sub_order_begin;
		};

		struct SubProblem {
			int node_begin, node_end, arc_begin, arc_end, sub_order_begin;
			int node_count() const { return node_end - node_begin; }
			int arc_count() const { return arc_end - arc_begin; }
		};
		std::vector<SubProblem> big, small;

		// By reordering the nodes in preorder, we can guarentee, that the nodes of every component are from a coninous range.
		// As we sorted the arcs this is also true for the arcs.
		// We identify components by marking the node in each component with the minimum ID.
		// We do this using the following observation, if an arc (u,v) exists with u<v then v is not such a node
		BitIDFunc component_begin(node_count);
		component_begin.fill(true);
		for(int i=0; i<arc_count; ++i)
			component_begin.set(std::max(head(i), tail(i)), false);
		{
			int node_begin = 0;
			int arc_begin = 0;
			for (int node_end = 1; node_end < node_count; ++node_end) {
				if (component_begin(node_end)) {
					int arc_end = arc_begin;
					while (arc_end < arc_count && tail(arc_end) < node_end) {
						++arc_end;
					}
					int sub_order_begin = get_sub_order_begin(arc_begin, arc_end);
					SubProblem sp = {node_begin, node_end, arc_begin, arc_end, sub_order_begin};
					if (sp.node_count() > TASK_SPAWN_CUTOFF) {
						big.push_back(sp);
					} else {
						small.push_back(sp);
					}
					node_begin = node_end;
					arc_begin = arc_end;
				}
			}

			int sub_order_begin = get_sub_order_begin(arc_begin, arc_count);
			SubProblem sp = {node_begin, node_count, arc_begin, arc_count, sub_order_begin};
			if (sp.node_count() > TASK_SPAWN_CUTOFF) {
				big.push_back(sp);
			} else {
				small.push_back(sp);
			}
		}

		//auto on_new_component = [&tail, &head, &input_node_id, &arc_weight, &order, &compute_connected_graph_order](SubProblem& sub_problem){
		auto on_new_component = [&](SubProblem sub_problem){
			int node_begin = sub_problem.node_begin; int node_end = sub_problem.node_end;
			int arc_begin = sub_problem.arc_begin; int arc_end = sub_problem.arc_end;
			int sub_order_begin = sub_problem.sub_order_begin;
			auto sub_node_count = node_end - node_begin;
			auto sub_arc_count = arc_end - arc_begin;

			auto sub_tail = id_id_func(
					sub_arc_count, sub_node_count,
					[&](int x){
						return tail(arc_begin + x) - node_begin;
					}
			);
			auto sub_head = id_id_func(
					sub_arc_count, sub_node_count,
					[&](int x){
						return head(arc_begin + x) - node_begin;
					}
			);
			auto sub_input_node_id = id_id_func(
					sub_node_count, input_node_id.image_count(),
					[&](int x){
						return input_node_id(node_begin + x);
					}
			);
			auto sub_input_arc_id = id_id_func(
					sub_arc_count, input_arc_id.image_count(),
					[&](int x){
						return input_arc_id(arc_begin + x);
					}
			);
			auto sub_arc_weight = id_func(
					sub_arc_count,
					[&](int x){
						return arc_weight(arc_begin + x);
					}
			);
			assert(is_loop_free(sub_tail, sub_head));

			auto sub_order = compute_nested_dissection_expanded_graph_order(sub_tail, sub_head, sub_input_node_id, sub_input_arc_id, sub_arc_weight, compute_cut);
			for (int i = 0; i < sub_arc_count; ++i) {
				order[sub_order_begin + i] = sub_order(i);
			}
		};

		tbb::task_group tg;
		if (big.size() == 1 && small.size() < 200000) {
			small.push_back(big.front());
			big.clear();
		}
		//std::sort(big.begin(), big.end(), [](const auto& a, const auto& b) { return a.node_count() > b.node_count(); });
		for (const SubProblem sp : big) {
			tg.run(std::bind(on_new_component, sp));
		}
		tg.run_and_wait([&]() {
			std::for_each(small.begin(), small.end(), on_new_component);
		});
		assert(order_begin == order_end);
		assert(is_valid_partial_order(order));
		return order; // NVRO*/
	}

	template<class ComputeCut>
	ArrayIDIDFunc compute_nested_dissection_expanded_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDFunc<int> arc_weight, 
		const ComputeCut& compute_cut
	){
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();
		auto input_node_id = identity_permutation(node_count);
		auto input_arc_id = identity_permutation(arc_count);
		return compute_nested_dissection_expanded_graph_order(tail, head, input_node_id, input_arc_id, arc_weight, compute_cut);
	}

	template<class ComputeSeparator, class ComputePartOrder>
	ArrayIDIDFunc compute_nested_dissection_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDIDFunc input_node_id,
		ArrayIDFunc<int> arc_weight, 
		const ComputeSeparator&compute_separator,
		const ComputePartOrder&compute_graph_part_order
	){
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();
		auto separator = compute_separator(tail, head, input_node_id, arc_weight);
		assert(separator.size() > 0);

		BitIDFunc in_separator(node_count);
		in_separator.fill(false);
		for(auto x:separator)
			in_separator.set(x, true);

		BitIDFunc keep_arc_flag = id_func(
			arc_count, 
			[&](int a){
				return in_separator(tail(a)) == in_separator(head(a));
			}
		);
		
		if((int)separator.size() == node_count){
			keep_arc_flag.fill(false);
		}


		int new_arc_count = count_true(keep_arc_flag);
		tail = keep_if(keep_arc_flag, new_arc_count, std::move(tail));
		head = keep_if(keep_arc_flag, new_arc_count, std::move(head));
		arc_weight = keep_if(keep_arc_flag, new_arc_count, std::move(arc_weight));

		assert(is_symmetric(tail, head));

		return reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			std::move(tail), std::move(head), 
			std::move(input_node_id), std::move(arc_weight), 
			compute_graph_part_order, std::move(in_separator)
		);
	}

	template<class ComputeSeparator>
	ArrayIDIDFunc compute_nested_dissection_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDIDFunc input_node_id,
		ArrayIDFunc<int> arc_weight, 
		const ComputeSeparator&compute_separator
	){
		auto compute_graph_part_order = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head, 
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int>a_arc_weight
		){
			return compute_nested_dissection_graph_order(
				std::move(a_tail), std::move(a_head), 
				std::move(a_input_node_id), std::move(a_arc_weight), 
				compute_separator
			);
		};
		return compute_nested_dissection_graph_order(tail, head, input_node_id, arc_weight, compute_separator, compute_graph_part_order);
	}

	template<class ComputeCoreGraphOrder>
	ArrayIDIDFunc compute_graph_order_with_large_degree_three_independent_set_at_the_begin(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDIDFunc input_node_id, ArrayIDFunc<int>arc_weight,
		const ComputeCoreGraphOrder&compute_core_graph_order
	){
		const int node_count = tail.image_count();
		int arc_count = tail.preimage_count();

		BitIDFunc in_independent_set(node_count);
		in_independent_set.fill(false);
		
		auto inv_tail = invert_sorted_id_id_func(tail);
		auto degree = id_func(
			node_count, 
			[&](int x){
				return inv_tail(x).end() - inv_tail(x).begin();
			}
		);

		auto back_arc = compute_back_arc_permutation(tail, head);

		for(auto x=0; x<node_count; ++x){
			if(degree(x) == 3){
				bool neighbor_in_set = false;
				for(auto xy:inv_tail(x)){
					auto y = head(xy);
					if(in_independent_set(y)){
						neighbor_in_set = true;
						break;
					}
				}
				if(!neighbor_in_set)
					in_independent_set.set(x, true);
			}
		}

		for(auto c=0; c<node_count; ++c){
			if(in_independent_set(c)){
				auto iter = inv_tail(c).begin();
				auto 
					cx = *iter++,
					cy = *iter++,
					cz = *iter++;
				assert(iter == inv_tail(c).end());
				auto
					xc = back_arc(cx),
					yc = back_arc(cy),
					zc = back_arc(cz);
				auto
					x = head(cx),
					y = head(cy),
					z = head(cz);

				head[xc] = y;
				head[yc] = z;
				head[zc] = x;

 				tail[cx] = x;
				tail[cy] = y;
				tail[cz] = z;

				head[cx] = z;
				head[cy] = x;
				head[cz] = y;
			}
		}

		// Remove multi arcs
		{
			auto keep_flag = identify_non_multi_arcs(tail, head);
			
			arc_count = count_true(keep_flag);
			tail = keep_if(keep_flag, arc_count, std::move(tail));
			head = keep_if(keep_flag, arc_count, std::move(head));
			arc_weight = keep_if(keep_flag, arc_count, std::move(arc_weight));
		}

		assert(is_symmetric(tail, head));

		return reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			std::move(tail), std::move(head), 
			std::move(input_node_id), std::move(arc_weight),
			compute_core_graph_order, ~std::move(in_independent_set)
		);
	}

	template<class ComputeCoreGraphOrder>
	ArrayIDIDFunc compute_graph_order_with_degree_two_chain_at_the_begin(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDIDFunc input_node_id, ArrayIDFunc<int>arc_weight,
		const ComputeCoreGraphOrder&compute_core_graph_order
	){
		const int node_count = tail.image_count();
		int arc_count = tail.preimage_count();

		assert(tail.preimage_count() == arc_count);
		assert(head.preimage_count() == arc_count);
		assert(arc_weight.preimage_count() == arc_count);
		assert(input_node_id.preimage_count() == node_count);
		assert(tail.image_count() == node_count);
		assert(head.image_count() == node_count);


		//auto degree = compute_histogram(tail);

		assert(is_symmetric(tail, head));
		assert(!has_multi_arcs(tail, head)); 
		assert(is_loop_free(tail, head));


		BitIDFunc keep_flag(arc_count);
		keep_flag.fill(true);

		auto inv_tail = invert_sorted_id_id_func(tail);
		auto degree = id_func(
			node_count, 
			[&](int x){
				return inv_tail(x).end() - inv_tail(x).begin();
			}
		);

		BitIDFunc node_in_core = id_func(
			node_count, 
			[&](int x){
				return degree(x) > 2;
			}
		);

		for(auto first_arc=0; first_arc<arc_count; ++first_arc){
			auto 
				chain_begin = tail(first_arc),
				chain_now = head(first_arc);

			if(degree(chain_begin) > 2 && degree(chain_now) <= 2){
				auto chain_prev = chain_begin;
				auto chain_weight = arc_weight(first_arc);

				int arc_prev_to_now = first_arc;

				while(degree(chain_now) == 2){
					for(auto arc_now_to_next : inv_tail(chain_now)){
						auto chain_next = head(arc_now_to_next);
						if(chain_next != chain_prev){
							chain_weight += arc_weight(arc_now_to_next);
						
							chain_prev = chain_now;
							chain_now = chain_next;
							arc_prev_to_now = arc_now_to_next;
							break;
						}
					}
				}

				assert(arc_prev_to_now != -1);

				auto chain_end = chain_now;
				auto last_arc = arc_prev_to_now;
				
				assert(degree(chain_end) != 0);

				if(degree(chain_end) == 1){
					// Dead end, no shortcut needed
					keep_flag.set(first_arc, false);
					for(auto back_arc_for_first_arc:inv_tail(head(first_arc))){
						if(head(back_arc_for_first_arc) == tail(first_arc)){
							keep_flag.set(back_arc_for_first_arc, false);
							break;
						}
					}
				}else{
					if(chain_begin == chain_end){
						// The chain is a loop, no shortcut needed
						keep_flag.set(first_arc, false);
						keep_flag.set(last_arc, false);
					}else{
						// A normal chain, shortcut needed
						head[first_arc] = chain_end;
						arc_weight[first_arc] = chain_weight;
						keep_flag.set(last_arc, false);
					}
				}
			}
		}

		// Remove arcs between chains and the rest graph
		{
			arc_count = count_true(keep_flag);
			tail = keep_if(keep_flag, arc_count, std::move(tail));
			head = keep_if(keep_flag, arc_count, std::move(head));
			arc_weight = keep_if(keep_flag, arc_count, std::move(arc_weight));
		}

		// Remove multi arcs
		{
			keep_flag = identify_non_multi_arcs(tail, head);
			arc_count = count_true(keep_flag);
			tail = keep_if(keep_flag, arc_count, std::move(tail));
			head = keep_if(keep_flag, arc_count, std::move(head));
			arc_weight = keep_if(keep_flag, arc_count, std::move(arc_weight));
		}

		#ifndef NDEBUG

		assert(is_symmetric(tail, head));
		assert(!has_multi_arcs(tail, head));
		assert(is_loop_free(tail, head));

		{
			auto degree = compute_histogram(tail);
			int not_in_core_count = 0;
			for(int x=0; x<node_count; ++x){
				if(!node_in_core(x)){
					assert(degree(x) <= 2);
					++not_in_core_count;
				}
			}

			if(not_in_core_count != node_count){
				UnionFind uf(node_count);
				for(int xy=0; xy<arc_count; ++xy){
					auto x = tail(xy), y = head(xy);
					if(node_in_core(x) && node_in_core(y)){
						uf.unite(x, y);
					}

				}
		
				assert(uf.component_count() == not_in_core_count+1);
			}
		}

		#endif

		#ifdef NDEBUG
		return reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			std::move(tail), std::move(head), 
			std::move(input_node_id), std::move(arc_weight),
			compute_core_graph_order, node_in_core
		);
		#else

		auto order = reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			tail, head, 
			input_node_id, arc_weight,
			compute_core_graph_order, node_in_core
		);

		{
			ArrayIDIDFunc super_to_sub(input_node_id.image_count(), node_count);
			for(int x=0; x<node_count; ++x)
				super_to_sub[input_node_id(x)] = x;
			auto local_order = order;
			for(auto&x:local_order)
				x = super_to_sub[x];
			for(int p=1; p<node_count; ++p){
				auto x = local_order(p-1), y = local_order(p); 
				assert(
					   (!node_in_core(x) && !node_in_core(y))
					|| ( node_in_core(x) &&  node_in_core(y))
					|| (!node_in_core(x) &&  node_in_core(y))
				);
			}
		}
		return order;
		#endif

	
	}

	template<class ComputeConnectedGraphOrder>
	ArrayIDIDFunc compute_graph_order_with_largest_biconnected_component_at_the_end(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDIDFunc input_node_id, ArrayIDFunc<int>arc_weight,
		const ComputeConnectedGraphOrder&compute_component_graph_order
	){
		int node_count = tail.image_count();
		int arc_count = tail.preimage_count();

		// Determine the nodes incident to largest biconnected component.
		// Large in terms of many arcs.

		BitIDFunc node_in_largest_biconnected_component(node_count);
		
		{
			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);

			auto arc_component = compute_biconnected_components(out_arc, head, back_arc);
			auto largest_component = max_preimage_over_id_func(compute_histogram(arc_component));
			node_in_largest_biconnected_component.fill(false);
			for(int i=0; i<arc_count; ++i){
				if(arc_component(i) == largest_component){
					node_in_largest_biconnected_component.set(tail(i), true);
					node_in_largest_biconnected_component.set(head(i), true);
				}
			}
		}


		// Remove all arcs that enter or leave the largest biconnected component

		{
			BitIDFunc keep_flag = id_func(
				arc_count, 
				[&](int a){
					return node_in_largest_biconnected_component(tail(a)) == node_in_largest_biconnected_component(head(a));
				}
			);

			arc_count = count_true(keep_flag);
			tail = keep_if(keep_flag, arc_count, std::move(tail));
			head = keep_if(keep_flag, arc_count, std::move(head));
			arc_weight = keep_if(keep_flag, arc_count, std::move(arc_weight));
		}
		return reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			std::move(tail), std::move(head), 
			std::move(input_node_id), std::move(arc_weight),
			compute_component_graph_order, std::move(node_in_largest_biconnected_component)
		);
	}





	inline
	void make_graph_simple(ArrayIDIDFunc&tail, ArrayIDIDFunc&head, ArrayIDFunc<int>&arc_weight){
		const int node_count = tail.image_count();
		int arc_count = tail.preimage_count();
		(void)node_count;

		assert(is_symmetric(tail, head));

		// Sort arcs
		{
			auto p = sort_arcs_first_by_tail_second_by_head(tail, head);
			tail = chain(p, std::move(tail));
			head = chain(p, std::move(head));
			arc_weight = chain(p, std::move(arc_weight));
		}
		
		// Remove multi-arcs and loops (requires sorted arcs)
		{
			BitIDFunc keep_flag = id_func(
				arc_count,
				[&](int i)->bool{
					if(i!=0){
						if(tail(i-1) == tail(i) && head(i-1) == head(i))
							return false;
					}
					if(tail(i) == head(i))
						return false;
					return true;
				}
			);
			int new_arc_count = count_true(keep_flag);
			tail = keep_if(keep_flag, new_arc_count, std::move(tail));
			head = keep_if(keep_flag, new_arc_count, std::move(head));
			arc_weight = keep_if(keep_flag, new_arc_count, std::move(arc_weight));
		}

		assert(is_loop_free(tail, head));
		assert(!has_multi_arcs(tail, head));
		assert(is_symmetric(tail, head));
	}


	template<class ComputeSeparator>
	ArrayIDIDFunc compute_nested_dissection_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDFunc<int> arc_weight, 
		const ComputeSeparator&compute_separator
	){
		const int node_count = tail.image_count();

		make_graph_simple(tail, head, arc_weight);

		auto input_node_id = identity_permutation(node_count);

		auto compute_order = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
		){
			return compute_nested_dissection_graph_order(
				std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), 
				compute_separator
			);
		};

		auto order = reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			std::move(tail), std::move(head), std::move(input_node_id), std::move(arc_weight), 
			compute_order, [](int){return false;}
		);

		assert(is_permutation(order));

		return order; // NVRO
	}

	template<class ComputeSeparator>
	ArrayIDIDFunc compute_cch_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDIDFunc input_node_id,
		ArrayIDFunc<int> arc_weight, 
		const ComputeSeparator&compute_separator
	){

		make_graph_simple(tail, head, arc_weight);

		auto orderer4 = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
		){
			return compute_nested_dissection_graph_order(
				std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), 
				compute_separator
			);
		};

		/*auto orderer3 = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
		){
			return compute_graph_order_with_large_degree_three_independent_set_at_the_begin(
				std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), 
				orderer4
			);
		};*/


		auto orderer2 = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
		){
			return compute_graph_order_with_degree_two_chain_at_the_begin(
				std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), 
				orderer4
			);
		};

		auto orderer1 = [&](
			ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
			ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
		){
			return compute_graph_order_with_largest_biconnected_component_at_the_end(
				std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), 
				orderer2
			);
		};

		auto order = reorder_nodes_in_preorder_and_compute_unconnected_graph_order_if_component_is_non_trivial(
			tail, head, input_node_id, arc_weight, 
			orderer1, [](int){return false;}
		);

		assert(is_permutation(order));

		return order; // NVRO
	}

	template<class ComputeSeparator>
	ArrayIDIDFunc compute_cch_graph_order(
		ArrayIDIDFunc tail, ArrayIDIDFunc head,
		ArrayIDFunc<int> arc_weight, 
		const ComputeSeparator&compute_separator
	){
		return compute_cch_graph_order(std::move(tail), std::move(head), identity_permutation(tail.image_count()), std::move(arc_weight), compute_separator);
	}

	class ComputeConstantSeparator{
	public:
		explicit ComputeConstantSeparator(std::vector<int>sep):sep(std::move(sep)){}

		template<class Tail, class Head, class InputNodeID, class ArcWeight>
		std::vector<int> operator()(const Tail&, const Head&, const InputNodeID&, const ArcWeight&)const{
			return sep;
		}
	private:
		std::vector<int> sep;
	};

	template<class ComputeSeparator>
	ArrayIDIDFunc compute_cch_graph_order_given_top_level_separator(
		ArrayIDIDFunc tail, ArrayIDIDFunc head, 
		ArrayIDFunc<int> arc_weight, std::vector<int>top_level_separator,
		const ComputeSeparator&compute_separator
	){
		const int node_count = tail.image_count();

		make_graph_simple(tail, head, arc_weight);

		auto input_node_id = identity_permutation(node_count);

		return compute_nested_dissection_graph_order(
			tail, head, input_node_id, arc_weight, 
			ComputeConstantSeparator(std::move(top_level_separator)), 
			[&](
				ArrayIDIDFunc a_tail, ArrayIDIDFunc a_head,
				ArrayIDIDFunc a_input_node_id, ArrayIDFunc<int> a_arc_weight
			){
				return compute_cch_graph_order(std::move(a_tail), std::move(a_head), std::move(a_input_node_id), std::move(a_arc_weight), compute_separator);
			}

		);
	}
}
#endif
