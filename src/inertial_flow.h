#ifndef INERTIAL_FLOW_H
#define INERTIAL_FLOW_H

#include "array_id_func.h"
#include "id_func.h"
#include "tiny_id_func.h"
#include "back_arc.h"
#include "geo_pos.h"
#include "edmond_karp.h"
#include "dinic.h"
#include "ford_fulkerson.h"
#include "permutation.h"
#include <vector>
#include <cassert>
#include <tuple>

namespace inertial_flow{

	struct Cut{
		BitIDFunc is_on_smaller_side;
		int smaller_side_size;
		int cut_size;
	};

	template<class CompGeoPos>
	void build_source_and_target_list(int node_count, double min_balance, const CompGeoPos&comp_geo_pos, ArrayIDIDFunc&source_list, ArrayIDIDFunc&target_list){
		ArrayIDIDFunc node_order = identity_permutation(node_count);
		int min_side_size = std::max(static_cast<int>(min_balance*node_count), 1);

		std::nth_element(node_order.begin(), node_order.begin() + min_side_size, node_order.end(), comp_geo_pos);
		if (min_side_size + 1 < node_count - min_side_size) {
			std::nth_element(node_order.begin() + min_side_size, node_order.begin() + (node_count - min_side_size), node_order.end(), comp_geo_pos);
		}

		source_list = id_id_func(min_side_size, node_count, [&](int x){return node_order[x];});
		target_list = id_id_func(min_side_size, node_count, [&](int x){return node_order[node_count-x-1];});

	}

	template<class InvTail, class Head>
	std::tuple<BitIDFunc, ArrayIDIDFunc> build_is_source_and_source_front(
									  const InvTail&inv_tail, const Head&head, const ArrayIDIDFunc& source_list) {
		const int node_count = head.image_count();
		BitIDFunc is_source(node_count);
		is_source.fill(false);
		for (int u : source_list) {
			is_source.set(u, true);
		}

		BitIDFunc is_source_front(source_list.preimage_count());
		is_source_front.fill(false);
		int source_front_size = 0;

		for (int i = 0; i < source_list.preimage_count(); ++i) {
			int u = source_list[i];
			for (auto uv : inv_tail(u)) {
				int v = head(uv);
				if (!is_source(v)) {
					is_source_front.set(i, true);
					++source_front_size;
					break;
				}
			}
		}

		ArrayIDIDFunc source_front(source_front_size, node_count);
		int si = 0;
		for (int i = 0; i < source_list.preimage_count(); ++i) {
			if (!is_source_front(i)) {
				continue;
			}
			source_front[si] = source_list[i];
			++si;
		}
		assert(si == source_front.preimage_count());

		return {std::move(is_source), std::move(source_front)};
	}

	template<class Tail, class InvTail, class Head, class BackArc, class GetGeoPos, typename FlowAlgo>
	Cut compute_inertial_flow_cut(
				      const Tail& tail,
		const InvTail&inv_tail, const Head&head, const BackArc&back_arc,
		const GetGeoPos&geo_pos, 
		double min_balance
	){
		const int node_count = head.image_count();

		ArrayIDIDFunc 
			source_list[4], 
			target_list[4];
		BitIDFunc is_source[4];
		ArrayIDIDFunc source_front[4];

		build_source_and_target_list(
			node_count, min_balance, 
			[&](int l, int r)->bool{return geo_pos(l).lon < geo_pos(r).lon;}, 
			source_list[0], target_list[0]
		);
		build_source_and_target_list(
			node_count, min_balance, 
			[&](int l, int r)->bool{return geo_pos(l).lat < geo_pos(r).lat;}, 
			source_list[1], target_list[1]
		);
		build_source_and_target_list(
			node_count, min_balance, 
			[&](int l, int r)->bool{return geo_pos(l).lon+geo_pos(l).lat < geo_pos(r).lon+geo_pos(r).lat;}, 
			source_list[2], target_list[2]
		);
		build_source_and_target_list(
			node_count, min_balance, 
			[&](int l, int r)->bool{return geo_pos(l).lon-geo_pos(l).lat < geo_pos(r).lon-geo_pos(r).lat;}, 
			source_list[3], target_list[3]
		);

		for (int i = 0; i < 4; ++i) {
			std::tie(is_source[i], source_front[i]) = build_is_source_and_source_front(inv_tail, head, source_list[i]);
		}

		FlowAlgo instance [] = {
			{tail, inv_tail, head, back_arc, source_list[0], target_list[0], is_source[0], source_front[0]},
			{tail, inv_tail, head, back_arc, source_list[1], target_list[1], is_source[1], source_front[1]},
			{tail, inv_tail, head, back_arc, source_list[2], target_list[2], is_source[2], source_front[2]},
			{tail, inv_tail, head, back_arc, source_list[3], target_list[3], is_source[3], source_front[3]}
		};

		for(;;){
			int next_instance_index = 0;
			for(int i=1; i<4; ++i)
				if(instance[i].get_current_flow_intensity() < instance[next_instance_index].get_current_flow_intensity())
					next_instance_index = i;

			auto& next_instance = instance[next_instance_index];
			if(next_instance.is_finished()) {
				const int reachable_count = next_instance.get_num_reachable_nodes();
				const int flow_intensity = next_instance.get_current_flow_intensity();
				next_instance.verify_flow_is_maximum();
#ifndef NDEBUG
				const int arc_count = head.preimage_count();
				int cutsize = 0;
				for (int e = 0; e < arc_count; ++e)
					if (next_instance.is_reachable_from_source(tail(e)) && !next_instance.is_reachable_from_source(head(e)))
						cutsize++;
				assert(cutsize == flow_intensity);
#endif
				if(reachable_count <= node_count/2) {
					return Cut{next_instance.move_reachable_flags(), reachable_count, flow_intensity};
				} else {
					return Cut{~next_instance.move_reachable_flags(), node_count-reachable_count, flow_intensity};
				}
			}

			next_instance.advance();
			next_instance.verify_flow_conservation();
		}
	}

