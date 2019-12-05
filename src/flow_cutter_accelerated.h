#ifndef FLOW_CUTTER_WITH_CLUSTERING_H
#define FLOW_CUTTER_WITH_CLUSTERING_H


#include "tiny_id_func.h"
#include "array_id_func.h"
#include "id_string.h"
#include "id_func.h"
#include "dijkstra.h"
#include "min_max.h"
#include "flow_cutter_dinic.h"
#include <vector>
#include <algorithm>
#include <sstream>
#include <random>
#include <memory>
#include <atomic>
#include <array>

#include "flow_cutter_config.h"
#include "permutation.h"
#include "timer.h"
#include "geo_pos.h"
#include <iostream>

#include <iterator>

#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>

namespace flow_cutter_accelerated{

	struct TerminalPair {
		std::vector<int> source_list;
		std::vector<int> target_list;
	};


	template<class Tail, class Head, class BackArc, class ArcWeight, class Capacity, class OutArc>
	struct Graph{
		Graph(
			Tail tail,
			Head head,
			BackArc back_arc,
			ArcWeight arc_weight,
			Capacity capacity,
			OutArc out_arc
		):
			tail(std::move(tail)),
			head(std::move(head)),
			back_arc(std::move(back_arc)),
			arc_weight(std::move(arc_weight)),
			capacity(std::move(capacity)),
			out_arc(std::move(out_arc)){}

		Tail tail;
		Head head;
		BackArc back_arc;
		//NodeWeight node_weight;
		ArcWeight arc_weight;
		Capacity capacity;
		OutArc out_arc;

		int node_count()const{
			return tail.image_count();
		}

		int arc_count()const{
			return tail.preimage_count();
		}
	};

	//! Each threads needs its own TemporaryData object.
	struct TemporaryData{
		TemporaryData(){}
		explicit TemporaryData(int node_count):
			node_space(node_count){}
		ArrayIDFunc<int>node_space;
	};

	template<class Tail, class Head, class BackArc, class ArcWeight, class Capacity, class OutArc>
	Graph<Tail, Head, BackArc, ArcWeight, Capacity, OutArc>
		make_graph(
			Tail tail, Head head, BackArc back_arc,
			ArcWeight arc_weight, Capacity capacity, OutArc out_arc
		){
		return {std::move(tail), std::move(head), std::move(back_arc), std::move(arc_weight), std::move(capacity), std::move(out_arc)};
	}

	template<class Tail, class Head, class BackArc, class OutArc>
	Graph<
		Tail, Head, BackArc,
		ConstIntIDFunc<1>, ConstIntIDFunc<1>,
		OutArc
	>
		make_graph(
			const Tail&tail, const Head&head,
			const BackArc&back_arc, const OutArc&out_arc
		){
		return {
			std::move(tail), std::move(head), std::move(back_arc),
			ConstIntIDFunc<1>(tail.preimage_count()), ConstIntIDFunc<1>(tail.preimage_count()),
			std::move(out_arc)
		};
	}

	int select_random_node(const std::vector<int>& node_list, int seed) {
		std::mt19937 rng(seed);
		std::uniform_int_distribution<int> dist(0, node_list.size()-1);
		return node_list[dist(rng)];
	}

	class PseudoDepthFirstSearch{
	public:
		template<class Graph, class WasNodeSeen, class SeeNode, class ShouldFollowArc, class OnNewArc>
		bool operator()(
			const Graph&graph, TemporaryData&tmp, int source_node,
			const WasNodeSeen&was_node_seen, const SeeNode&see_node,
			const ShouldFollowArc&should_follow_arc, const OnNewArc&on_new_arc
		)const{
			int stack_end = 1;
			auto&stack = tmp.node_space;
			stack[0] = source_node;

			while(stack_end != 0){
				int x = stack[--stack_end];
				for(auto xy : graph.out_arc(x)){
					on_new_arc(xy);
					int y = graph.head(xy);
					if(!was_node_seen(y)){
						if(should_follow_arc(xy)){
							if(!see_node(y))
								return false;
							stack[stack_end++] = y;
						}
					}
				}
			}

			return true;
		}
	};

	class BreadthFirstSearch{
	public:
		template<class Graph, class WasNodeSeen, class SeeNode, class ShouldFollowArc, class OnNewArc>
		bool operator()(
			const Graph&graph, TemporaryData&tmp, int source_node,
			const WasNodeSeen&was_node_seen, const SeeNode&see_node,
			const ShouldFollowArc&should_follow_arc, const OnNewArc&on_new_arc
		)const{
			int queue_begin = 0, queue_end = 1;
			auto&queue = tmp.node_space;
			queue[0] = source_node;
			while(queue_begin != queue_end){
				int x = queue[queue_begin++];
				for(auto xy : graph.out_arc(x)){
					on_new_arc(xy);
					int y = graph.head(xy);
					if(!was_node_seen(y)){
						if(should_follow_arc(xy)){
							if(!see_node(y))
								return false;
							queue[queue_end++] = y;
						}
					}
				}
			}

			return true;
		}

		template<class Graph, class WasNodeSeen, class SeeNode, class ShouldFollowArc, class OnNewArc, class ForwardIt>
		bool operator()(
				const Graph&graph, TemporaryData&tmp, ForwardIt sources_begin, ForwardIt sources_end,
				const WasNodeSeen&was_node_seen, const SeeNode&see_node,
				const ShouldFollowArc&should_follow_arc, const OnNewArc&on_new_arc
		)const{
			for (; sources_begin != sources_end; ++sources_begin) {
				if (!operator()(graph, tmp, *sources_begin, was_node_seen, see_node, should_follow_arc, on_new_arc))
					return false;
			}

			return true;
		}
	};

	struct UnitFlow{
		UnitFlow(){}
		explicit UnitFlow(int preimage_count):flow(preimage_count){}

		void clear(){
			flow.fill(1);
		}

		int preimage_count()const{
			return flow.preimage_count();
		}

		template<class Graph>
		void increase(const Graph&graph, int a){
			auto f = flow(a);
			assert((f == 0 || f == 1) && "Flow is already maximum; can not be increased");
			assert(flow(graph.back_arc(a)) == 2-f && "Back arc has invalid flow");
			++f;
			flow.set(a, f);
			flow.set(graph.back_arc(a), 2-f);
		}

		template<class Graph>
		void decrease(const Graph&graph, int a){
			auto f = flow(a);
			assert((f == 1 || f == 2) && "Flow is already minimum; can not be decreased");
			assert(flow(graph.back_arc(a)) == 2-f && "Back arc has invalid flow");
			--f;
			flow.set(a, f);
			flow.set(graph.back_arc(a), 2-f);
		}

		int operator()(int a)const{
			return static_cast<int>(flow(a))-1;
		}

		void swap(UnitFlow&o){
			flow.swap(o.flow);
		}

		TinyIntIDFunc<2>flow;
	};

	class BasicNodeSet{
	public:
		template<class Graph>
		explicit BasicNodeSet(const Graph&graph):
			last_grow_index(0),
			node_count_inside_(0),
			inside_flag(graph.node_count()),
			extra_nodes(){}

		void clear(){
			node_count_inside_ = 0;
			inside_flag.fill(false);
			extra_nodes.clear();
		}

		bool can_grow()const{
			return !extra_nodes.empty();
		}

		template<class Graph, class SearchAlgorithm, class OnNewNode, class ShouldFollowArc, class OnNewArc>
		void grow(
			const Graph&graph,
			TemporaryData&tmp,
			const SearchAlgorithm&search_algo,
			const OnNewNode&on_new_node, // on_new_node(x) is called for every node x. If it returns false then the search is stopped, if it returns true it continues
			const ShouldFollowArc&should_follow_arc, // is called for a subset of arcs and must say whether the arc sould be followed
			const OnNewArc&on_new_arc // on_new_arc(xy) is called for ever arc xy with x in the set
		){
			assert(can_grow());

			auto see_node = [&](int x){
				assert(!inside_flag(x));
				inside_flag.set(x, true);
				++this->node_count_inside_;
				return on_new_node(x);
			};

			auto was_node_seen = [&](int x){
				return inside_flag(x);
			};

			if (last_grow_index >= (int)extra_nodes.size()) last_grow_index = 0;

			for (int i = last_grow_index; i < (int)extra_nodes.size(); ++i) {
				if (!search_algo(graph, tmp, extra_nodes[i], was_node_seen, see_node, should_follow_arc, on_new_arc)) {
					last_grow_index = i;
					return;
				}
			}

			for (int i = 0; i < last_grow_index; ++i) {
				if (!search_algo(graph, tmp, extra_nodes[i], was_node_seen, see_node, should_follow_arc, on_new_arc)) {
					last_grow_index = i;
					return;
				}
			}
		}

