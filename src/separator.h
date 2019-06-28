#ifndef SEPARATOR_H
#define SEPARATOR_H

#include "node_flow_cutter.h"
#include "flow_cutter.h"
#include "flow_cutter_config.h"
#include "union_find.h"
#include "tiny_id_func.h"
#include "min_max.h"
#include "timer.h"

#include <tbb/spin_mutex.h>

namespace flow_cutter{

	template <typename CutterFactory, class GetGeoPos>
	class ComputeSeparator{
	public:
		explicit ComputeSeparator(const GetGeoPos& geo_pos, Config config):geo_pos(geo_pos), config(config){}

		template<class Tail, class Head, class InputNodeID, class ArcWeight>
		std::vector<int> operator()(const Tail&tail, const Head&head, const InputNodeID& input_node_id, const ArcWeight&arc_weight)const{

			const int node_count = tail.image_count();
			const int arc_count = tail.preimage_count();

			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);

			auto adapted_geo_pos = id_func(node_count, [&](int x){return geo_pos(input_node_id(x));});

			std::vector<int>separator;

			CutterFactory factory(config);

			switch(config.separator_selection){
				case Config::SeparatorSelection::node_min_expansion:
				{
					
					auto expanded_graph = expanded_graph::make_graph(
						make_const_ref_id_id_func(tail), 
						make_const_ref_id_id_func(head), 
						make_const_ref_id_id_func(back_arc), 
						make_const_ref_id_id_func(arc_weight), 
						make_const_ref_id_func(out_arc)
					);
					auto cutter = factory(expanded_graph);
					auto pairs = factory.select_source_target_pairs(node_count, adapted_geo_pos, config.cutter_count, config.random_seed);

					std::atomic<double> best_score(std::numeric_limits<double>::max());
					int best_flow_intensity = std::numeric_limits<int>::max();
					int best_cutter_id = std::numeric_limits<int>::max();

					auto expanded_geo_pos = id_func(expanded_graph.node_count(), [&](int x) -> GeoPos {return adapted_geo_pos(expanded_graph::expanded_node_to_original_node(x));});
					cutter.init(expanded_graph::expand_source_target_pair_list(std::move(pairs)), config.random_seed, expanded_geo_pos);

					tbb::spin_mutex current_cut_mutex;

					cutter.enum_cuts(
							 /* shall_continue */
							 [&](const auto& cutter) {
								 double cut_size = cutter.get_current_flow_intensity();
								 // If a cut is available, the next cut will be at least one larger
								 if (cutter.cut_available()) {
									 cut_size += 1;
								 }
								double potential_best_next_score = cut_size/(double)(expanded_graph::expanded_node_count(node_count)/2);
								return potential_best_next_score <= best_score.load(std::memory_order_acquire);
							 },
							 /* report_cut */
							 [&](const auto& cutter, int cutter_id) {
								double cut_size = cutter.get_current_flow_intensity();
								double small_side_size = cutter.get_current_smaller_cut_side_size();

								double score = cut_size / small_side_size;

								if(cutter.get_current_smaller_cut_side_size() < config.max_imbalance * expanded_graph::expanded_node_count(node_count))
									score += 1000000;


								if(score <= best_score.load(std::memory_order_acquire)) {
									tbb::spin_mutex::scoped_lock cut_lock(current_cut_mutex);

									double tmp_best_score = best_score.load(std::memory_order_acquire);

									if (std::tie(score, cut_size, cutter_id) < std::tie(tmp_best_score, best_flow_intensity, best_cutter_id)) {
										best_score.store(score, std::memory_order_release);
										best_flow_intensity = cut_size;
										best_cutter_id = cutter_id;
										separator = expanded_graph::extract_original_separator_from_cut(tail, head, expanded_graph, cutter).sep;
									}
								}
							 },
							 /* report_cuts_in_order */
							 false);
				}
				break;
				case Config::SeparatorSelection::edge_min_expansion:
				{
					auto graph = flow_cutter::make_graph(
						make_const_ref_id_id_func(tail), 
						make_const_ref_id_id_func(head), 
						make_const_ref_id_id_func(back_arc), 
						make_const_ref_id_func(arc_weight),
						ConstIntIDFunc<1>(arc_count),
						make_const_ref_id_func(out_arc)
					);

					auto cutter = factory(graph);

					std::vector<int>best_cut;
					std::atomic<double> best_score(std::numeric_limits<double>::max());

					cutter.init(factory.select_source_target_pairs(node_count, adapted_geo_pos, config.cutter_count, config.random_seed), config.random_seed, adapted_geo_pos);

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
							 [&](const auto& cutter, int) {
								double cut_size = cutter.get_current_flow_intensity();
								double small_side_size = cutter.get_current_smaller_cut_side_size();

								double score = cut_size / small_side_size;

								if(cutter.get_current_smaller_cut_side_size() < config.max_imbalance * node_count)
									score += 1000000;


								if(score < best_score.load(std::memory_order_acquire)) {
									tbb::spin_mutex::scoped_lock cut_lock(current_cut_mutex);

									if (score < best_score.load(std::memory_order_acquire)) {
										best_score.store(score, std::memory_order_release);
										best_cut = cutter.get_current_cut();
									}

								}
							 },
							 /* report_cuts_in_order */
							 false);

					for(auto x:best_cut)
						separator.push_back(head(x));

					std::sort(separator.begin(), separator.end());
					separator.erase(std::unique(separator.begin(), separator.end()), separator.end());
				}
				break;
				case Config::SeparatorSelection::edge_first:
				{
					auto graph = flow_cutter::make_graph(
						make_const_ref_id_id_func(tail), 
						make_const_ref_id_id_func(head), 
						make_const_ref_id_id_func(back_arc), 
						make_const_ref_id_func(arc_weight),
						ConstIntIDFunc<1>(arc_count),
						make_const_ref_id_func(out_arc)
					);

					auto cutter = factory(graph);
					cutter.init(factory.select_source_target_pairs(node_count, adapted_geo_pos, config.cutter_count, config.random_seed), config.random_seed, adapted_geo_pos);

					int best_small_side_size = 0;
					std::vector<int> best_cut;
					std::atomic<int> best_cut_size(std::numeric_limits<int>::max());
					tbb::spin_mutex  current_cut_mutex;

					cutter.enum_cuts(
							 /* shall_continue */
							 [&](const auto& cutter) {
								 int cut_size = cutter.get_current_flow_intensity();
								 // If a cut is available, the next cut will be at least one larger
								 if (cutter.cut_available()) {
									 cut_size += 1;
								 }

								 return cut_size <= best_cut_size.load(std::memory_order_acquire);
							 },
							 /* report_cut */
							 [&](const auto& cutter, int) {
								int cut_size = cutter.get_current_flow_intensity();
								int small_side_size = cutter.get_current_smaller_cut_side_size();

								if(cutter.get_current_smaller_cut_side_size() < config.max_imbalance * node_count)
									return;


								if(cut_size <= best_cut_size.load(std::memory_order_acquire)) {
									tbb::spin_mutex::scoped_lock cut_lock(current_cut_mutex);

									const int tmp_best_cut_size = best_cut_size.load(std::memory_order_acquire);

									if (cut_size < tmp_best_cut_size ||
									    (cut_size == tmp_best_cut_size && best_small_side_size < small_side_size)) {
										best_cut_size.store(cut_size, std::memory_order_release);
										best_small_side_size = small_side_size;
										best_cut = cutter.get_current_cut();
									}
								}
							 },
							 /* report_cuts_in_order */
							 false);

					for(auto x : best_cut)
						separator.push_back(head(x));

					std::sort(separator.begin(), separator.end());
					separator.erase(std::unique(separator.begin(), separator.end()), separator.end());
				}
				break;
				case Config::SeparatorSelection::node_first:
				{
					auto expanded_graph = expanded_graph::make_graph(
						make_const_ref_id_id_func(tail), 
						make_const_ref_id_id_func(head), 
						make_const_ref_id_id_func(back_arc), 
						make_const_ref_id_id_func(arc_weight), 
						make_const_ref_id_func(out_arc)
					);

					auto cutter = factory(expanded_graph);
					auto pairs = factory.select_source_target_pairs(node_count, adapted_geo_pos, config.cutter_count, config.random_seed);

					auto expanded_geo_pos = id_func(expanded_graph.node_count(), [&](int x) -> GeoPos {return adapted_geo_pos(expanded_graph::expanded_node_to_original_node(x));});
					cutter.init(expanded_graph::expand_source_target_pair_list(pairs), config.random_seed, expanded_geo_pos);

					int best_small_side_size = 0;
					std::atomic<int> best_cut_size(std::numeric_limits<int>::max());
					tbb::spin_mutex  current_cut_mutex;

					cutter.enum_cuts(
							 /* shall_continue */
							 [&](const auto& cutter) {
								 int cut_size = cutter.get_current_flow_intensity();
								 // If a cut is available, the next cut will be at least one larger
								 if (cutter.cut_available()) {
									 cut_size += 1;
								 }

								 return cut_size <= best_cut_size.load(std::memory_order_acquire);
							 },
							 /* report_cut */
							 [&](const auto& cutter, int) {
								int cut_size = cutter.get_current_flow_intensity();
								int small_side_size = cutter.get_current_smaller_cut_side_size();

								if(cutter.get_current_smaller_cut_side_size() < config.max_imbalance * expanded_graph::expanded_node_count(node_count))
									return;


								if(cut_size <= best_cut_size.load(std::memory_order_acquire)) {
									tbb::spin_mutex::scoped_lock cut_lock(current_cut_mutex);

									const int tmp_best_cut_size = best_cut_size.load(std::memory_order_acquire);

									if (cut_size < tmp_best_cut_size ||
									    (cut_size == tmp_best_cut_size && best_small_side_size < small_side_size)) {
										best_cut_size.store(cut_size, std::memory_order_release);
										best_small_side_size = small_side_size;
										separator = expanded_graph::extract_original_separator_from_cut(tail, head, expanded_graph, cutter).sep;
									}
								}
							 },
							 /* report_cuts_in_order */
							 false);

				}
				break;
				default:
					throw std::logic_error("Invalid separator selection config");
			}

