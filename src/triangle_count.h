#ifndef TRIANGLE_COUNT_H
#define TRIANGLE_COUNT_H
#include "multi_arc.h"
#include "tiny_id_func.h"
#include "id_multi_func.h"
#include "back_arc.h"
#include "id_sort.h"
#include <cassert>

template<class Tail, class Head>
ArrayIDFunc<int> count_arc_triangles(const Tail&tail, const Head&head){
	assert(is_symmetric(tail, head));

	int node_count = tail.image_count();
	int arc_count = tail.preimage_count();

	auto deg = compute_histogram(tail);
	ArrayIDFunc<int>nodes_decreasing_by_deg(node_count);
	stable_sort_copy_by_id(
		CountIterator{0}, CountIterator{node_count}, 
		std::begin(nodes_decreasing_by_deg), 
		id_id_func(node_count, node_count, [&](int x){ return node_count - deg(x) - 1;})
	);

	auto out_arc = invert_id_id_func(tail);

	BitIDFunc is_finished(node_count), is_neighbor(node_count);
	is_finished.fill(false);
	is_neighbor.fill(false);

	ArrayIDFunc<int>triangle_count(arc_count);
	triangle_count.fill(0);

	for(auto x:nodes_decreasing_by_deg){
		for(auto xy:out_arc(x)){
			auto y = head(xy);
			if(!is_finished(y))
				is_neighbor.set(y, true);
		}

		for(auto xy:out_arc(x)){
			auto y = head(xy);
			if(is_neighbor(y)){
				for(auto yz:out_arc(y)){
					auto z = head(yz);
					if(is_neighbor(z)){
						++triangle_count[xy];
						if(y < z)
							++triangle_count[yz];
					}
				}
			}
		}

		for(auto xy:out_arc(x)){
			is_neighbor.set(head(xy), false);
		}

		is_finished.set(x, true);
	}

	auto back_arc = compute_back_arc_permutation(tail, head);
	for(int xy = 0; xy < arc_count; ++xy){
		auto yx = back_arc(xy);
		if(xy < yx){
			int s = triangle_count(xy) + triangle_count(yx);
			triangle_count[xy] = s;
			triangle_count[yx] = s;
		}
	}

	return triangle_count;
}


#endif