		template<class Graph>
		void add_node(const Graph &, int x) {
			assert(!inside_flag(x));
			inside_flag.set(x, true);
			++node_count_inside_;
		}

		void remove_node(int x) {
			assert(inside_flag(x));
			inside_flag.set(x, false);
			--node_count_inside_;
		}

		template<class Graph>
		void add_extra_node(const Graph &, int x){
			extra_nodes.push_back(x);
		}

		void copy_nodes_without_extra_nodes(const BasicNodeSet& other) {
			node_count_inside_ = other.node_count_inside_;
			inside_flag = other.inside_flag;
		}

		const std::vector<int>& get_extra_nodes() const {
			return extra_nodes;
		}

		void clear_extra_nodes() {
			extra_nodes.clear();
		}

		bool is_inside(int x) const {
			return inside_flag(x);
		}

		int node_count_inside() const {
			return node_count_inside_;
		}

		int max_node_count_inside() const {
			return inside_flag.preimage_count();
		}

	private:
		int last_grow_index;
		int node_count_inside_;
		BitIDFunc inside_flag;
		std::vector<int> extra_nodes;
	};

	class ReachableNodeSet;

	class AssimilatedNodeSet{
		friend class ReachableNodeSet;
	public:
		template<class Graph>
		explicit AssimilatedNodeSet(const Graph&graph):
			node_set(graph){}

		void clear(){
			node_set.clear();
			front.clear();
		}

		template<class Graph>
		void add_node(const Graph& graph, int x) {
			node_set.add_node(graph, x);
		}

		void remove_node(int x) {
			node_set.remove_node(x);
		}

		template<class Graph>
		void add_extra_node(const Graph &graph, int x){
			node_set.add_extra_node(graph, x);
		}

		const std::vector<int>& get_extra_nodes() const {
			return node_set.get_extra_nodes();
		}

		void clear_extra_nodes() {
			node_set.clear_extra_nodes();
		}

		bool can_grow()const{
			return node_set.can_grow();
		}

		template<class Graph, class SearchAlgorithm, class OnNewNode, class ShouldFollowArc, class OnNewArc, class HasFlow>
		void grow(
			const Graph&graph,
			TemporaryData&tmp,
			const SearchAlgorithm&search_algo,
			const OnNewNode&on_new_node, // on_new_node(x) is called for every node x. If it returns false then the search is stopped, if it returns true it continues
			const ShouldFollowArc&should_follow_arc, // is called for a subset of arcs and must say whether the arc sould be followed
			const OnNewArc&on_new_arc, // on_new_arc(xy) is called for ever arc xy with x in the set
			const HasFlow&has_flow
		){
			auto my_on_new_arc = [&](int xy){
				if(has_flow(xy))
					front.push_back(xy);
				on_new_arc(xy);
			};

			node_set.grow(graph, tmp, search_algo, on_new_node, should_follow_arc, my_on_new_arc);
		}

		bool is_inside(int x) const {
			return node_set.is_inside(x);
		}

		int node_count_inside() const {
			return node_set.node_count_inside();
		}

		int max_node_count_inside() const {
			return node_set.max_node_count_inside();
		}

		template<class Graph>
		void shrink_cut_front(const Graph&graph){
			front.erase(
				std::remove_if(
					front.begin(), front.end(),
					[&](int xy){ return node_set.is_inside(graph.head(xy)); }
				),
				front.end()
			);
		}

		const std::vector<int>&get_cut_front() const {
			return front;
		}

	private:
		BasicNodeSet node_set;
		std::vector<int>front;
	};

	class ReachableNodeSet{
	public:
		template<class Graph>
		explicit ReachableNodeSet(const Graph&graph):
			node_set(graph), predecessor(graph.node_count()){}

		void reset(const AssimilatedNodeSet&other, bool keep_extra){
			if (keep_extra) {
				node_set.copy_nodes_without_extra_nodes(other.node_set);
			} else {
				node_set = other.node_set;
			}
		}

		void clear(){
			node_set.clear();
		}

		template<class Graph>
		void add_node(const Graph& graph, int x) {
			node_set.add_node(graph, x);
		}

		template<class Graph>
		void add_extra_node(const Graph &graph, int x){
			node_set.add_extra_node(graph, x);
		}

		const std::vector<int>& get_extra_nodes() const {
			return node_set.get_extra_nodes();
		}

		void clear_extra_nodes() {
			node_set.clear_extra_nodes();
		}

		bool can_grow()const{
			return node_set.can_grow();
		}

		template<class Graph, class SearchAlgorithm, class OnNewNode, class ShouldFollowArc, class OnNewArc>
		void grow(
			const Graph&graph,
			TemporaryData&tmp,
			const SearchAlgorithm&search_algo,
			const OnNewNode&on_new_node, // on_new_node(x) is called for every node x. If it returns false then the search is stopped, if it returns true it continues
			const ShouldFollowArc&should_follow_arc, // is called for a subset of arcs and must say whether the arc sould be followed
			const OnNewArc&on_new_arc // on_new_arc(xy) is called for ever arc xy with x in the set
		){
			auto my_should_follow_arc = [&](int xy){
				predecessor[graph.head(xy)] = xy;
				return should_follow_arc(xy);
			};

			node_set.grow(graph, tmp, search_algo, on_new_node, my_should_follow_arc, on_new_arc);
		}

		bool is_inside(int x) const {
			return node_set.is_inside(x);
		}

		int node_count_inside() const {
			return node_set.node_count_inside();
		}

		int max_node_count_inside() const {
			return node_set.max_node_count_inside();
		}

		template<class Graph, class IsSource, class OnNewArc>
		void forall_arcs_in_path_to(const Graph&graph, const IsSource&is_source, int target, const OnNewArc&on_new_arc){
			int x = target;
			while(!is_source(x)){
				on_new_arc(predecessor[x]);
				x = graph.tail(predecessor[x]);
			}
		}

	private:
		BasicNodeSet node_set;
		ArrayIDFunc<int>predecessor;
	};

	struct SourceTargetPair{
		int source, target;
	};

	struct CutterStateDump{
		BitIDFunc source_assimilated, target_assimilated, source_reachable, target_reachable, flow;
	};

	class BasicCutter{
	public:
		template<class Graph>
		explicit BasicCutter(const Graph&graph, const flow_cutter::Config& config):
			assimilated{AssimilatedNodeSet(graph), AssimilatedNodeSet(graph)},
			reachable{ReachableNodeSet(graph), ReachableNodeSet(graph)},
			flow(graph.arc_count()),
			flow_intensity(0),
			//dinic(graph),
			can_advance(false),
			has_cut(false),
			initial_flow(true),
			side(0),
			node_order(),
			order_pointer({0, (graph.node_count() - 1)}),
			config(config)
		{ }

		template<class Graph, class SearchAlgorithm>
		void init(const Graph&graph, TemporaryData& tmp, const SearchAlgorithm&, std::vector<int> order, int random_seed, SourceTargetPair st){
			(void) tmp;
			assimilated[source_side].clear();
			reachable[source_side].clear();
			assimilated[target_side].clear();
			reachable[target_side].clear();
			node_order = std::move(order);
			rng.seed(random_seed);
			order_pointer[source_side] = 0;
			order_pointer[target_side] = graph.node_count() - 1;
			flow.clear();
			flow_intensity = 0;
			has_cut = false;
			initial_flow = true;
			side = 0;

			perform_bulk_piercing = st.source == -1 && st.target == -1 && !node_order.empty();
			if (perform_bulk_piercing) {
				if (!bulk_piercing(graph, source_side, config.initial_assimilated_fraction, true)) {
					assimilated[source_side].add_node(graph, node_order.front());
					assimilated[source_side].add_extra_node(graph, node_order.front());
					reachable[source_side].add_node(graph, node_order.front());
					reachable[source_side].add_extra_node(graph, node_order.front());
				}

				if (!bulk_piercing(graph, target_side, config.initial_assimilated_fraction, true)) {
					assimilated[target_side].add_node(graph, node_order.back());
					assimilated[target_side].add_extra_node(graph, node_order.back());
					reachable[target_side].add_node(graph, node_order.back());
					reachable[target_side].add_extra_node(graph, node_order.back());
				}
			}
			else {
				//revert to standard FlowCutter
				if (st.source == -1 || st.target == -1) { throw std::runtime_error("Standard FlowCutter with invalid source/target pair."); }
				assimilated[source_side].add_node(graph, st.source);
				assimilated[source_side].add_extra_node(graph, st.source);
				reachable[source_side].add_node(graph, st.source);
				reachable[source_side].add_extra_node(graph, st.source);

				assimilated[target_side].add_node(graph, st.target);
				assimilated[target_side].add_extra_node(graph, st.target);
				reachable[target_side].add_node(graph, st.target);
				reachable[target_side].add_extra_node(graph, st.target);
			}

			can_advance = true;
			check_invariants(graph);
		}

