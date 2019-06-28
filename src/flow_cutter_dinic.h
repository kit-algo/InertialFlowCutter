#ifndef FLOW_CUTTER_DINIC_H
#define FLOW_CUTTER_DINIC_H

#include "tiny_id_func.h"
#include "array_id_func.h"

namespace flow_cutter_accelerated{

	class UnitDinicAlgo{
	public:
	    template<class Graph>
		explicit UnitDinicAlgo(
			const Graph& graph
		):
			node_count(graph.node_count()), arc_count(graph.arc_count()),
			is_blocked(arc_count),
			queue(node_count),
			is_on_same_level_or_lower(node_count), current_path_node(node_count), current_path_arc(node_count-1){

			flow_intensity = 0;

			is_finished_flag = false;
		}

	private:
	    template<class Graph, class SourceList, class SourceSet, class TargetSet, class SourceReachable, class IsSaturated>
		bool compute_blocking_flow(
		        const Graph& graph,
                const SourceList& source_list,
                const SourceSet& source_set,
                const TargetSet& target_set,
                SourceReachable& source_reachable,
                const IsSaturated& is_saturated){
		    assert(!source_list.empty());
		    auto is_target = [&](int x) {
		        return target_set.is_inside(x);
		    };
			bool target_reachable = false;
			is_blocked.fill(false);
			is_on_same_level_or_lower.fill(false);
			source_reachable.reset(source_set, false);
			int queue_begin = 0;
			int queue_end = source_list.size();
			for(size_t i=0; i<source_list.size(); ++i)
				queue[i] = source_list[i];
			int queue_current_level_end = queue_end;

			while(queue_begin != queue_end){

				for(int i=queue_begin; i<queue_current_level_end; ++i){
					is_on_same_level_or_lower.set(queue(i), true);
				}

				for(int i=queue_begin; i<queue_current_level_end; ++i){
					auto x = queue(i);
					assert(source_reachable.is_inside(x));
					for(auto xy : graph.out_arc(x)){
						if(is_saturated(xy)){
							is_blocked.set(xy, true);
						} else {
							auto y = graph.head(xy);
							if(is_on_same_level_or_lower(y) || source_set.is_inside(y)){
								is_blocked.set(xy, true);
							} else {
								if(is_target(y)){
									target_reachable = true;
								} else {						
									if(!source_reachable.is_inside(y)){
										queue[queue_end++] = y;
										source_reachable.add_node(graph, y);
									}
								}
							}
						}
					}
				}

				queue_begin = queue_current_level_end;
				queue_current_level_end = queue_end;			
			}

			return target_reachable;
		}

		template<class Graph>
		int find_first_non_block_out_arc(const Graph& graph, int x)const{
			for(int xy : graph.out_arc(x))
				if(!is_blocked(xy))
					return xy;
			return -1;
		}

		template<class Graph, class SourceList, class SourceSet, class TargetSet, class IsSaturated, class IncreaseFlow>
		int augment_all_non_blocked_path(const Graph& graph, const SourceList& source_list, const SourceSet& source_set, const TargetSet& target_set, IsSaturated is_saturated, IncreaseFlow& increase_flow){
			(void)source_set;
			(void)is_saturated;
			auto is_target = [&](int x) {
				return target_set.is_inside(x);
			};
			int augmented_intensity = 0;
			for(size_t i=0; i<source_list.size(); ++i){
				current_path_node[0] = source_list[i];
				current_path_arc[0] = source_list[i];
				int current_path_arc_count = 0;
				for(;;){
					auto x = current_path_node[current_path_arc_count];
					auto xy = find_first_non_block_out_arc(graph, x);
					if(xy == -1){
						if(current_path_arc_count == 0)
							break;
						--current_path_arc_count;
						is_blocked.set(current_path_arc[current_path_arc_count], true);
					}else{
						assert(!is_blocked(xy));
						auto y = graph.head(xy);
						assert(!source_set.is_inside(y));
						assert(!is_saturated(xy));
						current_path_arc[current_path_arc_count] = xy;
						++current_path_arc_count;
						current_path_node[current_path_arc_count] = y;
						if(is_target(y)){
							for(int i=0; i<current_path_arc_count; ++i){
								auto a = current_path_arc[i];
								is_blocked.set(a, true);
								increase_flow(a);
							}
							current_path_arc_count = 0;
							++augmented_intensity;
							++flow_intensity;
						}
					}
				}
			}
		    return augmented_intensity;
		}

	public:
        void init() {
           flow_intensity = 0;
           is_finished_flag = false;
        }

	    template<class Graph, class SourceList, class SourceSet, class TargetSet, class SourceReachable, class IsSaturated, class IncreaseFlow>
		int advance(
		        const Graph& graph,
                const SourceList& source_list,
                const SourceSet& source_set,
                const TargetSet& target_set,
                SourceReachable& source_reachable,
                const IsSaturated& is_saturated,
                const IncreaseFlow& increase_flow
		        ){
			if(!is_finished_flag && compute_blocking_flow(graph, source_list, source_set, target_set, source_reachable, is_saturated)){
				int augmented_intensity = augment_all_non_blocked_path(graph, source_list, source_set, target_set, is_saturated, increase_flow);
				is_finished_flag = false;
				return augmented_intensity;
			}else{
				is_finished_flag = true;
				return 0;
			}
		}

		int get_current_flow_intensity()const{
			return flow_intensity;
		}

		bool is_finished()const{
			return is_finished_flag;
		}

		template<class Graph, class SourceSet, class TargetSet, class SourceReachable, class IsSaturated, class IncreaseFlow>
		int run(const Graph& graph,
		        const SourceSet& source_set,
		        const TargetSet& target_set,
		        SourceReachable& source_reachable,
		        const IsSaturated& is_saturated,
		        const IncreaseFlow& increase_flow) {
		    assert(!source_set.get_extra_nodes().empty());
		    flow_intensity = 0;
		    is_finished_flag = false;
		    do {
		        advance(graph, source_set.get_extra_nodes(), source_set, target_set, source_reachable, is_saturated, increase_flow);
		    } while (!is_finished());
		    return get_current_flow_intensity();
		}

	private:
		int node_count, arc_count;

		int flow_intensity;
		BitIDFunc is_blocked;

		ArrayIDFunc<int> queue;
		BitIDFunc is_on_same_level_or_lower;

		ArrayIDFunc<int>current_path_node;
		ArrayIDFunc<int>current_path_arc;

		bool is_finished_flag;
	};

}

#endif