			return separator;

		}
	private:
		const GetGeoPos& geo_pos;
		Config config;
	};


	template <typename CutterFactory>
	class ComputeIsOnSmallerSideOfCut{
	public:
		explicit ComputeIsOnSmallerSideOfCut(Config config):config(config){}

		template<class Tail, class Head, class ArcWeight>
		BitIDFunc operator()(const Tail&tail, const Head&head, const ArcWeight&arc_weight)const{

			CutterFactory factory(config);

			const int node_count = tail.image_count();
			const int arc_count = tail.preimage_count();

			auto out_arc = invert_sorted_id_id_func(tail);
			auto back_arc = compute_back_arc_permutation(tail, head);


			auto graph = flow_cutter::make_graph(
				make_const_ref_id_id_func(tail), 
				make_const_ref_id_id_func(head), 
				make_const_ref_id_id_func(back_arc), 
				make_const_ref_id_func(arc_weight),
				ConstIntIDFunc<1>(arc_count),
				make_const_ref_id_func(out_arc)
			);
			auto cutter = factory(graph);
			cutter.init(factory.select_source_target_pairs(node_count, config.cutter_count, config.random_seed), config.random_seed);
			for(;;){
				if(cutter.get_current_smaller_cut_side_size() > node_count/20){
					return id_func(node_count, [&](unsigned x){return cutter.is_on_smaller_side(x);});
				}else{
					cutter.advance();
				}
			}
		}
	private:
		Config config;
	};
}