		CutterStateDump dump_state()const{
			return {
				id_func(
					assimilated[source_side].max_node_count_inside(),
					[&](int x){
						return assimilated[source_side].is_inside(x);
					}
				),
				id_func(
					assimilated[target_side].max_node_count_inside(),
					[&](int x){
						return assimilated[target_side].is_inside(x);
					}
				),
				id_func(
					assimilated[source_side].max_node_count_inside(),
					[&](int x){
						return reachable[source_side].is_inside(x);
					}
				),
				id_func(
					assimilated[target_side].max_node_count_inside(),
					[&](int x){
						return reachable[target_side].is_inside(x);
					}
				),
				id_func(
					flow.preimage_count(),
					[&](int xy){
						return flow(xy) != 0;
					}
				)
			};
		}

		//! Returns true if a new cut was found. Returns false if no cut was found. False implies that no cut
		//! will be found in the future. Repeatly calling this function after it returned false does not do
		//! anything.
		template<class Graph, class SearchAlgorithm, class ScorePierceNode>
		bool advance(const Graph&graph, TemporaryData&tmp, const SearchAlgorithm&search_algo, const ScorePierceNode&score_pierce_node){
			assert(can_advance);
			bool has_pierced = false;

			if (has_cut) {
				check_invariants(graph);
				side = get_current_cut_side();
				if(assimilated[side].node_count_inside() >= graph.node_count()/2){
					can_advance = false;
					return false;
				}
				reachable[side].clear_extra_nodes();
				assimilated[side].clear_extra_nodes();

				has_pierced = true;

				if (!does_next_advance_increase_flow(graph, score_pierce_node) || !bulk_piercing(graph, side, config.bulk_step_fraction, false)) {
					int pierce_node = select_pierce_node(graph, side, score_pierce_node);
					if(pierce_node == -1){
						can_advance = false;
						return false;
					}

					assert(!assimilated[1-side].is_inside(pierce_node));

					assimilated[side].add_node(graph, pierce_node);
					assimilated[side].add_extra_node(graph, pierce_node);
					reachable[side].add_node(graph, pierce_node);
					reachable[side].add_extra_node(graph, pierce_node);
				}

				has_cut = false;
			}

			advance_flow(graph, tmp, search_algo, side, has_pierced);

			if (has_cut) {
				grow_assimilated_sets(graph, tmp, search_algo);
			}

			check_invariants(graph);
			can_advance = true;
			return true;
		}

		bool is_not_finished()const{
			return can_advance;
		}

		template<class Graph, class ScorePierceNode>
		bool does_next_advance_increase_flow(const Graph &graph, const ScorePierceNode &score_pierce_node){
			assert(cut_available());
			int side = get_current_cut_side();
			if(assimilated[side].node_count_inside() >= graph.node_count()/2)
				return true;
			check_invariants(graph);
			int pierce_node = select_pierce_node(graph, side, score_pierce_node);
			if(pierce_node == -1)
				return true;
			else if(reachable[1-side].is_inside(pierce_node))
				return true;
			else
				return false;
		}

		bool is_on_smaller_side(int x)const{
			return assimilated[get_current_cut_side()].is_inside(x);
		}

		static const int source_side = 0;
		static const int target_side = 1;

		int get_current_cut_side()const{
			if(
				reachable[source_side].node_count_inside() == assimilated[source_side].node_count_inside() && (
					reachable[target_side].node_count_inside() != assimilated[target_side].node_count_inside() ||
					assimilated[source_side].node_count_inside() <= assimilated[target_side].node_count_inside()
				)
			)
				return source_side;
			else
				return target_side;
		}

		int get_current_smaller_cut_side_size()const{
			return assimilated[get_current_cut_side()].node_count_inside();
		}

		const std::vector<int>&get_current_cut()const{
			return assimilated[get_current_cut_side()].get_cut_front();
		}

		int get_current_flow_intensity() const {
			std::atomic_thread_fence(std::memory_order_acquire);
			return flow_intensity;
		}

		int get_assimilated_node_count()const{
			return assimilated[source_side].node_count_inside() + assimilated[target_side].node_count_inside();
		}

		bool cut_available() const {
			return has_cut;
		}

		bool is_initial_flow() const {
			return initial_flow;
		}

	private:

		bool perform_bulk_piercing = true;

		static constexpr bool perform_equidistant_bulk_piercing = false;

		int side_bulk_pointer(const int side) const { return order_pointer[side]; }
		int remaining_bulk_nodes(const int side) const {
			return static_cast<int>(side == source_side ? config.bulk_assimilation_order_threshold * node_order.size() - order_pointer[side]
														: order_pointer[side] - (1.0 - config.bulk_assimilation_order_threshold) * node_order.size());
		}


		template<class Graph>
		bool bulk_piercing(const Graph& graph, int side, double max_bulk_step_fraction, bool overrule_adaptive) {
			if (perform_equidistant_bulk_piercing || overrule_adaptive) return equidistant_bulk_piercing(graph, side, max_bulk_step_fraction);
			else return adaptive_bulk_piercing(graph, side, max_bulk_step_fraction);
		}

		bool node_order_exceeded(int side) {
			return remaining_bulk_nodes(side) <= 0;
		}

		/**
		 * Gets the next node from the node order.
		 *
		 * The order_pointer is on the returned node
		 * afterwards. Multiple calls will return the same
		 * node unless the node has been assimilated in the
		 * meantime.
		 *
		 * @param side The side to take the node from.
		 * @param force Override the bulk_assimilation_order_threshold
		 * @return The selected node.
		 */
		int get_next_node_from_order(int side, bool force = false) {
			while (
			       order_pointer[source_side] <= order_pointer[target_side] &&
			       (force || !node_order_exceeded(side))
			       ) {
				assert(order_pointer[side] < (int)node_order.size());
				assert(order_pointer[side] >= 0);

				int x = node_order[order_pointer[side]];

				if (!assimilated[source_side].is_inside(x) && !assimilated[target_side].is_inside(x)) {
					return x;
				}

				if (side == source_side) {
					++order_pointer[source_side];
				} else {
					--order_pointer[target_side];
				}
			}

                        return -1;
                }

		template<class Graph>
		bool adaptive_bulk_piercing(const Graph& graph, int side, double chunk_size) {
			if (!perform_bulk_piercing) return false;
			if (node_order_exceeded(side)) return false;
			if ( (double)assimilated[side].node_count_inside() > config.bulk_assimilation_threshold * graph.node_count()) return false;
			auto all_neighbors_in_side = [&](int x, int side) {
				for(auto xy : graph.out_arc(x)){
					int y = graph.head(xy);
					if(!assimilated[side].is_inside(y)){
						return false;
					}
				}
				return true;
			};

			//int nodes_to_assimilate = chunk_size * (static_cast<int>((graph.node_count() * (1 - chunk_size)) / 2) - assimilated[side].node_count_inside());
			int nodes_to_assimilate = chunk_size * ( (1-chunk_size)*graph.node_count()/2 - assimilated[side].node_count_inside() );
			nodes_to_assimilate = std::max(nodes_to_assimilate, 1);

			std::vector<int> newly_assimilated;
			while ((int)newly_assimilated.size() < nodes_to_assimilate) {
				int node = get_next_node_from_order(side);
				if (node == -1) break;
				assimilated[side].add_node(graph, node);
				reachable[side].add_node(graph, node);
				newly_assimilated.push_back(node);
			}
			for (int node : newly_assimilated) {
				if (!all_neighbors_in_side(node, side)) {
					assimilated[side].add_extra_node(graph, node);
					reachable[side].add_extra_node(graph, node);
				}
			}
			return !newly_assimilated.empty();

		}