	template<class Tail, class InvTail, class Head, class BackArc, class GetGeoPos>
	Cut compute_inertial_flow_cut(
				      const Tail& tail,
		const InvTail&inv_tail, const Head&head, const BackArc&back_arc,
		const GetGeoPos&geo_pos,
		double min_balance,
		bool use_dinic
	){
		if (use_dinic) {
			return compute_inertial_flow_cut<Tail, InvTail, Head, BackArc, GetGeoPos, max_flow::UnitDinicAlgo<Tail, InvTail, Head, BackArc, ArrayIDIDFunc, ArrayIDIDFunc>>(tail, inv_tail, head, back_arc, geo_pos, min_balance);
		} else {
			return compute_inertial_flow_cut<Tail, InvTail, Head, BackArc, GetGeoPos, max_flow::FordFulkersonAlgo<Tail, InvTail, Head, BackArc, ArrayIDIDFunc, ArrayIDIDFunc>>(tail, inv_tail, head, back_arc, geo_pos, min_balance);
		}
	}


	template<class Tail, class Head, class GetGeoPos>
	Cut compute_inertial_flow_cut(
		const Tail&tail, const Head&head, 
		const GetGeoPos&geo_pos, 
		double min_balance,
		bool use_dinic
	){
		if(std::is_sorted(tail.begin(), tail.end()))
			return compute_inertial_flow_cut(tail, invert_sorted_id_id_func(tail), head, compute_back_arc_permutation(tail, head), geo_pos, min_balance, use_dinic);
		else
			return compute_inertial_flow_cut(tail, invert_id_id_func(tail), head, compute_back_arc_permutation(tail, head), geo_pos, min_balance, use_dinic);
	}

	template<class Tail, class Head, class GetGeoPos>
	std::vector<int> compute_inertial_flow_separator(const Tail&tail, const Head&head, const GetGeoPos&geo_pos, double min_balance, bool use_dinic){
		const int arc_count = head.preimage_count();	
		const int node_count = head.image_count();

		std::vector<int>sep;
		if(node_count == 1){
			sep = {0};
		} else {
			Cut c = compute_inertial_flow_cut(tail, head, geo_pos, min_balance, use_dinic);
			
			for(int i=0; i<arc_count; ++i)
				if(c.is_on_smaller_side(tail(i)) && !c.is_on_smaller_side(head(i)))
					sep.push_back(head(i));
			assert((int)sep.size() == c.cut_size);
			std::sort(sep.begin(), sep.end());
			auto last = std::unique(sep.begin(), sep.end());
			sep.erase(last, sep.end());
		}
		return sep; // NVRO
	}

	template<class GetGeoPos>
	struct InertialFlowSeparator{
		InertialFlowSeparator(const GetGeoPos&geo_pos, double min_balance, bool use_dinic):
			geo_pos(&geo_pos), min_balance(min_balance), use_dinic(use_dinic){}

		template<class Tail, class Head, class InputNodeID, class ArcWeight>
		std::vector<int>operator()(const Tail&tail, const Head&head, const InputNodeID& input_node_id, const ArcWeight&)const{
			const int node_count = head.image_count();
			return compute_inertial_flow_separator(
				tail, head, 
				id_func(node_count, [&](int x){return (*geo_pos)(input_node_id(x));}),
				min_balance,
				use_dinic
			);
		}

		const GetGeoPos*geo_pos;
		double min_balance;
		bool use_dinic;
	};

	template<class GetGeoPos>
	InertialFlowSeparator<GetGeoPos>
		ComputeSeparator(const GetGeoPos&geo_pos, double min_balance, bool use_dinic){
		return {geo_pos, min_balance, use_dinic};
	}
}

#endif

