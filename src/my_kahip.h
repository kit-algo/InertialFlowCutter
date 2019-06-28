#ifndef MY_KAHIP_H
#define MY_KAHIP_H

#include "array_id_func.h"
#include "id_func.h"
#include "tiny_id_func.h"
#include "back_arc.h"
#include "geo_pos.h"
#include "inertial_flow.h"

#include <vector>
#include <cassert>

#include <kaHIP_interface.h>



namespace my_kahip{

	inline
	::inertial_flow::Cut compute_my_kahip_cut(
		const RangeIDIDMultiFunc&inv_tail, const ArrayIDIDFunc&head,
		double min_balance
	){

		int node_count = head.image_count();
		//int arc_count = head.preimage_count();

		//std::vector<int>one_node_weights(node_count, 1);
		//std::vector<int>one_arc_weights(arc_count, 1);
		std::vector<int>part_of_node(node_count);

		int part_count = 2;
		int cut_size;

		int* n = &node_count;
		int* vwgt = /*&one_node_weights[0]*/nullptr;
		int* xadj = (int*)&inv_tail.range_begin[0]; 
		int* adjcwgt = /*&one_arc_weights[0]*/nullptr;
		int* adjncy = head.data_;
		int* nparts = &part_count;
		double* imbalance = &min_balance;
		bool suppress_output = true;
		int seed = 42;
		int mode = STRONG;

		int* edgecut = &cut_size;
		int* part = &part_of_node[0];

		kaffpa(n, vwgt, xadj, adjcwgt, adjncy,nparts, imbalance, suppress_output, seed, mode, edgecut, part);

		BitIDFunc reachable = id_func(node_count, [&](unsigned x){return part[x] == 1;});

		int reachable_count = 0;
		for(int x=0; x<node_count; ++x)
			if(reachable(x))
				++reachable_count;
			
		cut_size = 0;
		for(int x=0; x<node_count; ++x)
			for(auto xy:inv_tail(x))
				if(reachable(x) != reachable(head(xy)))
					++cut_size;
		cut_size /= 2;

		if(reachable_count <= node_count/2)
			return ::inertial_flow::Cut{std::move(reachable), reachable_count, cut_size};
		else
			return ::inertial_flow::Cut{~std::move(reachable), node_count-reachable_count, cut_size};
	}

	inline
	::inertial_flow::Cut compute_my_kahip_cut(
		const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, 
		double min_balance
	){
		if(!std::is_sorted(tail.begin(), tail.end()))
			throw std::runtime_error("tail must be sorted");
		return compute_my_kahip_cut(invert_sorted_id_id_func(tail), head, min_balance);
	}

	inline
	std::vector<int> compute_my_kahip_separator(const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, double min_balance){
		const int arc_count = head.preimage_count();	
		const int node_count = head.image_count();

		std::vector<int>sep;
		if(node_count == 1){
			sep = {0};
		} else {
			::inertial_flow::Cut c = compute_my_kahip_cut(tail, head, min_balance);
			
			for(int i=0; i<arc_count; ++i)
				if(c.is_on_smaller_side(tail(i)) && !c.is_on_smaller_side(head(i)))
					sep.push_back(head(i));
		}
		return sep; // NVRO
	}

	struct MyKahipSeparator{
		MyKahipSeparator(double min_balance):
			min_balance(min_balance){}

		template<class InputNodeID, class ArcWeight>
		std::vector<int>operator()(const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const InputNodeID& /*input_node_id*/, const ArcWeight&/*arc_weight*/)const{
			return compute_my_kahip_separator(
				tail, head, 
				min_balance
			);
		}

		double min_balance;
	};

	inline
	MyKahipSeparator
		ComputeSeparator(double min_balance){
		return {min_balance};
	}



	inline
	std::vector<int> compute_my_kahip2_separator(const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, double min_balance){
		RangeIDIDMultiFunc inv_tail = invert_sorted_id_id_func(tail);

		int node_count = head.image_count();
		//int arc_count = head.preimage_count();

		//std::vector<int>one_node_weights(node_count, 1);
		//std::vector<int>one_arc_weights(arc_count, 1);

		int part_count = 2;
		int separator_size;
		int*separator_pointer;

		int* n = &node_count;
		int* vwgt = /*&one_node_weights[0]*/nullptr;
		int* xadj = (int*)&inv_tail.range_begin[0]; 
		int* adjcwgt = /*&one_arc_weights[0]*/nullptr;
		int* adjncy = head.data_;
		int* nparts = &part_count;
		double* imbalance = &min_balance;
		bool suppress_output = true;
		int seed = 42;
		int mode = STRONG;

		node_separator(n, vwgt, xadj, adjcwgt, adjncy, nparts, imbalance, suppress_output, seed, mode, &separator_size, &separator_pointer);

		std::vector<int>separator_list(separator_size);
		std::copy(separator_pointer, separator_pointer + separator_size, separator_list.begin());
		delete[]separator_pointer;
		return separator_list;
	}

	struct MyKahip2Separator{
		MyKahip2Separator(double min_balance):
			min_balance(min_balance){}

		template<class InputNodeID, class ArcWeight>
		std::vector<int>operator()(const ArrayIDIDFunc&tail, const ArrayIDIDFunc&head, const InputNodeID& /*input_node_id*/, const ArcWeight&/*arc_weight*/)const{
			return compute_my_kahip2_separator(
				tail, head, 
				min_balance
			);
		}

		double min_balance;
	};

	inline
	MyKahip2Separator
		ComputeSeparator2(double min_balance){
		return {min_balance};
	}
}

#endif