		template<class Graph>
		bool equidistant_bulk_piercing(const Graph& graph, int side, double max_bulk_step_fraction) {
			// FIXME: This does not fully work anymore apart from the initialization.
			// TODO: Rewrite to just support the initialization use case.
			if (!perform_bulk_piercing) return false;
			if ( (double)assimilated[side].node_count_inside() > config.bulk_assimilation_threshold * graph.node_count()) return false;
			auto nodes_to_assimilate = static_cast<int>(graph.node_count() * max_bulk_step_fraction);
			if (nodes_to_assimilate <= 1) return false;
			std::vector<int> newly_assimilated;
			int iterations = 0;
			while (newly_assimilated.empty() && remaining_bulk_nodes(side) > 1) {
				iterations++;
				auto try_assimilate_node = [&](int node) {
					if (!assimilated[source_side].is_inside(node) && !assimilated[target_side].is_inside(node)) {
						newly_assimilated.push_back(node);
						assimilated[side].add_node(graph, node);
						reachable[side].add_node(graph, node);
					}
				};
				nodes_to_assimilate = std::min(nodes_to_assimilate, remaining_bulk_nodes(side));
				if (side == source_side) {
					int _end = order_pointer[side] + nodes_to_assimilate;
					for ( ; order_pointer[side] < _end; order_pointer[side]++)
						try_assimilate_node(node_order[order_pointer[side]]);
				}
				else {
					int _end = order_pointer[side] - nodes_to_assimilate;
					for ( ; order_pointer[side] > _end; order_pointer[side]--)
						try_assimilate_node(node_order[order_pointer[side]]);
				}

			}

			auto all_neighbors_in_side = [&](int x, int side) {
				for(auto xy : graph.out_arc(x)){
					int y = graph.head(xy);
					if(!assimilated[side].is_inside(y)){
						return false;
					}
				}
				return true;
			};

			for (int node : newly_assimilated) {
				if (!all_neighbors_in_side(node, side)) {
					assimilated[side].add_extra_node(graph, node);
					reachable[side].add_extra_node(graph, node);
				}
			}
			return !newly_assimilated.empty();
		}

		template<class Graph, class ScorePierceNode>
		int select_pierce_node(const Graph&graph, int side, const ScorePierceNode&score_pierce_node){
			int pierce_node = -1;
			std::tuple<bool, int, int> max_score = {false, std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
			for(auto xy : assimilated[side].get_cut_front()){
				int y = graph.head(xy);
				if(!assimilated[1-side].is_inside(y)){
					auto score = score_pierce_node(y, side, graph.node_count(), reachable[1-side].is_inside(y), graph.arc_weight(xy));
					if(score > max_score){
						max_score = score;
						pierce_node = y;
					}
				}
			}

			if (pierce_node == -1) {
				pierce_node = get_next_node_from_order(side, true);
			}

			assert(pierce_node != -1 || (assimilated[source_side].node_count_inside() + assimilated[target_side].node_count_inside() == graph.node_count()));

			return pierce_node;
		}

		template<class Graph>
		bool is_saturated(const Graph&graph, int direction, int xy){
			if(direction == target_side)
				xy = graph.back_arc(xy);
			return graph.capacity(xy) == flow(xy);
		}


		template<class Graph, class SearchAlgorithm>
		void advance_flow(const Graph &graph, TemporaryData &tmp, const SearchAlgorithm &search_algo, int pierced_side, bool has_pierced) {

			int my_source_side = pierced_side;
			int my_target_side = 1 - pierced_side;

			assert(reachable[pierced_side].can_grow());

			auto is_forward_saturated = [&, this](int xy) {
				return this->is_saturated(graph, my_source_side, xy);
			};

			auto is_source = [&](int x) {
				return assimilated[my_source_side].is_inside(x);
			};

			auto is_target = [&](int x) {
				return assimilated[my_target_side].is_inside(x);
			};

			auto increase_flow = [&](int xy) {
				if (pierced_side == source_side)
					flow.increase(graph, xy);
				else
					flow.decrease(graph, xy);
			};

			bool was_flow_augmented = false;
			/*
			if (false && initial_flow) {
				int augmented_intensity = dinic.advance(graph,
									assimilated[my_source_side].get_extra_nodes(),
									assimilated[my_source_side],
									assimilated[my_target_side],
									reachable[my_source_side],
									is_forward_saturated,
									increase_flow);
				was_flow_augmented = augmented_intensity != 0;
				assert(was_flow_augmented != dinic.is_finished());
				flow_intensity += augmented_intensity;
				if (!was_flow_augmented)
					initial_flow = false;
				check_flow_conservation(graph);
			} else
			*/
			{

				int target_hit = -1;
				auto on_new_node = [&](int x) {
					if (is_target(x)) {
						target_hit = x;
						return false;
					} else
						return true;
				};
				auto should_follow_arc = [&](int xy) { return !is_forward_saturated(xy); };
				auto on_new_arc = [](int) {};
				reachable[my_source_side].grow(graph, tmp, search_algo, on_new_node, should_follow_arc, on_new_arc);

				if (target_hit != -1) {
					check_flow_conservation(graph);
					reachable[my_source_side].forall_arcs_in_path_to(graph, is_source, target_hit, increase_flow);
					check_flow_conservation(graph);
					reachable[my_source_side].reset(assimilated[my_source_side], true);
					++flow_intensity;

					was_flow_augmented = true;
					check_flow_conservation(graph);
				}
			}
			if (!was_flow_augmented) {
				has_cut = true;
				if (!has_pierced) {
					// If we have pierced and the flow has not been augmented, there is no need to reset the backward-reachable nodes
					grow_backward_reachable(graph, tmp, search_algo, my_target_side);
				}
			}
		}


		template<class Graph, class SearchAlgorithm>
		void grow_backward_reachable(const Graph&graph, TemporaryData&tmp, const SearchAlgorithm&search_algo, int my_target_side) {
			auto is_backward_saturated = [&, this](int xy) {
				return this->is_saturated(graph, my_target_side, xy);
			};
			reachable[my_target_side].reset(assimilated[my_target_side], false);
			auto should_follow_arc = [&](int xy) { return !is_backward_saturated(xy); };
			auto on_new_node = [&](int) { return true; };
			auto on_new_arc = [](int) {};
			reachable[my_target_side].grow(graph, tmp, search_algo, on_new_node, should_follow_arc, on_new_arc);
		}

		template<class Graph, class SearchAlgorithm>
		void grow_assimilated_sets(const Graph&graph, TemporaryData&tmp, const SearchAlgorithm&search_algo){
			auto is_forward_saturated = [&,this](int xy){
				return this->is_saturated(graph, source_side, xy);
			};

			auto is_backward_saturated = [&,this](int xy){
				return this->is_saturated(graph, target_side, xy);
			};

			if(reachable[source_side].node_count_inside() <= reachable[target_side].node_count_inside()){
				auto on_new_node = [&](int){return true;};
				auto should_follow_arc = [&](int xy){ return !is_forward_saturated(xy); };
				auto on_new_arc = [](int){};
				auto has_flow = [&](int xy){ return flow(xy) != 0; };
				assimilated[source_side].grow(graph, tmp, search_algo, on_new_node, should_follow_arc, on_new_arc, has_flow);
				assimilated[source_side].shrink_cut_front(graph);
			}else{
				auto on_new_node = [&](int){return true;};
				auto should_follow_arc = [&](int xy){ return !is_backward_saturated(xy); };
				auto on_new_arc = [](int){};
				auto has_flow = [&](int xy){ return flow(xy) != 0; };
				assimilated[target_side].grow(graph, tmp, search_algo, on_new_node, should_follow_arc, on_new_arc, has_flow);
				assimilated[target_side].shrink_cut_front(graph);
			}
		}

		template<class Graph>
		void check_flow_conservation(const Graph&graph){
			(void)graph;
			#ifndef NDEBUG
			for(int x=0; x<graph.node_count(); ++x)
				if(!assimilated[source_side].is_inside(x) && !assimilated[target_side].is_inside(x)){
					int flow_surplus = 0;
					for(auto xy : graph.out_arc(x))
						flow_surplus += flow(xy);
					assert(flow_surplus == 0 && "Flow must be conserved outside of the assimilated sides");
				}
			#endif
		}

		template<class Graph>
		void check_invariants(const Graph&graph){
			(void)graph;
			#ifndef NDEBUG
			for(int side = 0; side < 2; ++side)
				assert(assimilated[side].node_count_inside() > 0 && "Each side must contain at least one node");

			if (has_cut) {
				for(int x=0; x<graph.node_count(); ++x)
					assert((!assimilated[source_side].is_inside(x) || !assimilated[target_side].is_inside(x)) && "a node can not be assimilated by both sides");
			}

			for(int side = 0; side < 2; ++side)
				for(int x=0; x<graph.node_count(); ++x)
					if(assimilated[side].is_inside(x))
						assert(reachable[side].is_inside(x) && "assimilated must be a subset of reachable");

			check_flow_conservation(graph);

			if (has_cut) {
				int smaller_reachable_side;
				if(reachable[source_side].node_count_inside() <= reachable[target_side].node_count_inside())
					smaller_reachable_side = source_side;
				else
					smaller_reachable_side = target_side;
				assert(reachable[smaller_reachable_side].node_count_inside() == assimilated[smaller_reachable_side].node_count_inside());
				for(int x=0; x<graph.node_count(); ++x)
					assert(reachable[smaller_reachable_side].is_inside(x) == assimilated[smaller_reachable_side].is_inside(x));

				assert(flow_intensity == (int)get_current_cut().size());

				TemporaryData tmp(graph.node_count());

				for (int side = 0; side < 2; ++side) {
				        ReachableNodeSet reachable_check(graph);
					reachable_check.reset(assimilated[side], false);
					reachable_check.clear_extra_nodes();
					for (int x = 0; x < graph.node_count(); ++x) {
						if (assimilated[side].is_inside(x)) {
							reachable_check.add_extra_node(graph, x);
						}
					}

					auto should_follow_arc = [&](int xy) { return !is_saturated(graph, side, xy); };
					auto on_new_node = [&](int) { return true; };
					auto on_new_arc = [](int) {};
					reachable_check.grow(graph, tmp, PseudoDepthFirstSearch(), on_new_node, should_follow_arc, on_new_arc);

					assert(reachable[side].node_count_inside() == reachable_check.node_count_inside());

					for (int x = 0; x < graph.node_count(); ++x) {
						assert(reachable[side].is_inside(x) == reachable_check.is_inside(x));
					}
				}
			}
			#endif
		}

		AssimilatedNodeSet assimilated[2];
		ReachableNodeSet reachable[2];
		UnitFlow flow;
		int flow_intensity;
		//flow_cutter_accelerated::UnitDinicAlgo dinic;
		bool can_advance;
		bool has_cut;
		bool initial_flow;
		int side;
		std::vector<int> node_order;
		std::array<int, 2> order_pointer;
		const flow_cutter::Config& config;
		std::mt19937 rng;
	};



