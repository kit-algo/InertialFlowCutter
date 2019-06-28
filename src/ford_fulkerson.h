#ifndef FORD_FULKERSON_H
#define FORD_FULKERSON_H

#include "tiny_id_func.h"
#include "array_id_func.h"
#include <vector>

namespace max_flow{

	template<class Tail, class InvTail, class Head, class BackArc, class SourceList, class TargetList>
	class FordFulkersonAlgo{
	public:
		FordFulkersonAlgo(
				  const Tail&tail,
			const InvTail&inv_tail, const Head&head, const BackArc&back_arc,
			const SourceList&source_list, const TargetList&target_list,
			const BitIDFunc& is_source, const SourceList& source_front
		):
			node_count(head.image_count()), arc_count(head.preimage_count()),
			tail(tail), inv_tail(inv_tail), head(head), back_arc(back_arc),
			source_list(source_list), source_front(source_front),
			is_source(is_source), is_target(node_count),
			is_saturated(arc_count),
			stack(node_count), predecessor(node_count), is_reachable(node_count)
			{

			is_target.fill(false);
			for(int i=0; i<target_list.preimage_count(); ++i)
				is_target.set(target_list(i), true);

			flow_intensity = 0;
			is_saturated.fill(false);

			is_finished_flag = false;
			last_source = 0;
			num_reachable = 0;
			verify_flow_conservation();
		}
	private:
		int search_from(int source_node, BitIDFunc& reachable) {
			int stack_end = 1;
			stack[0] = source_node;

			while (stack_end != 0) {
				int x = stack[--stack_end];
				for(auto xy : inv_tail(x)){
					int y = head(xy);
					if(!reachable(y)){
						if(!is_saturated(xy)){
							reachable.set(y, true);
							++num_reachable;

							predecessor[y] = xy;
							if(is_target(y))
								return y;
							stack[stack_end++] = y;
						}
					}
				}
			}

			return -1;
		}

		bool augment_flow_from(int source_node) {
			int target_hit = search_from(source_node, is_reachable);
			if (target_hit != -1) {
				flow_intensity++;
				for (int x = target_hit; !is_source(x); x = tail(predecessor[x])) {
					auto a = predecessor[x];
					auto b = back_arc(a);
					if (is_saturated(b))
					    is_saturated.set(b, false);
					else
					    is_saturated.set(a, true);
				}
			}

			return (target_hit != -1);
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
			BitIDFunc assert_reachable(node_count);
			assert_reachable = is_source;
			int old_num_reachable = num_reachable;
			num_reachable = source_list.preimage_count();
			for (int u : source_list) {
				int target_hit = search_from(u, assert_reachable);	//technically not fine, but search_from looks correct
				assert(target_hit == -1);
			}
			for (int u = 0; u < node_count; ++u)
				assert(assert_reachable(u) == is_reachable(u));
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
			is_reachable = is_source;
			num_reachable = source_list.preimage_count();

			for (int i = last_source; i < source_front.preimage_count(); ++i) {
				if (augment_flow_from(source_front(i))) {
					last_source = i;
					return;
				}
			}

			for (int i = 0; i < last_source; ++i) {
				if (augment_flow_from(source_front(i))) {
					assert(false);
					last_source = i;
					return;
				}
			}

			is_finished_flag = true;
		}

		int get_current_flow_intensity()const{
			return flow_intensity;
		}

		int get_num_reachable_nodes() const {
			return num_reachable;
		}

		const BitIDFunc&get_saturated_flags()const{
			return is_saturated;
		}

		BitIDFunc move_saturated_flags()const{
			return std::move(is_saturated);
		}

		BitIDFunc move_reachable_flags() {
			return std::move(is_reachable);
		}

		bool is_reachable_from_source(const int u) const {
			return is_reachable(u);
		}

		bool is_finished()const{
			return is_finished_flag;
		}

	private:
		int node_count, arc_count;
		const Tail&tail;
		const InvTail&inv_tail;
		const Head&head;
		const BackArc&back_arc;
		const SourceList& source_list;
		const SourceList& source_front;

		const BitIDFunc& is_source;
		BitIDFunc is_target;
		BitIDFunc is_saturated;
		ArrayIDFunc<int> stack;
		ArrayIDFunc<int> predecessor;
		BitIDFunc is_reachable;
		int last_source;

		int flow_intensity;
		int num_reachable;
		bool is_finished_flag;
	};
}

#endif
