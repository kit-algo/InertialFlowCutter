
#ifndef FLOW_CUTTER_CONFIG_H
#define FLOW_CUTTER_CONFIG_H
#include <string>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace flow_cutter{
	struct Config{
		int cutter_count;
		int random_seed;
		int source;
		int target;
		int thread_count;
		int max_cut_size;
		float max_imbalance;
		int branch_factor;
		double chunk_size;
		double bulk_distance_factor;
		double bulk_assimilation_threshold;
		double bulk_assimilation_order_threshold;
		double initial_assimilated_fraction;
		double bulk_step_fraction;
		int geo_pos_ordering_cutter_count;
		int distance_ordering_cutter_count;

		enum class BulkDistance{
			no,
			yes
		};
		BulkDistance bulk_distance;

		enum class SeparatorSelection{
			node_min_expansion,
			edge_min_expansion,
			node_first,
			edge_first
		};
		SeparatorSelection separator_selection;

		enum class AvoidAugmentingPath{
			avoid_and_pick_best,
			do_not_avoid,
			avoid_and_pick_oldest,
			avoid_and_pick_random
		};
		AvoidAugmentingPath avoid_augmenting_path;

		enum class SkipNonMaximumSides{
			skip,
			no_skip
		};
		SkipNonMaximumSides skip_non_maximum_sides;

		enum class GraphSearchAlgorithm{
			pseudo_depth_first_search,
			breadth_first_search,
			depth_first_search
		};
		GraphSearchAlgorithm graph_search_algorithm;

		enum class DumpState{
			no,
			yes
		};
		DumpState dump_state;

		enum class ReportCuts{
			yes,
			no
		};
		ReportCuts report_cuts;

		enum class PierceRating{
			max_target_minus_source_hop_dist,
			min_source_hop_dist,
			max_target_hop_dist,
			max_target_minus_source_weight_dist,
			min_source_weight_dist,
			max_target_weight_dist,
			random,
			oldest,
			max_arc_weight,
			min_arc_weight,
			circular_hop,
			circular_weight,
			max_target_minus_source_hop_dist_with_source_dist_tie_break,
			max_target_minus_source_hop_dist_with_closer_dist_tie_break
		};
		PierceRating pierce_rating;

		Config():
			cutter_count(3),
			random_seed(5489),
			source(-1),
			target(-1),
			thread_count(1),
			max_cut_size(1000),
			max_imbalance(0.2),
			branch_factor(5),
			chunk_size(0.1),
			bulk_distance_factor(0.05),
			bulk_assimilation_threshold(0.4),
			bulk_assimilation_order_threshold(0.25),
			initial_assimilated_fraction(0.05),
			bulk_step_fraction(0.05),
			geo_pos_ordering_cutter_count(8),
			distance_ordering_cutter_count(0),
			bulk_distance(BulkDistance::no),
			separator_selection(SeparatorSelection::node_min_expansion),
			avoid_augmenting_path(AvoidAugmentingPath::avoid_and_pick_best),
			skip_non_maximum_sides(SkipNonMaximumSides::skip),
			graph_search_algorithm(GraphSearchAlgorithm::pseudo_depth_first_search),
			dump_state(DumpState::no),
			report_cuts(ReportCuts::yes),
			pierce_rating(PierceRating::max_target_minus_source_hop_dist){}

		void set(const std::string&var, const std::string&val){
			int val_id = -1;
			if(var == "BulkDistance" || var == "bulk_distance"){
				if(val == "no" || val_id == static_cast<int>(BulkDistance::no)) 
					bulk_distance = BulkDistance::no;
				else if(val == "yes" || val_id == static_cast<int>(BulkDistance::yes)) 
					bulk_distance = BulkDistance::yes;
				else throw std::runtime_error("Unknown config value "+val+" for variable BulkDistance; valid are no, yes");
			}else if(var == "SeparatorSelection" || var == "separator_selection"){
				if(val == "node_min_expansion" || val_id == static_cast<int>(SeparatorSelection::node_min_expansion)) 
					separator_selection = SeparatorSelection::node_min_expansion;
				else if(val == "edge_min_expansion" || val_id == static_cast<int>(SeparatorSelection::edge_min_expansion)) 
					separator_selection = SeparatorSelection::edge_min_expansion;
				else if(val == "node_first" || val_id == static_cast<int>(SeparatorSelection::node_first)) 
					separator_selection = SeparatorSelection::node_first;
				else if(val == "edge_first" || val_id == static_cast<int>(SeparatorSelection::edge_first)) 
					separator_selection = SeparatorSelection::edge_first;
				else throw std::runtime_error("Unknown config value "+val+" for variable SeparatorSelection; valid are node_min_expansion, edge_min_expansion, node_first, edge_first");
			}else if(var == "AvoidAugmentingPath" || var == "avoid_augmenting_path"){
				if(val == "avoid_and_pick_best" || val_id == static_cast<int>(AvoidAugmentingPath::avoid_and_pick_best)) 
					avoid_augmenting_path = AvoidAugmentingPath::avoid_and_pick_best;
				else if(val == "do_not_avoid" || val_id == static_cast<int>(AvoidAugmentingPath::do_not_avoid)) 
					avoid_augmenting_path = AvoidAugmentingPath::do_not_avoid;
				else if(val == "avoid_and_pick_oldest" || val_id == static_cast<int>(AvoidAugmentingPath::avoid_and_pick_oldest)) 
					avoid_augmenting_path = AvoidAugmentingPath::avoid_and_pick_oldest;
				else if(val == "avoid_and_pick_random" || val_id == static_cast<int>(AvoidAugmentingPath::avoid_and_pick_random)) 
					avoid_augmenting_path = AvoidAugmentingPath::avoid_and_pick_random;
				else throw std::runtime_error("Unknown config value "+val+" for variable AvoidAugmentingPath; valid are avoid_and_pick_best, do_not_avoid, avoid_and_pick_oldest, avoid_and_pick_random");
			}else if(var == "SkipNonMaximumSides" || var == "skip_non_maximum_sides"){
				if(val == "skip" || val_id == static_cast<int>(SkipNonMaximumSides::skip)) 
					skip_non_maximum_sides = SkipNonMaximumSides::skip;
				else if(val == "no_skip" || val_id == static_cast<int>(SkipNonMaximumSides::no_skip)) 
					skip_non_maximum_sides = SkipNonMaximumSides::no_skip;
				else throw std::runtime_error("Unknown config value "+val+" for variable SkipNonMaximumSides; valid are skip, no_skip");
			}else if(var == "GraphSearchAlgorithm" || var == "graph_search_algorithm"){
				if(val == "pseudo_depth_first_search" || val_id == static_cast<int>(GraphSearchAlgorithm::pseudo_depth_first_search)) 
					graph_search_algorithm = GraphSearchAlgorithm::pseudo_depth_first_search;
				else if(val == "breadth_first_search" || val_id == static_cast<int>(GraphSearchAlgorithm::breadth_first_search)) 
					graph_search_algorithm = GraphSearchAlgorithm::breadth_first_search;
				else if(val == "depth_first_search" || val_id == static_cast<int>(GraphSearchAlgorithm::depth_first_search)) 
					graph_search_algorithm = GraphSearchAlgorithm::depth_first_search;
				else throw std::runtime_error("Unknown config value "+val+" for variable GraphSearchAlgorithm; valid are pseudo_depth_first_search, breadth_first_search, depth_first_search");
			}else if(var == "DumpState" || var == "dump_state"){
				if(val == "no" || val_id == static_cast<int>(DumpState::no)) 
					dump_state = DumpState::no;
				else if(val == "yes" || val_id == static_cast<int>(DumpState::yes)) 
					dump_state = DumpState::yes;
				else throw std::runtime_error("Unknown config value "+val+" for variable DumpState; valid are no, yes");
			}else if(var == "ReportCuts" || var == "report_cuts"){
				if(val == "yes" || val_id == static_cast<int>(ReportCuts::yes)) 
					report_cuts = ReportCuts::yes;
				else if(val == "no" || val_id == static_cast<int>(ReportCuts::no)) 
					report_cuts = ReportCuts::no;
				else throw std::runtime_error("Unknown config value "+val+" for variable ReportCuts; valid are yes, no");
			}else if(var == "PierceRating" || var == "pierce_rating"){
				if(val == "max_target_minus_source_hop_dist" || val_id == static_cast<int>(PierceRating::max_target_minus_source_hop_dist)) 
					pierce_rating = PierceRating::max_target_minus_source_hop_dist;
				else if(val == "min_source_hop_dist" || val_id == static_cast<int>(PierceRating::min_source_hop_dist)) 
					pierce_rating = PierceRating::min_source_hop_dist;
				else if(val == "max_target_hop_dist" || val_id == static_cast<int>(PierceRating::max_target_hop_dist)) 
					pierce_rating = PierceRating::max_target_hop_dist;
				else if(val == "max_target_minus_source_weight_dist" || val_id == static_cast<int>(PierceRating::max_target_minus_source_weight_dist)) 
					pierce_rating = PierceRating::max_target_minus_source_weight_dist;
				else if(val == "min_source_weight_dist" || val_id == static_cast<int>(PierceRating::min_source_weight_dist)) 
					pierce_rating = PierceRating::min_source_weight_dist;
				else if(val == "max_target_weight_dist" || val_id == static_cast<int>(PierceRating::max_target_weight_dist)) 
					pierce_rating = PierceRating::max_target_weight_dist;
				else if(val == "random" || val_id == static_cast<int>(PierceRating::random)) 
					pierce_rating = PierceRating::random;
				else if(val == "oldest" || val_id == static_cast<int>(PierceRating::oldest)) 
					pierce_rating = PierceRating::oldest;
				else if(val == "max_arc_weight" || val_id == static_cast<int>(PierceRating::max_arc_weight)) 
					pierce_rating = PierceRating::max_arc_weight;
				else if(val == "min_arc_weight" || val_id == static_cast<int>(PierceRating::min_arc_weight)) 
					pierce_rating = PierceRating::min_arc_weight;
				else if(val == "circular_hop" || val_id == static_cast<int>(PierceRating::circular_hop)) 
					pierce_rating = PierceRating::circular_hop;
				else if(val == "circular_weight" || val_id == static_cast<int>(PierceRating::circular_weight)) 
					pierce_rating = PierceRating::circular_weight;
				else if(val == "max_target_minus_source_hop_dist_with_source_dist_tie_break" || val_id == static_cast<int>(PierceRating::max_target_minus_source_hop_dist_with_source_dist_tie_break)) 
					pierce_rating = PierceRating::max_target_minus_source_hop_dist_with_source_dist_tie_break;
				else if(val == "max_target_minus_source_hop_dist_with_closer_dist_tie_break" || val_id == static_cast<int>(PierceRating::max_target_minus_source_hop_dist_with_closer_dist_tie_break)) 
					pierce_rating = PierceRating::max_target_minus_source_hop_dist_with_closer_dist_tie_break;
				else throw std::runtime_error("Unknown config value "+val+" for variable PierceRating; valid are max_target_minus_source_hop_dist, min_source_hop_dist, max_target_hop_dist, max_target_minus_source_weight_dist, min_source_weight_dist, max_target_weight_dist, random, oldest, max_arc_weight, min_arc_weight, circular_hop, circular_weight, max_target_minus_source_hop_dist_with_source_dist_tie_break, max_target_minus_source_hop_dist_with_closer_dist_tie_break");
			}else if(var == "cutter_count"){
				int x = std::stoi(val);
				if(!(x>0))
					throw std::runtime_error("Value for \"cutter_count\" must fullfill \"x>0\"");
				cutter_count = x; 
			}else if(var == "random_seed"){
				random_seed = std::stoi(val); 
			}else if(var == "source"){
				int x = std::stoi(val);
				if(!(x>=-1))
					throw std::runtime_error("Value for \"source\" must fullfill \"x>=-1\"");
				source = x; 
			}else if(var == "target"){
				int x = std::stoi(val);
				if(!(x>=-1))
					throw std::runtime_error("Value for \"target\" must fullfill \"x>=-1\"");
				target = x; 
			}else if(var == "thread_count"){
				int x = std::stoi(val);
				if(!(x>=1))
					throw std::runtime_error("Value for \"thread_count\" must fullfill \"x>=1\"");
				thread_count = x; 
			}else if(var == "max_cut_size"){
				int x = std::stoi(val);
				if(!(x>=1))
					throw std::runtime_error("Value for \"max_cut_size\" must fullfill \"x>=1\"");
				max_cut_size = x; 
			}else if(var == "max_imbalance"){
				float x = std::stof(val);
				if(!(0.5>=x&&x>=0.0))
					throw std::runtime_error("Value for \"max_imbalance\" must fullfill \"0.5>=x&&x>=0.0\"");
				max_imbalance = x; 
			}else if(var == "branch_factor"){
				int x = std::stoi(val);
				if(!(x>=1))
					throw std::runtime_error("Value for \"branch_factor\" must fullfill \"x>=1\"");
				branch_factor = x; 
			}else if(var == "chunk_size"){
				double x = std::stof(val);
				if(!(0.5>=x&&x>=0.0))
					throw std::runtime_error("Value for \"chunk_size\" must fullfill \"0.5>=x&&x>=0.0\"");
				chunk_size = x; 
			}else if(var == "bulk_distance_factor"){
				double x = std::stof(val);
				if(!(0.5>x&&x>0.0))
					throw std::runtime_error("Value for \"bulk_distance_factor\" must fullfill \"0.5>x&&x>0.0\"");
				bulk_distance_factor = x; 
			}else if(var == "bulk_assimilation_threshold"){
				double x = std::stof(val);
				if(!(0.5>x&&x>0.0))
					throw std::runtime_error("Value for \"bulk_assimilation_threshold\" must fullfill \"0.5>x&&x>0.0\"");
				bulk_assimilation_threshold = x; 
			}else if(var == "bulk_assimilation_order_threshold"){
				double x = std::stof(val);
				if(!(0.5>x&&x>0.0))
					throw std::runtime_error("Value for \"bulk_assimilation_order_threshold\" must fullfill \"0.5>x&&x>0.0\"");
				bulk_assimilation_order_threshold = x; 
			}else if(var == "initial_assimilated_fraction"){
				double x = std::stof(val);
				if(!(0.5>x&&x>0.0))
					throw std::runtime_error("Value for \"initial_assimilated_fraction\" must fullfill \"0.5>x&&x>0.0\"");
				initial_assimilated_fraction = x; 
			}else if(var == "bulk_step_fraction"){
				double x = std::stof(val);
				if(!(0.5>x&&x>0.0))
					throw std::runtime_error("Value for \"bulk_step_fraction\" must fullfill \"0.5>x&&x>0.0\"");
				bulk_step_fraction = x; 
			}else if(var == "geo_pos_ordering_cutter_count"){
				int x = std::stoi(val);
				if(!(x>0))
					throw std::runtime_error("Value for \"geo_pos_ordering_cutter_count\" must fullfill \"x>0\"");
				geo_pos_ordering_cutter_count = x; 
			}else if(var == "distance_ordering_cutter_count"){
				int x = std::stoi(val);
				if(!(x>=0))
					throw std::runtime_error("Value for \"distance_ordering_cutter_count\" must fullfill \"x>=0\"");
				distance_ordering_cutter_count = x; 
			}else throw std::runtime_error("Unknown config variable "+var+"; valid are BulkDistance, SeparatorSelection, AvoidAugmentingPath, SkipNonMaximumSides, GraphSearchAlgorithm, DumpState, ReportCuts, PierceRating, cutter_count, random_seed, source, target, thread_count, max_cut_size, max_imbalance, branch_factor, chunk_size, bulk_distance_factor, bulk_assimilation_threshold, bulk_assimilation_order_threshold, initial_assimilated_fraction, bulk_step_fraction, geo_pos_ordering_cutter_count, distance_ordering_cutter_count");
		}
		std::string get(const std::string&var)const{
			if(var == "BulkDistance" || var == "bulk_distance"){
				if(bulk_distance == BulkDistance::no) return "no";
				else if(bulk_distance == BulkDistance::yes) return "yes";
				else {assert(false); return "";}
			}else if(var == "SeparatorSelection" || var == "separator_selection"){
				if(separator_selection == SeparatorSelection::node_min_expansion) return "node_min_expansion";
				else if(separator_selection == SeparatorSelection::edge_min_expansion) return "edge_min_expansion";
				else if(separator_selection == SeparatorSelection::node_first) return "node_first";
				else if(separator_selection == SeparatorSelection::edge_first) return "edge_first";
				else {assert(false); return "";}
			}else if(var == "AvoidAugmentingPath" || var == "avoid_augmenting_path"){
				if(avoid_augmenting_path == AvoidAugmentingPath::avoid_and_pick_best) return "avoid_and_pick_best";
				else if(avoid_augmenting_path == AvoidAugmentingPath::do_not_avoid) return "do_not_avoid";
				else if(avoid_augmenting_path == AvoidAugmentingPath::avoid_and_pick_oldest) return "avoid_and_pick_oldest";
				else if(avoid_augmenting_path == AvoidAugmentingPath::avoid_and_pick_random) return "avoid_and_pick_random";
				else {assert(false); return "";}
			}else if(var == "SkipNonMaximumSides" || var == "skip_non_maximum_sides"){
				if(skip_non_maximum_sides == SkipNonMaximumSides::skip) return "skip";
				else if(skip_non_maximum_sides == SkipNonMaximumSides::no_skip) return "no_skip";
				else {assert(false); return "";}
			}else if(var == "GraphSearchAlgorithm" || var == "graph_search_algorithm"){
				if(graph_search_algorithm == GraphSearchAlgorithm::pseudo_depth_first_search) return "pseudo_depth_first_search";
				else if(graph_search_algorithm == GraphSearchAlgorithm::breadth_first_search) return "breadth_first_search";
				else if(graph_search_algorithm == GraphSearchAlgorithm::depth_first_search) return "depth_first_search";
				else {assert(false); return "";}
			}else if(var == "DumpState" || var == "dump_state"){
				if(dump_state == DumpState::no) return "no";
				else if(dump_state == DumpState::yes) return "yes";
				else {assert(false); return "";}
			}else if(var == "ReportCuts" || var == "report_cuts"){
				if(report_cuts == ReportCuts::yes) return "yes";
				else if(report_cuts == ReportCuts::no) return "no";
				else {assert(false); return "";}
			}else if(var == "PierceRating" || var == "pierce_rating"){
				if(pierce_rating == PierceRating::max_target_minus_source_hop_dist) return "max_target_minus_source_hop_dist";
				else if(pierce_rating == PierceRating::min_source_hop_dist) return "min_source_hop_dist";
				else if(pierce_rating == PierceRating::max_target_hop_dist) return "max_target_hop_dist";
				else if(pierce_rating == PierceRating::max_target_minus_source_weight_dist) return "max_target_minus_source_weight_dist";
				else if(pierce_rating == PierceRating::min_source_weight_dist) return "min_source_weight_dist";
				else if(pierce_rating == PierceRating::max_target_weight_dist) return "max_target_weight_dist";
				else if(pierce_rating == PierceRating::random) return "random";
				else if(pierce_rating == PierceRating::oldest) return "oldest";
				else if(pierce_rating == PierceRating::max_arc_weight) return "max_arc_weight";
				else if(pierce_rating == PierceRating::min_arc_weight) return "min_arc_weight";
				else if(pierce_rating == PierceRating::circular_hop) return "circular_hop";
				else if(pierce_rating == PierceRating::circular_weight) return "circular_weight";
				else if(pierce_rating == PierceRating::max_target_minus_source_hop_dist_with_source_dist_tie_break) return "max_target_minus_source_hop_dist_with_source_dist_tie_break";
				else if(pierce_rating == PierceRating::max_target_minus_source_hop_dist_with_closer_dist_tie_break) return "max_target_minus_source_hop_dist_with_closer_dist_tie_break";
				else {assert(false); return "";}
			}else if(var == "cutter_count"){
				return std::to_string(cutter_count);
			}else if(var == "random_seed"){
				return std::to_string(random_seed);
			}else if(var == "source"){
				return std::to_string(source);
			}else if(var == "target"){
				return std::to_string(target);
			}else if(var == "thread_count"){
				return std::to_string(thread_count);
			}else if(var == "max_cut_size"){
				return std::to_string(max_cut_size);
			}else if(var == "max_imbalance"){
				return std::to_string(max_imbalance);
			}else if(var == "branch_factor"){
				return std::to_string(branch_factor);
			}else if(var == "chunk_size"){
				return std::to_string(chunk_size);
			}else if(var == "bulk_distance_factor"){
				return std::to_string(bulk_distance_factor);
			}else if(var == "bulk_assimilation_threshold"){
				return std::to_string(bulk_assimilation_threshold);
			}else if(var == "bulk_assimilation_order_threshold"){
				return std::to_string(bulk_assimilation_order_threshold);
			}else if(var == "initial_assimilated_fraction"){
				return std::to_string(initial_assimilated_fraction);
			}else if(var == "bulk_step_fraction"){
				return std::to_string(bulk_step_fraction);
			}else if(var == "geo_pos_ordering_cutter_count"){
				return std::to_string(geo_pos_ordering_cutter_count);
			}else if(var == "distance_ordering_cutter_count"){
				return std::to_string(distance_ordering_cutter_count);
			}else throw std::runtime_error("Unknown config variable "+var+"; valid are BulkDistance,SeparatorSelection,AvoidAugmentingPath,SkipNonMaximumSides,GraphSearchAlgorithm,DumpState,ReportCuts,PierceRating, cutter_count, random_seed, source, target, thread_count, max_cut_size, max_imbalance, branch_factor, chunk_size, bulk_distance_factor, bulk_assimilation_threshold, bulk_assimilation_order_threshold, initial_assimilated_fraction, bulk_step_fraction, geo_pos_ordering_cutter_count, distance_ordering_cutter_count");
		}
		std::string get_config()const{
			std::ostringstream out;
			out
				<< std::setw(30) << "BulkDistance" << " : " << get("BulkDistance") << '\n'
				<< std::setw(30) << "SeparatorSelection" << " : " << get("SeparatorSelection") << '\n'
				<< std::setw(30) << "AvoidAugmentingPath" << " : " << get("AvoidAugmentingPath") << '\n'
				<< std::setw(30) << "SkipNonMaximumSides" << " : " << get("SkipNonMaximumSides") << '\n'
				<< std::setw(30) << "GraphSearchAlgorithm" << " : " << get("GraphSearchAlgorithm") << '\n'
				<< std::setw(30) << "DumpState" << " : " << get("DumpState") << '\n'
				<< std::setw(30) << "ReportCuts" << " : " << get("ReportCuts") << '\n'
				<< std::setw(30) << "PierceRating" << " : " << get("PierceRating") << '\n'
				<< std::setw(30) << "cutter_count" << " : " << get("cutter_count") << '\n'
				<< std::setw(30) << "random_seed" << " : " << get("random_seed") << '\n'
				<< std::setw(30) << "source" << " : " << get("source") << '\n'
				<< std::setw(30) << "target" << " : " << get("target") << '\n'
				<< std::setw(30) << "thread_count" << " : " << get("thread_count") << '\n'
				<< std::setw(30) << "max_cut_size" << " : " << get("max_cut_size") << '\n'
				<< std::setw(30) << "max_imbalance" << " : " << get("max_imbalance") << '\n'
				<< std::setw(30) << "branch_factor" << " : " << get("branch_factor") << '\n'
				<< std::setw(30) << "chunk_size" << " : " << get("chunk_size") << '\n'
				<< std::setw(30) << "bulk_distance_factor" << " : " << get("bulk_distance_factor") << '\n'
				<< std::setw(30) << "bulk_assimilation_threshold" << " : " << get("bulk_assimilation_threshold") << '\n'
				<< std::setw(30) << "bulk_assimilation_order_threshold" << " : " << get("bulk_assimilation_order_threshold") << '\n'
				<< std::setw(30) << "initial_assimilated_fraction" << " : " << get("initial_assimilated_fraction") << '\n'
				<< std::setw(30) << "bulk_step_fraction" << " : " << get("bulk_step_fraction") << '\n'
				<< std::setw(30) << "geo_pos_ordering_cutter_count" << " : " << get("geo_pos_ordering_cutter_count") << '\n'
				<< std::setw(30) << "distance_ordering_cutter_count" << " : " << get("distance_ordering_cutter_count") << '\n';
			return out.str();
		}

	};
}
#endif