	enum class DistanceType{
		no_distance,
		hop_distance,
		hop_distance_terminal_set,
		weighted_distance,
		order_distance
	};

	class DistanceAwareCutter{
	private:
		template<class Graph>
		static void compute_hop_distance_from(const Graph&graph, TemporaryData&tmp, int source, ArrayIDFunc<int>&dist){
			dist.fill(std::numeric_limits<int>::max());
			dist[source] = 0;

			auto was_node_seen = [&](int){return false;};
			auto see_node = [](int){ return true; };
			auto should_follow_arc = [&](int xy){
				if(dist(graph.tail(xy)) < dist(graph.head(xy)) - 1){
					dist[graph.head(xy)] = dist(graph.tail(xy))+1;
					return true;
				}else{
					return false;
				}
			};
			auto on_new_arc = [&](int){};
			BreadthFirstSearch()(graph, tmp, source, was_node_seen, see_node, should_follow_arc, on_new_arc);
		}

		template<class Graph, class ForwardIt>
		static void compute_hop_distance_from(const Graph&graph, TemporaryData&tmp, ForwardIt sources_begin, ForwardIt sources_end, ArrayIDFunc<int>&dist){
			dist.fill(std::numeric_limits<int>::max());
			for (; sources_begin != sources_end; ++sources_begin) {
			   dist[*sources_begin] = 0;
			}
			auto was_node_seen = [&](int x){return dist[x] != std::numeric_limits<int>::max();};
			auto see_node = [](int){ return true; };
			auto should_follow_arc = [&](int xy){
				if(dist(graph.tail(xy)) < dist(graph.head(xy)) - 1){
					dist[graph.head(xy)] = dist(graph.tail(xy))+1;
					return true;
				}else{
					return false;
				}
			};
			auto on_new_arc = [&](int){};
			BreadthFirstSearch()(graph, tmp, sources_begin, sources_end, was_node_seen, see_node, should_follow_arc, on_new_arc);
		}

		template<class Graph>
		static void compute_weighted_distance_from(const Graph&graph, TemporaryData&, int source, ArrayIDFunc<int>&dist){
			Dijkstra<BitIDFunc>dij(graph.node_count());
			dij.clear();
			dij.add_source_node(source);
			while(!dij.is_finished())
				dij.settle_next(graph.out_arc, graph.head, graph.arc_weight, [](int,bool,int){});
			dist = dij.move_distance_array();
		}

		inline static double square(const double a) { return a * a; }
		inline static double squared_distance(const GeoPos &a, const GeoPos &b) { return square(a.lat - b.lat) + square(a.lon - b.lon); }

	public:
		template<class Graph>
		DistanceAwareCutter(const Graph&graph, const flow_cutter::Config& config):
			cutter(graph, config),
			node_dist{ArrayIDFunc<int>{graph.node_count()}, ArrayIDFunc<int>{graph.node_count()}},
			config(config)
			{ }

		struct TerminalInformation {
			std::vector<int> node_order;
			bool build_node_order_from_distance;
			SourceTargetPair st;
			int cutter_id;
		};

		template<class Graph, class SearchAlgorithm, class GeoPos>
        void init(const Graph &graph, TemporaryData &tmp, const SearchAlgorithm &search_algo, DistanceType dist_type,
                  TerminalInformation ti, double bulk_distance_factor, int random_seed, const GeoPos& geo_pos) {
			if (ti.build_node_order_from_distance && (ti.st.source == -1 || ti.st.target == -1)) {
				throw std::runtime_error("DistanceAwareCutter::init() No source or no target specified (==-1) but node order from distance requested.");
			}
			std::vector<int> node_order = std::move(ti.node_order);
			int source = ti.st.source;
			if (source == -1) {
				if (node_order.empty()) { throw std::runtime_error("DistanceAwareCutter::init() No source specified (==-1) ==> take first from node_order but node_order is empty."); }
				source = node_order.front();
			}
			int target = ti.st.target;
			if (target == -1) {
				if (node_order.size() < 2) { throw std::runtime_error("DistanceAwareCutter::init() No target specified (==-1) ==> take last from node_order but node_order has less than two elements."); }
				target = node_order.back();
			}

			int terminal_set_size = std::max(static_cast<int>(node_order.size() * bulk_distance_factor), 1);

			switch(dist_type){
			case DistanceType::hop_distance:
				compute_hop_distance_from(graph, tmp, source, node_dist[source_side]);
				compute_hop_distance_from(graph, tmp, target, node_dist[target_side]);
				break;
			case DistanceType::hop_distance_terminal_set:
				if ((int)node_order.size() < 2 * terminal_set_size) {
					throw std::runtime_error("DistanceAwareCutter::init() Hop distance from front and back of node order requested, but node order is too small.");
				}
				compute_hop_distance_from(graph, tmp, node_order.begin(), node_order.begin() + terminal_set_size, node_dist[source_side]);
				compute_hop_distance_from(graph, tmp, node_order.end() - terminal_set_size, node_order.end(), node_dist[target_side]);
				break;
			case DistanceType::weighted_distance:
				compute_weighted_distance_from(graph, tmp, source, node_dist[source_side]);
				compute_weighted_distance_from(graph, tmp, target, node_dist[target_side]);
				break;
			case DistanceType::no_distance:
				break;
			default:
				assert(false);
				break;
			}
			if (ti.build_node_order_from_distance) {
				node_order.resize(graph.node_count());
				std::iota(node_order.begin(), node_order.end(), 0);
				//using ScoreT = std::tuple<int, double, int, double>;
				//using ScoreT = std::tuple<int, int>;
				using ScoreT = std::tuple<int>;
				(void) geo_pos;
				auto score = [&](int x) -> ScoreT {
					const int tar_dist = node_dist[target_side][x]; const int src_dist = node_dist[source_side][x];
					//const double tar_geo = squared_distance(geo_pos(target), geo_pos(x)); const double src_geo = squared_distance(geo_pos(source), geo_pos(x));
					//return std::make_tuple(tar_geo - src_geo, square(tar_dist) - square(src_dist));//, -1 * std::min(tar_dist, src_dist), -1.0 * std::min(tar_geo, src_geo) );
					//return std::make_tuple( square(tar_dist) - square(src_dist), tar_geo - src_geo);//, -1 * std::min(tar_dist, src_dist), -1.0 * std::min(tar_geo, src_geo) );
					return std::make_tuple( tar_dist - src_dist);//, /*tar_geo - src_geo, */-1 * std::min(tar_dist, src_dist));//-1.0 * std::min(tar_geo, src_geo) );
				};
				auto comp = [&](int l, int r) { return score(l) > score(r); };
				auto time = - get_micro_time();
				int max_bulk = std::max(static_cast<int>(config.bulk_assimilation_order_threshold * graph.node_count()), 1);
				std::nth_element(node_order.begin(), node_order.begin() + max_bulk, node_order.end(), comp);
				std::sort(node_order.begin(), node_order.begin() + max_bulk + 1, comp);
				std::nth_element(node_order.begin() + max_bulk + 1, node_order.end() - (max_bulk + 1), node_order.end(), comp);
				std::sort(node_order.end() - (max_bulk + 1), node_order.end(), comp);
				time += get_micro_time();
				std::cout << "sort took " << time/1000 << " ms" << std::endl;
				{
#ifndef NDEBUG
					std::sort(node_order.begin(), node_order.end(), comp);
					std::vector<ScoreT> scores, unique_scores;
					for (int u : node_order) scores.push_back(score(u));
					std::unique_copy(scores.begin(), scores.end(), std::back_inserter(unique_scores));
					std::cout << "total scores " << scores.size() << " unique scores " << unique_scores.size() << std::endl;
#endif
				}
			}
			cutter.init(graph, tmp, search_algo, std::move(node_order), random_seed, ti.st);
		}

