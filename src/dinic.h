#ifndef DINIC_H
#define DINIC_H

#include "tiny_id_func.h"
#include "array_id_func.h"
#include <vector>

namespace max_flow{

	template<class Tail, class InvTail, class Head, class BackArc, class SourceList, class TargetList>
	class UnitDinicAlgo{
	public:
		UnitDinicAlgo(
			      const Tail&,
			const InvTail&inv_tail, const Head&head, const BackArc&back_arc,
			const SourceList&source_list, const TargetList&target_list,
			const BitIDFunc& is_source, const SourceList& source_front
		):
			node_count(head.image_count()), arc_count(head.preimage_count()),
			inv_tail(inv_tail), head(head), back_arc(back_arc), source_list(source_list),
			source_front(source_front),
			is_source(is_source), is_target(node_count),
			is_saturated(arc_count), is_blocked(arc_count), 
			queue(node_count), was_pushed(node_count), 
			is_on_same_level_or_lower(node_count), current_path_node(node_count), current_path_arc(node_count-1){

			is_target.fill(false);
			for(int i=0; i<target_list.preimage_count(); ++i)
				is_target.set(target_list(i), true);

			flow_intensity = 0;
			num_reachable = 0;
			is_saturated.fill(false);

			is_finished_flag = false;
		}

	private:
		bool compute_blocking_flow(){
			bool target_reachable = false;
			is_blocked.fill(false);
			is_on_same_level_or_lower = is_source;
			num_reachable = source_list.preimage_count();
			was_pushed.fill(false);
			int queue_begin = 0;
			int queue_end = source_front.preimage_count();
			std::copy(source_front.begin(), source_front.end(), queue.begin());
			int queue_current_level_end = queue_end;

			while(queue_begin != queue_end){

				for(int i=queue_begin; i<queue_current_level_end; ++i){
					is_on_same_level_or_lower.set(queue(i), true);
				}

				for(int i=queue_begin; i<queue_current_level_end; ++i){
					auto x = queue(i);
					for(auto xy:inv_tail(x)){
						if(is_saturated(xy)){
							is_blocked.set(xy, true);
						} else {
							auto y = head(xy);
							if(is_on_same_level_or_lower(y)){
								is_blocked.set(xy, true);
							} else {
								if(is_target(y)){
									target_reachable = true;
								} else {						
									if(!was_pushed(y)){
										++num_reachable;
										queue[queue_end++] = y;
										was_pushed.set(y, true);
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

		int find_first_non_block_out_arc(int x)const{
			for(int xy:inv_tail(x))
				if(!is_blocked(xy))
					return xy;
			return -1;
		}

		void augment_all_non_blocked_path(){
			for(int i=0; i<source_front.preimage_count(); ++i){
				current_path_node[0] = source_front[i];
				current_path_arc[0] = source_front[i];
				int current_path_arc_count = 0;
				for(;;){
					auto x = current_path_node[current_path_arc_count];
					auto xy = find_first_non_block_out_arc(x);
					if(xy == -1){
						if(current_path_arc_count == 0)
							break;
						--current_path_arc_count;
						is_blocked.set(current_path_arc[current_path_arc_count], true);
					}else{
						auto y = head(xy);
						current_path_arc[current_path_arc_count] = xy;
						++current_path_arc_count;
						current_path_node[current_path_arc_count] = y;
						if(is_target(y)){
							for(int i=0; i<current_path_arc_count; ++i){
								auto a = current_path_arc[i];
								is_blocked.set(a, true);
								auto b = back_arc(a);
								if(is_saturated(b))
									is_saturated.set(b, false);
								else
									is_saturated.set(a, true);
							}
							current_path_arc_count = 0;
							++flow_intensity;
						}
					}
				}
			}
		}

		int excess_at_node(int u) {
			int excess = 0;
			for (int uv : inv_tail(u)) {
				excess -= static_cast<int>(is_saturated(uv));
				excess += static_cast<int>(is_saturated(back_arc(uv)));
			}
			return excess;
		}

	public:
		void verify_flow_is_maximum() {
#ifndef NDEBUG
			int old_num_reachable = num_reachable;
			BitIDFunc old_is_blocked = is_blocked;
			BitIDFunc old_was_pushed = was_pushed;
			BitIDFunc old_is_on_same_level_or_lower = is_on_same_level_or_lower;
			assert(!compute_blocking_flow());		//should technically not be done with compute_blocking_flow. but we know it works
			for (int u = 0; u < node_count; ++u) {
				assert(was_pushed(u) == old_was_pushed(u));
				assert(is_on_same_level_or_lower(u) == old_is_on_same_level_or_lower(u));
			}
			for (int e = 0; e < arc_count; ++e) {
				assert(is_blocked(e) == old_is_blocked(e));
			}
			assert(num_reachable == old_num_reachable);
			verify_flow_conservation();
#endif
		}

		void verify_flow_conservation() {
#ifndef NDEBUG
			int source_excess = 0;
			int target_excess = 0;
			for (int u = 0; u < node_count; ++u) {
				if (is_source(u)) source_excess += excess_at_node(u);
				else if (is_target(u)) target_excess += excess_at_node(u);
				else assert (excess_at_node(u) == 0);
			}
			assert(source_excess == - flow_intensity);
			assert(target_excess == flow_intensity);
			for (int e = 0; e < arc_count; ++e)
				assert(!(is_saturated(e) && is_saturated(back_arc(e))));
#endif
		}


		void advance(){
			if(!is_finished_flag && compute_blocking_flow()){
				augment_all_non_blocked_path();
				is_finished_flag = false;
			}else{
				is_finished_flag = true;
			}
		}

		int get_current_flow_intensity()const{
			return flow_intensity;
		}

		int get_num_reachable_nodes()const{
			return num_reachable;
		}

		const BitIDFunc&get_saturated_flags()const{
			return is_saturated;
		}

		BitIDFunc move_saturated_flags()const{
			return std::move(is_saturated);
		}

		BitIDFunc move_reachable_flags() {
			return std::move(is_on_same_level_or_lower);
		}

		bool is_reachable_from_source(const int u) const {
			return is_on_same_level_or_lower(u);
		}

		bool is_finished()const{
			return is_finished_flag;
		}

	private:
		int node_count, arc_count;
		const InvTail&inv_tail;
		const Head&head;
		const BackArc&back_arc;
		const SourceList& source_list;
		const SourceList& source_front;

		const BitIDFunc& is_source;
		BitIDFunc is_target;
		BitIDFunc is_saturated;
		int flow_intensity;
		int num_reachable;
		BitIDFunc is_blocked;

		ArrayIDFunc<int> queue;
		BitIDFunc was_pushed; 
		BitIDFunc is_on_same_level_or_lower;

		ArrayIDFunc<int>current_path_node;
		ArrayIDFunc<int>current_path_arc;

		bool is_finished_flag;
	};
}

#endif