namespace separator{
	template<class Tail, class Head, class Sep>
	int determine_largest_part_size(const Tail&tail, const Head&head, const Sep&sep){
		const int node_count = tail.image_count();
		const int arc_count = tail.preimage_count();

		BitIDFunc not_in_sep(node_count);
		not_in_sep.fill(true);

		for(auto x:sep)
			not_in_sep.set(x, false);

		UnionFind uf(node_count);
		for(int i=0; i<arc_count; ++i){
			auto h = head(i), t = tail(i);
			if(not_in_sep(h) && not_in_sep(t))
				uf.unite(t, h);
		}

		int max_comp_size = 0;
		for(int i=0; i<node_count; ++i)
			if(uf.is_representative(i))
				max_to(max_comp_size, uf.component_size(i));

		return max_comp_size;
	}

	template<class ComputeSeparator>
	class ReportSeparatorStatistics{
	public:
		ReportSeparatorStatistics(std::ostream&out, ComputeSeparator compute_separator):
			out(out), compute_separator(std::move(compute_separator)){
			out << "node_count,arc_count,sep_node_count,large_node_count,running_time,reporting_running_time\n";
		}

		template<class Tail, class Head, class InputNodeID, class ArcWeight>
		std::vector<int> operator()(const Tail&tail, const Head&head, const InputNodeID&input_node_id, const ArcWeight&arc_weight)const{
		
			const int node_count = tail.image_count();
			const int arc_count = tail.preimage_count();

			long long running_time = -get_micro_time();
			auto sep = compute_separator(tail, head, input_node_id, arc_weight);
			running_time += get_micro_time();

			long long reporting_running_time = -get_micro_time();
			auto large_node_count = determine_largest_part_size(tail, head, sep);
			reporting_running_time += get_micro_time();

			out << node_count << ',' << arc_count << ',' << sep.size() << ',' << large_node_count << ',' << running_time << ',' << reporting_running_time << '\n';
			return sep;
		}
	
	private:
		std::ostream&out;
		ComputeSeparator compute_separator;
	};

	template<class ComputeSeparator>
	ReportSeparatorStatistics<ComputeSeparator> report_separator_statistics(std::ostream&out, ComputeSeparator compute_separator){
		return {out, std::move(compute_separator)};
	}
}

#endif