		CutterStateDump dump_state()const{
			return cutter.dump_state();
		}

		template<class Graph, class SearchAlgorithm, class ScorePierceNode>
		bool advance(const Graph&graph, TemporaryData&tmp, const SearchAlgorithm&search_algo, const ScorePierceNode&score_pierce_node){
			auto my_score_pierce_node = [&](int x, int side, int node_count, bool causes_augmenting_path, int arc_weight){
				return score_pierce_node(x, side, node_count, causes_augmenting_path, arc_weight, node_dist[side](x), node_dist[1-side](x));
			};
			return cutter.advance(graph, tmp, search_algo, my_score_pierce_node);
		}

		bool is_not_finished()const{
			return cutter.is_not_finished();
		}

		template<class Graph, class ScorePierceNode>
		bool does_next_advance_increase_flow(const Graph &graph, const ScorePierceNode &score_pierce_node){
			auto my_score_pierce_node = [&](int x, int side, int node_count, bool causes_augmenting_path, int arc_weight){
				return score_pierce_node(x, side, node_count, causes_augmenting_path, arc_weight, node_dist[side](x), node_dist[1-side](x));
			};
			return cutter.does_next_advance_increase_flow(graph, my_score_pierce_node);
		}

		static const int source_side = BasicCutter::source_side;
		static const int target_side = BasicCutter::target_side;

		int get_current_cut_side()const{
			return cutter.get_current_cut_side();
		}

		int get_current_smaller_cut_side_size()const{
			return cutter.get_current_smaller_cut_side_size();
		}

		int get_current_flow_intensity() const {
			return cutter.get_current_flow_intensity();
		}

		const std::vector<int>&get_current_cut()const{
			return cutter.get_current_cut();
		}

		int get_assimilated_node_count()const{
			return cutter.get_assimilated_node_count();
		}

		bool is_on_smaller_side(int x)const{
			return cutter.is_on_smaller_side(x);
		}

		bool is_empty()const{
			return node_dist[0].preimage_count() == 0;
		}

		bool cut_available() const {
			return cutter.cut_available();
		}

		bool is_initial_flow() const {
			return cutter.is_initial_flow();
		}

		int node_count() const {
			return node_dist[0].preimage_count();
		}

	private:
		BasicCutter cutter;
		ArrayIDFunc<int>node_dist[2];
		const flow_cutter::Config& config;
	};

	class MultiCutter{
	public:

		MultiCutter(const flow_cutter::Config& config, const int node_count) :  tmp(TemporaryData(node_count)), config(config) { }

		using TerminalInformation = std::vector<DistanceAwareCutter::TerminalInformation>;

		template<class Graph, class SearchAlgorithm, class GeoPos>
		void init(
			const Graph&graph,
			const SearchAlgorithm&search_algo,
			DistanceType dist_type,
			TerminalInformation terminal_info,
			double bulk_distance_factor,
			int random_seed,
			const GeoPos& geo_pos
		)
		{
			while(cutter_list.size() > terminal_info.size())
				cutter_list.pop_back(); // can not use resize because that requires default constructor...
			while(cutter_list.size() < terminal_info.size())
				cutter_list.emplace_back(graph, config);

			if (graph.node_count() > ParallelismCutoff)
				tbb::parallel_for(size_t(0), cutter_list.size(), [&](size_t i) { cutter_list[i].init(graph, tmp.local(), search_algo, dist_type, std::move(terminal_info[i]), bulk_distance_factor, random_seed+1+i, geo_pos); } );
			else
				for (size_t i = 0; i < cutter_list.size(); ++i) { cutter_list[i].init(graph, tmp.local(), search_algo, dist_type, std::move(terminal_info[i]), bulk_distance_factor, random_seed+1+i, geo_pos); }
		}

		CutterStateDump dump_state()const{
			if(cutter_list.size() != 1)
				throw std::runtime_error("Can only dump the cutter state if a single instance is run");
			return cutter_list[0].dump_state();
		}

