#ifndef EDMOND_KARP_H
#define EDMOND_KARP_H

#include "tiny_id_func.h"
#include "array_id_func.h"

namespace max_flow{
	template<class InvTail, class Head, class BackArc, class Source, class Target>
	BitIDFunc compute_maximum_unit_flow_using_edmond_karp(
		const InvTail&inv_tail, const Head&head, const BackArc&back_arc,
		const Source&source_list, const Target&target_list
	){
		int node_count = head.image_count();
		int arc_count = head.preimage_count();

		BitIDFunc is_target(node_count);
		is_target.fill(false);
		for(int i=0; i<target_list.preimage_count(); ++i)
			is_target.set(target_list(i), true);

		BitIDFunc is_source(node_count);
		is_source.fill(false);
		for(int i=0; i<source_list.preimage_count(); ++i)
			is_source.set(source_list(i), true);


		ArrayIDFunc<int> queue(node_count);
		int queue_begin = 0;
		int queue_end = 0;
		BitIDFunc was_pushed(node_count);
	
		auto advance_queue_pos = [&](int pos){
			if(pos == node_count-1)
				return 0;
			else
				return pos+1;
		};

		auto push = [&](int x){
			queue[queue_end] = x;
			queue_end = advance_queue_pos(queue_end);
			was_pushed.set(x, true);
		};

		auto pop = [&]{
			auto x = queue[queue_begin];
			queue_begin = advance_queue_pos(queue_begin);
			return x;
		};

		auto clear = [&]{
			queue_begin = 0;
			queue_end = 0;
			was_pushed.fill(false);
		};

		auto is_empty = [&]{
			return queue_begin == queue_end;
		};


		struct Pred{
			int node;
			int arc;
		};

		ArrayIDFunc<Pred> pred(node_count);

		BitIDFunc flow(arc_count);
		flow.fill(false);

		auto find_augmenting_path = [&](int s){
			clear();
			push(s);
			while(!is_empty()){
				auto x = pop();
				for(auto xy:inv_tail(x)){
					if(!flow(xy)){
						int y = head(xy);
						if(!was_pushed(y) && !is_source(y)){
							pred[y].node = x;
							pred[y].arc = xy;
							if(is_target(y)){
								return y;
							}else{
								push(y);
							}
						}
					}
				}
			}
			return -1;
		};

		auto augment_flow_along_path = [&](int s, int y){
			while(y != s){
				auto x = pred(y).node;
				auto xy = pred(y).arc;
				auto yx = back_arc(xy);

				assert(!flow(xy));
				if(flow(yx))
					flow.set(yx, false);
				else
					flow.set(xy, true);
				y = x;
			}
		};

		auto check_flow_invariants = [&]{
			#ifndef NDEBUG
			for(int i=0; i<arc_count; ++i)
				assert(!flow(i) || !flow(back_arc(i)));

		
			for(int x=0; x<node_count; ++x){
				int surplus = 0;
				for(int xy:inv_tail(x)){
					if(flow(xy))
						++surplus;
					if(flow(back_arc(xy)))
						--surplus;
				}
				if(is_source(x))
					assert(surplus >= 0);
				else if(is_target(x))
					assert(surplus <= 0);
				else
					assert(surplus == 0);
			}

			for(int x=0; x<node_count; ++x)
				assert(!is_source(x) || !is_target(x));	
			#endif
		};

		check_flow_invariants();
		for(int i=0; i<source_list.preimage_count(); ++i){
			auto s = source_list(i);
			auto t = find_augmenting_path(s);
			while(t != -1){
				augment_flow_along_path(s, t);
				check_flow_invariants();
				t = find_augmenting_path(s);
			}
		}
	
		return flow;
	}
}

#endif

