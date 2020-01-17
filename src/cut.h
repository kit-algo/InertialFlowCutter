#ifndef CUT_H
#define CUT_H

#include "node_flow_cutter.h"
#include "flow_cutter.h"
#include "flow_cutter_config.h"

#include <tbb/spin_mutex.h>

#include <deque>

namespace flow_cutter{

	template <typename CutterFactory, class GetGeoPos>
	class ComputeCut{
	public:
		explicit ComputeCut(const GetGeoPos& geo_pos, Config config, bool reorder_arcs = true):geo_pos(geo_pos), config(config), reorder_arcs(reorder_arcs){}

		template<class Tail, class Head, class InputNodeID, class ArcWeight>
		std::deque<int> operator()(const Tail&tail, const Head&head, const InputNodeID& input_node_id, const ArcWeight&arc_weight)const{
			const int node_count = tail.image_count();
			const int arc_count = tail.preimage_count();

			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);

			auto adapted_geo_pos = id_func(node_count, [&](int x){return geo_pos(input_node_id(x));});

			std::deque<int> best_cut;
			CutterFactory factory(config);

			switch(config.separator_selection){
				case Config::SeparatorSelection::node_min_expansion:
				{
					auto graph = flow_cutter::make_graph(
						make_const_ref_id_id_func(tail), 
						make_const_ref_id_id_func(head), 
						make_const_ref_id_id_func(back_arc), 
						make_const_ref_id_func(arc_weight),
						ConstIntIDFunc<1>(arc_count),//make_const_ref_id_func(arc_weight),//
						make_const_ref_id_func(out_arc)
					);
					auto cutter = factory(graph);
					auto pairs = factory.select_source_target_pairs(node_count, adapted_geo_pos, config.cutter_count, config.random_seed);

					std::atomic<double> best_score(std::numeric_limits<double>::max());
					int best_flow_intensity = std::numeric_limits<int>::max();
					int best_cutter_id = std::numeric_limits<int>::max();
					
					cutter.init(pairs, config.random_seed, adapted_geo_pos);
					tbb::spin_mutex current_cut_mutex;

					cutter.enum_cuts(
						/* shall_continue */
						[&](const auto& cutter) {
							double cut_size = cutter.get_current_flow_intensity();
							// If a cut is available, the next cut will be at least one larger
							if (cutter.cut_available()) {
								cut_size += 1;
							}
							double potential_best_next_score = cut_size/(double)(node_count/2);
							return potential_best_next_score < best_score.load(std::memory_order_acquire);
						},
						/* report_cut */
						[&](const auto& cutter, int cutter_id) {
							double cut_size = cutter.get_current_flow_intensity();
							double small_side_size = cutter.get_current_smaller_cut_side_size();

							double score = cut_size / small_side_size;


							if(cutter.get_current_smaller_cut_side_size() < config.max_imbalance * node_count)
								score += 1000000;

							if(score <= best_score.load(std::memory_order_acquire)) {
								tbb::spin_mutex::scoped_lock cut_lock(current_cut_mutex);

								double tmp_best_score = best_score.load(std::memory_order_acquire);
								if (std::tie(score, cut_size, cutter_id) < std::tie(tmp_best_score, best_flow_intensity, best_cutter_id)) {
									best_score.store(score, std::memory_order_release);
									best_flow_intensity = cut_size;
									best_cutter_id = cutter_id;
									/* order edges by direction */

									//TODO are those x real edges? are those all edges?
									std::deque<int> cur_cut;
									auto cut = cutter.get_current_cut();
									if (reorder_arcs) {
										for (auto x : cut) {
											if (cutter.is_on_smaller_side(head(x))) {
												cur_cut.push_back(x);
												cur_cut.push_front(back_arc(x));
											} else {
												cur_cut.push_front(x);
												cur_cut.push_back(back_arc(x));
											}
										}
									} else {
										for (auto x : cut) {
											cur_cut.push_back(x);
											cur_cut.push_back(back_arc(x));
										}
									}
									best_cut = cur_cut;
								}
							}
						},
						/* report_cuts_in_order */
						false);
				}
				break;
				default:
					throw std::logic_error("Invalid cut selection config");
			}
			return best_cut;
		}

	private:
		const GetGeoPos& geo_pos;
		Config config;
		const bool reorder_arcs;
	};
}

#endif