		template<class Graph, class SearchAlgorithm, class ScorePierceNode, class ShallContinue, class ReportCut>
		bool parallel_enum_cuts(const Graph&graph, const SearchAlgorithm&search_algo, const ScorePierceNode&score_pierce_node, const ShallContinue &shall_continue, const ReportCut &report_cut) {
			// Work around atomic_flag not being copyable
			std::vector<std::atomic_flag> cutter_running(cutter_list.size());
			for (auto &r : cutter_running) {
				r.clear();
			}

			// Work around std::atomic not being copyable
			std::vector<std::atomic<bool>> cutter_active(cutter_list.size());
			for (auto &a : cutter_active) {
				a = true;
			}


			tbb::enumerable_thread_specific<std::vector<std::pair<int, int>>> tmp_cutter_flow;

			std::atomic<int> acquired_cutters(0);


			auto get_cutter_to_work_on = [&]() -> int {
				auto &my_cutter_flow = tmp_cutter_flow.local();
				my_cutter_flow.clear();
				my_cutter_flow.reserve(cutter_list.size());

				for (int i = 0; i < static_cast<int>(cutter_list.size()); ++i) {
					if (cutter_active[i].load(std::memory_order_consume)) {
					    my_cutter_flow.emplace_back(i, cutter_list[i].get_current_flow_intensity());
					}
				}

				std::sort(my_cutter_flow.begin(), my_cutter_flow.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

				for (auto & cutter_flow : my_cutter_flow) {
					int cutter_id = cutter_flow.first;
					if (!cutter_running[cutter_id].test_and_set(std::memory_order_acquire)) {
						acquired_cutters++;
						return cutter_id;
					}
				}

				return -1;
			};




			tbb::parallel_for(static_cast<size_t>(0), cutter_list.size(), [&](const size_t task_id) {
				int cutter_id = task_id;
				std::vector<int> task_local_flow_augs(cutter_list.size(), 0);
				std::vector<int> task_local_cuts(cutter_list.size(), 0);

				if (cutter_running[cutter_id].test_and_set(std::memory_order_acquire)) {
					cutter_id = get_cutter_to_work_on();
				}
				else {
					acquired_cutters++;
				}

				while (cutter_id != -1) {
					auto& c = cutter_list[cutter_id];
					auto my_score_pierce_node = [&](int x, int side, int node_count,
													bool causes_augmenting_path, int arc_weight,
													int source_dist, int target_dist) {
						return score_pierce_node(x, side, node_count, causes_augmenting_path, arc_weight,
												    source_dist, target_dist, cutter_id);
					};

					//reassess memory fence and assess overhead for distributing cutters.
					if (cutter_active[cutter_id].load(std::memory_order_consume)) {
						std::atomic_thread_fence(std::memory_order_consume);

						if (c.is_not_finished() && shall_continue(c)) {


							task_local_flow_augs[cutter_id]++;
							if (!c.advance(graph, tmp.local(), search_algo, my_score_pierce_node)) {
								cutter_active[cutter_id].store(false, std::memory_order_release);
							}
							else if (c.cut_available()) {
								task_local_flow_augs[cutter_id]--;
								task_local_cuts[cutter_id]++;
								while (!c.does_next_advance_increase_flow(graph, my_score_pierce_node)) {
									c.advance(graph, tmp.local(), search_algo, my_score_pierce_node);
								}

								report_cut(c, cutter_id);
							}
						}
						else {
							cutter_active[cutter_id].store(false, std::memory_order_release);
						}

						std::atomic_thread_fence(std::memory_order_release);
					}

					if (acquired_cutters != (int)cutter_list.size() || !cutter_active[cutter_id]) {
						cutter_running[cutter_id].clear(std::memory_order_release);
						acquired_cutters--;
						cutter_id = get_cutter_to_work_on();
					}
				}
			});

			return false;
		}

		/**
		 * Enumerate all cuts
		 * @param graph The graph to use.
		 * @param search_algo The search algorithm to use.
		 * @param score_pierce_node The pierce node scoring
		 * @param shall_continue Callback that gets the current flow value of a cutter and returns a boolean if the cutter shall continue
		 * @param report_cut Callback that receives the cut.
		 */
		template<class Graph, class SearchAlgorithm, class ScorePierceNode, class ShallContinue, class ReportCut>
		void enum_cuts(const Graph&graph, const SearchAlgorithm&search_algo, const ScorePierceNode&score_pierce_node, const ShallContinue &shall_continue, const ReportCut &report_cut, const bool report_cuts_in_order) {


			if (!report_cuts_in_order && graph.node_count() > ParallelismCutoff && config.thread_count > 1) {
				parallel_enum_cuts(graph, search_algo, score_pierce_node, shall_continue, report_cut);
				return;
			}

			bool all_cutters_with_min_flow_have_cut = true;
			int minimum_flow_intensity = std::numeric_limits<int>::max();

			std::vector<std::atomic<bool>> cutter_active(cutter_list.size());
			for (auto &ca : cutter_active) {
				ca = true;
			}

			auto update_min_flow = [&]() {
				minimum_flow_intensity = std::numeric_limits<int>::max();

				for (size_t cutter_id = 0; cutter_id < cutter_list.size(); ++cutter_id) {
					const auto& cutter = cutter_list[cutter_id];
					if (!cutter.is_not_finished() || !cutter_active[cutter_id]) continue;
					if (cutter.get_current_flow_intensity() < minimum_flow_intensity) {
						all_cutters_with_min_flow_have_cut = cutter.cut_available();
						minimum_flow_intensity = cutter.get_current_flow_intensity();
					} else if (cutter.get_current_flow_intensity() == minimum_flow_intensity) {
						all_cutters_with_min_flow_have_cut &= cutter.cut_available();
					}
				}
			};

			update_min_flow();

			int current_smaller_side_size = 0;

			for(;;) {

				std::vector<size_t> active_cutter_ids;
				for(size_t i=0; i<cutter_list.size(); ++i){
					auto& cutter = cutter_list[i];
					if (cutter_active[i] && cutter.is_not_finished() && cutter.get_current_flow_intensity() == minimum_flow_intensity && (!cutter.cut_available() || all_cutters_with_min_flow_have_cut)) {
						active_cutter_ids.push_back(i);
					}
				}

				auto handle_cutter = [&](const size_t cutter_id) {
					auto &cutter = cutter_list[cutter_id];

					// Deactivate cutter if desired
					if (!shall_continue(cutter)) {
						cutter_active[cutter_id].store(false, std::memory_order_release);
						return;
                    }

					auto my_score_pierce_node = [&](int x, int side, int node_count,
													bool causes_augmenting_path, int arc_weight,
													int source_dist, int target_dist) {
						return score_pierce_node(x, side, node_count, causes_augmenting_path, arc_weight,
												 source_dist, target_dist, cutter_id);
					};
					if (cutter.advance(graph, tmp.local(), search_algo, my_score_pierce_node)) {
						while (cutter.cut_available() &&
							   !cutter.does_next_advance_increase_flow(graph, my_score_pierce_node)) {
							if (!cutter.advance(graph, tmp.local(), search_algo, my_score_pierce_node)) {
								cutter_active[cutter_id] = false;
								break;
							}
						}
					}
				};


				if (graph.node_count() > ParallelismCutoff && config.thread_count > 1)
					tbb::parallel_for_each(active_cutter_ids, handle_cutter);
				else
					std::for_each(active_cutter_ids.begin(), active_cutter_ids.end(), handle_cutter);

				update_min_flow();

				if (minimum_flow_intensity == std::numeric_limits<int>::max()) {
					return;
				}

				if (all_cutters_with_min_flow_have_cut) {
					int best_cutter_weight = 0;
					int best_cutter_id = -1;
					for(int i=0; i<(int)cutter_list.size(); ++i){
						if(cutter_list[i].is_not_finished()){
							if(
							cutter_list[i].get_current_flow_intensity() == minimum_flow_intensity &&
							cutter_list[i].get_current_smaller_cut_side_size() > best_cutter_weight
							){
								assert(cutter_list[i].get_current_flow_intensity() == (int)cutter_list[i].get_current_cut().size());
								best_cutter_id = i;
								best_cutter_weight = cutter_list[i].get_current_smaller_cut_side_size();
							}
						}
					}

					assert(best_cutter_id != -1);

					if(best_cutter_weight <= current_smaller_side_size) {
						continue;
					}

					current_smaller_side_size = best_cutter_weight;

					report_cut(cutter_list[best_cutter_id], best_cutter_id);
				}
			}
		}

		static constexpr int ParallelismCutoff = 5000;
	private:
		std::vector<DistanceAwareCutter>cutter_list;
		tbb::enumerable_thread_specific<TemporaryData> tmp;
		const flow_cutter::Config& config;
	};

	struct PierceNodeScore{
		static constexpr unsigned hash_modulo = ((1u<<31u)-1u);
		unsigned hash_factor, hash_offset;

		PierceNodeScore(flow_cutter::Config config): config(config){
			std::mt19937 gen;
			gen.seed(config.random_seed);
			gen();
			hash_factor = gen() % hash_modulo;
			hash_offset = gen() % hash_modulo;
		}

		flow_cutter::Config config;


		std::tuple<bool, int, int> operator()(int x, int side, int node_count, bool causes_augmenting_path, int arc_weight, int source_dist, int target_dist, int cutter_id)const{
			(void)cutter_id;
			(void)node_count;
			auto random_number = [&]{
				if(side == BasicCutter::source_side)
					return (hash_factor * (unsigned)(x<<1) + hash_offset) % hash_modulo;
				else
					return (hash_factor * ((unsigned)(x<<1)+1) + hash_offset) % hash_modulo;
			};

			bool avoids_augmenting_path = false;
			int score;
			//int secondary_score = random_number();
			int secondary_score = 0;
			switch(config.pierce_rating){
			case flow_cutter::Config::PierceRating::max_target_minus_source_hop_dist:
			case flow_cutter::Config::PierceRating::max_target_minus_source_weight_dist:
				score = target_dist - source_dist;
				break;
			case flow_cutter::Config::PierceRating::max_target_hop_dist:
			case flow_cutter::Config::PierceRating::max_target_weight_dist:
				score = target_dist;
				break;
			case flow_cutter::Config::PierceRating::min_source_hop_dist:
			case flow_cutter::Config::PierceRating::min_source_weight_dist:
				score = -source_dist;
				break;
			case flow_cutter::Config::PierceRating::oldest:
				score = 0;
				break;
			case flow_cutter::Config::PierceRating::random:
				score = random_number();
				break;
			case flow_cutter::Config::PierceRating::max_arc_weight:
				score = arc_weight;
				break;
			case flow_cutter::Config::PierceRating::min_arc_weight:
				score = -arc_weight;
				break;

			case flow_cutter::Config::PierceRating::circular_hop:
			case flow_cutter::Config::PierceRating::circular_weight:
				if(side == BasicCutter::source_side)
					score = -source_dist;
				else
					score = target_dist;
				break;

			case flow_cutter::Config::PierceRating::max_target_minus_source_hop_dist_with_source_dist_tie_break:
				score = target_dist - source_dist;
				secondary_score = source_dist;
				break;
			case flow_cutter::Config::PierceRating::max_target_minus_source_hop_dist_with_closer_dist_tie_break:
				score = target_dist - source_dist;
				if (source_dist < target_dist) {
					secondary_score = -source_dist;
				} else {
					secondary_score = -target_dist;
				}
				break;

			default:
				assert(false);
				score = 0;
			}
			switch(config.avoid_augmenting_path){
				case flow_cutter::Config::AvoidAugmentingPath::avoid_and_pick_best:
					avoids_augmenting_path = !causes_augmenting_path;
					break;
				case flow_cutter::Config::AvoidAugmentingPath::do_not_avoid:
					break;
				case flow_cutter::Config::AvoidAugmentingPath::avoid_and_pick_oldest:
					avoids_augmenting_path = !causes_augmenting_path;
					if (!causes_augmenting_path) {
						score = std::numeric_limits<int>::max();
					}
					break;
				case flow_cutter::Config::AvoidAugmentingPath::avoid_and_pick_random:
					avoids_augmenting_path = !causes_augmenting_path;
					if(!causes_augmenting_path) {
						score = random_number();
					}
					break;
				default:
					assert(false);
					score = 0;
			}
			return std::make_tuple(avoids_augmenting_path, score, secondary_score);
		}
	};

