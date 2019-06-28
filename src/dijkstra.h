#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "heap.h"
#include "tiny_id_func.h"
#include "array_id_func.h"
#include "timestamp_id_func.h"
#include <stdexcept>

template<class OutArc, class Head, class Weight>
class ForAllSuccessors{
public:
	ForAllSuccessors(OutArc out_arc, Head head, Weight weight):
		out_arc(std::move(out_arc)), head(std::move(head)), weight(std::move(weight)){}

	int preimage_count()const{
		return out_arc.preimage_count();
	}

	typedef typename id_func_image_type<Weight>::type WeightType;

	template<class F>
	void operator()(int x, const F&f)const{
		for(auto xy:out_arc(x))
			f(head(xy), weight(xy));
	}

private:
	OutArc out_arc;
	Head head;
	Weight weight;

};

template<class OutArc, class Head, class Weight>
ForAllSuccessors<OutArc, Head, Weight> make_forall_successors(
	OutArc out_arc, Head head, Weight weight
){
	return {std::move(out_arc), std::move(head), std::move(weight)};
}

template<class BoolIDFunc, class Dist = int>
class Dijkstra{
public:
	Dijkstra(){}

	explicit Dijkstra(int node_count):
		distance(node_count), queue(node_count), was_pushed(node_count){}

	void clear(){
		was_pushed.fill(false);
		queue.clear();
	}

	void add_source_node(int x, Dist d = 0){
		distance.set(x, d);
		queue.push_or_decrease_key(x, d);
		was_pushed.set(x, true);
	}

	bool was_reached(int x)const{
		return was_pushed(x);
	}

	// Returns an upper bound to the distance. This bound is tight if it is not larger than get_radius. This bound is numeric_limits<Dist>::max if x was not visited yet.
	Dist extract_current_distance(int x)const{
		if(was_pushed(x))
			return distance(x);
		else
			return std::numeric_limits<Dist>::max();
	}

	bool is_finished()const{
		return queue.empty();
	}

	// Removes one node from the queue and relaxes its outgoing arc. The node ID is returned.
	// * on_push_or_decrease_distance(x, new_node_pushed, pred) is called for each node that is added to the queue or its key is decreased.
	//     new_node_pushed is boolean that is true when a new node is pushed instead of just decreasing its current distance.
	//     pred is the ID of the node from which x was reached with the new distance.
	template<class OutArc, class Head, class Weight, class OnPushOrDecreaseDistance>
	int settle_next(
		const OutArc&out_arc, const Head&head,
		const Weight&weight,
		const OnPushOrDecreaseDistance&on_push_or_decrease_dist
	){
		return settle_next(
			make_forall_successors(
				make_const_ref_id_func(out_arc),
				make_const_ref_id_id_func(head),
				make_const_ref_id_func(weight)
			),
			on_push_or_decrease_dist
		);
	}

	template<class ForAllSuccessors, class OnPushOrDecreaseDistance>
	int settle_next(
		const ForAllSuccessors&forall_successors,
		const OnPushOrDecreaseDistance&on_push_or_decrease_dist
	){
		assert(!queue.empty());
		auto x = queue.pop();
		assert(was_pushed(x));
		Dist x_dist = distance(x);
		forall_successors(
			x,
			[&](int y, Dist xy_weight){
				#ifdef DIJKSTRA_RUNTIME_OVERFLOW_CHECK
				if(xy_weight < 0)
					throw std::runtime_error("arc weight is negative");
				#endif

				assert(xy_weight >= 0);
				Dist y_dist;
				if(was_pushed(y))
					y_dist = distance(y);
				else
					y_dist = std::numeric_limits<Dist>::max();

				if(x_dist < y_dist - xy_weight){

					#ifdef DIJKSTRA_RUNTIME_OVERFLOW_CHECK
					if(x_dist >= std::numeric_limits<Dist>::max() - xy_weight)
						throw std::runtime_error("path length exceeds 64 bits");
					#endif

					y_dist = x_dist + xy_weight;
					distance.set(y, y_dist);
					bool is_decrease_key = queue.contains(y);
					queue.push_or_decrease_key(y, y_dist);
					was_pushed.set(y, true);
					on_push_or_decrease_dist(y, !is_decrease_key, x);
				}
			}
		);
		return x;
	}

	Dist get_radius()const{
		if(queue.empty())
			return std::numeric_limits<Dist>::max();
		else
			return queue.peek_min_key();
	}

	int get_front_size()const{
		return queue.size();
	}

	ArrayIDFunc<Dist>move_distance_array(){
		return std::move(distance);
	}

private:
	ArrayIDFunc<Dist>distance;
	min_id_heap<Dist>queue;
	BoolIDFunc was_pushed;
};

template<class OutArc, class Head, class Weight>
void compute_distances(
	const OutArc&out, const Head&head, const Weight&weight,
	int source_node,
	BitIDFunc&visited, ArrayIDFunc<int>&dist, min_id_heap<int>&q
){
	q.clear();
	visited.fill(false);
	q.push(source_node, 0);
	visited.set(source_node, true);
	dist[source_node] = 0;
	while(!q.empty()){
		int x = q.pop();

		for(auto xy:out(x)){
			int y = head(xy);
			auto w = weight(xy);
			if(!visited(y) || dist[x] < dist[y] - w){
				visited.set(y, true);
				dist[y] = dist[x] + w;
				q.push_or_decrease_key(y, dist[y]);
			}
		}
	}
}

template<class OutArc, class Head, class Weight>
ArrayIDFunc<int>compute_distances(const OutArc&out, const Head&head, const Weight&weight, int source_node){
	const int node_count = head.image_count();

	BitIDFunc visited(node_count);
	ArrayIDFunc<int>dist(node_count);
	min_id_heap<int>q(node_count);

	dist.fill(std::numeric_limits<int>::max());

	compute_distances(out, head, weight, source_node, visited, dist, q);

	return dist; // NVRO
}

template<class OutArc, class Head, class Weight, class Dist, class OnFirst, class OnLast>
void depth_first_traverse_shortest_path_tree(
	const OutArc&out, const Head&head, const Weight&weight,
	const Dist&dist,
	int source_node,
	const OnFirst&on_first, const OnLast&on_last,
	BitIDFunc&pushed, ArrayIDFunc<int>&stack
){
	int stack_top = 1;
	stack[0] = source_node;

	pushed.fill(false);
	pushed.set(source_node, true);

	while(stack_top != 0){
		int x = stack[--stack_top];
		if(x < 0)
			on_last(~x);
		else{
			stack[stack_top++] = ~x;

			on_first(x);

			for(auto xy:out(x)){
				auto y = head(xy);
				if(!pushed(y) && dist(x) + weight(xy) == dist(y)){
					stack[stack_top++] = y;
					pushed.set(y, true);
				}
			}
		}
	}
}

template<class OutArc, class Head, class Weight, class OnFirst, class OnLast>
void depth_first_traverse_shortest_path_tree(
	const OutArc&out, const Head&head, const Weight&weight,
	int source_node,
	const OnFirst&on_first, const OnLast&on_last
){
	const int node_count = head.image_count();

	BitIDFunc visited(node_count);
	ArrayIDFunc<int>dist(node_count);

	{
		min_id_heap<int>q(node_count);
		compute_distances(out, head, weight, source_node, visited, dist, q);
	}

	{
		ArrayIDFunc<int>stack(node_count);
		depth_first_traverse_shortest_path_tree(out, head, weight, dist, source_node, on_first, on_last, visited, stack);
	}
}

#endif