	template<class Graph>
	class SimpleCutter{
	public:
		SimpleCutter(const Graph&graph, const flow_cutter::Config& config):
			graph(graph), cutter(config, graph.node_count()), config(config){
		}

		template<class GeoPos>
		void init(MultiCutter::TerminalInformation terminal_info, int random_seed, const GeoPos& geo_pos){
			DistanceType dist_type;

			if(
				config.pierce_rating == flow_cutter::Config::PierceRating::min_source_hop_dist ||
				config.pierce_rating == flow_cutter::Config::PierceRating::max_target_hop_dist ||
				config.pierce_rating == flow_cutter::Config::PierceRating::max_target_minus_source_hop_dist ||
				config.pierce_rating == flow_cutter::Config::PierceRating::circular_hop
			) {
                if (config.bulk_distance == flow_cutter::Config::BulkDistance::yes) {
                    dist_type = DistanceType::hop_distance_terminal_set;
                } else {
                    dist_type = DistanceType::hop_distance;
                }
            } else if(
				config.pierce_rating == flow_cutter::Config::PierceRating::min_source_weight_dist ||
				config.pierce_rating == flow_cutter::Config::PierceRating::max_target_weight_dist ||
				config.pierce_rating == flow_cutter::Config::PierceRating::circular_weight
			) {
                dist_type = DistanceType::weighted_distance;
			} else {
                dist_type = DistanceType::no_distance;
            }

			switch(config.graph_search_algorithm){
			case flow_cutter::Config::GraphSearchAlgorithm::pseudo_depth_first_search:
                cutter.init(graph, PseudoDepthFirstSearch(), dist_type,
                            std::move(terminal_info), config.bulk_distance_factor, random_seed,
                            geo_pos);
				break;

			case flow_cutter::Config::GraphSearchAlgorithm::breadth_first_search:
                cutter.init(graph, BreadthFirstSearch(), dist_type,
                            std::move(terminal_info), config.bulk_distance_factor, random_seed,
                            geo_pos);
				break;

			case flow_cutter::Config::GraphSearchAlgorithm::depth_first_search:
				throw std::runtime_error("depth first search is not yet implemented");
			default:
				assert(false);

			}
		}

		template<class ShallContinue, class ReportCut>
		void enum_cuts(const ShallContinue &shall_continue, const ReportCut &report_cut, const bool report_cuts_in_order) {
			switch(config.graph_search_algorithm){
			case flow_cutter::Config::GraphSearchAlgorithm::pseudo_depth_first_search:
				return cutter.enum_cuts(graph, PseudoDepthFirstSearch(), PierceNodeScore(config), shall_continue, report_cut, report_cuts_in_order);

			case flow_cutter::Config::GraphSearchAlgorithm::breadth_first_search:
				return cutter.enum_cuts(graph, BreadthFirstSearch(), PierceNodeScore(config), shall_continue, report_cut, report_cuts_in_order);

			case flow_cutter::Config::GraphSearchAlgorithm::depth_first_search:
				throw std::runtime_error("depth first search is not yet implemented");
			default:
				throw std::runtime_error("Invalid config option");
			}
		}

		CutterStateDump dump_state()const{
			return cutter.dump_state();
		}
	private:
		const Graph&graph;
		MultiCutter cutter;
		const flow_cutter::Config& config;
	};

	class CutterFactory {
	public:
		explicit CutterFactory(const flow_cutter::Config& config) : config(config) { }

		template<class Graph>
		SimpleCutter<Graph> operator()(const Graph&graph){
			return SimpleCutter<Graph>(graph, config);
		}

		template <typename GeoPos>
		MultiCutter::TerminalInformation select_source_target_pairs(int node_count, GeoPos &node_geo_pos, int /*cutter_count*/, int seed) {
			MultiCutter::TerminalInformation res;
			if (node_count < 750 && false) {	//disable. did not improve quality, but slowed down by 200 s, using 4 threads
				auto st = select_random_source_target_pairs(node_count, 20, seed);
				for (int i = 0; i < 20; ++i)
					res.push_back( { std::vector<int>(0), false, st[i], i });
			}
			else {
				compute_inertial_flow_orders(node_count, node_geo_pos, config.geo_pos_ordering_cutter_count, res);
				auto st = select_random_source_target_pairs(node_count, config.distance_ordering_cutter_count, seed);
				for (int i = 0; i < config.distance_ordering_cutter_count; ++i) {
					res.push_back({ std::vector<int>(0), true, st[i], config.geo_pos_ordering_cutter_count + i });
				}
			}
			return res;
		}

		template<class CompGeoPos>
		std::vector<int> build_geo_order(int node_count, const CompGeoPos &comp_geo_pos) {
			std::vector<int> node_order(node_count);
			std::iota(node_order.begin(), node_order.end(), 0);
			int max_bulk = std::max(static_cast<int>(config.bulk_assimilation_order_threshold * node_count), 1);
			std::nth_element(node_order.begin(), node_order.begin() + max_bulk, node_order.end(), comp_geo_pos);
			std::sort(node_order.begin(), node_order.begin() + max_bulk + 1, comp_geo_pos);
			std::nth_element(node_order.begin() + max_bulk + 1, node_order.end() - (max_bulk + 1), node_order.end(), comp_geo_pos);
			std::sort(node_order.end() - (max_bulk + 1), node_order.end(), comp_geo_pos);
			return node_order;
		}

		GeoPos polarToEuclidean(const double r, const double phi) {
			return { r * std::cos(phi), r * std::sin(phi) };
		}

		template<class GetGeoPos>
		void compute_inertial_flow_orders(int node_count, const GetGeoPos& geo_pos, const int cutter_count, MultiCutter::TerminalInformation& terminals) {
			if (cutter_count < 4) throw std::runtime_error("At least four inertial flow orders are required.");
			if (cutter_count % 4 != 0) std::cout << "Warning. Number of inertial flow cutters % 4 != 0. This means the horizontal, vertical and main diagonal lines will not be in the projection set." << std::endl;
			const double r = 1.0;
			const double pi  = 3.141592653589793238463;
			tbb::enumerable_thread_specific<MultiCutter::TerminalInformation> lt;

			auto build_one_geo_order =  [&](int i) {
				const double phi = i * pi / cutter_count;
				const GeoPos multiplier = polarToEuclidean(r, phi);
				auto geo_order = build_geo_order(node_count, [&](int l, int r) {
					return geo_pos(l).lat * multiplier.lat + geo_pos(l).lon * multiplier.lon <
						   geo_pos(r).lat * multiplier.lat + geo_pos(r).lon * multiplier.lon;
				});
				lt.local().push_back({std::move(geo_order), false, SourceTargetPair{-1, -1}, i});
			};
			if (node_count > MultiCutter::ParallelismCutoff/2)
				tbb::parallel_for(0, cutter_count, build_one_geo_order);
			else
				for (int i = 0; i < cutter_count; i++) { build_one_geo_order(i); }


			lt.combine_each([&](auto& local_mti) {
				std::move(local_mti.begin(), local_mti.end(), std::back_inserter(terminals));
				local_mti.clear();
			});
			std::sort(terminals.begin(), terminals.end(), [](const auto& a, const auto& b) { return a.cutter_id < b.cutter_id; });

		}

		SourceTargetPair select_random_source_target_pair(std::mt19937& rng, std::uniform_int_distribution<int>& dist) {
			int source = -1, target = -1;
			do{
				source = dist(rng);
				target = dist(rng);
			}while(source == target);
			return { source, target };
		}

		std::vector<SourceTargetPair>select_random_source_target_pairs(int node_count, int cutter_count, int seed){
			std::vector<SourceTargetPair>p;
			std::mt19937 rng(seed);
			std::uniform_int_distribution<int> dist(0, node_count-1);
			for(int i = 0; i < cutter_count; ++i) {
				p.push_back(select_random_source_target_pair(rng, dist));
			}
			return p;
		}

	private:
		const flow_cutter::Config& config;
	};

	inline
	bool requires_non_negative_weights(flow_cutter::Config config){
		return
			config.pierce_rating == flow_cutter::Config::PierceRating::min_source_weight_dist ||
			config.pierce_rating == flow_cutter::Config::PierceRating::max_target_weight_dist ||
			config.pierce_rating == flow_cutter::Config::PierceRating::max_target_minus_source_weight_dist;
	}

}

#endif

