#ifndef PREORDER_H
#define PREORDER_H

#include "array_id_func.h"
#include "tiny_id_func.h"
#include <type_traits>

template<class Out>
std::pair<ArrayIDIDFunc, size_t> compute_preorder(const Out&out){
	const int node_count = out.preimage_count();

	ArrayIDIDFunc p(node_count, node_count);

	BitIDFunc seen(node_count);
	seen.fill(false);

	typedef typename std::decay<decltype(out(0).begin())>::type Iter;
	ArrayIDFunc<Iter>next_out(node_count);
	for(int i=0; i<node_count; ++i)
		next_out[i] = std::begin(out(i));

	ArrayIDFunc<int> stack(node_count);
	int stack_end = 0;

	int id = 0;	
	size_t num_components = 0;

	for(int r=0; r<node_count; ++r){
		if(!seen(r)){
			num_components++;	//If it becomes necessary later on, we might collect the first nodes of the components or something
			int x = r;
			seen.set(x, true);
//			p[x] = id++;
			p[id++] = x;

			for(;;){
				if(next_out[x] != std::end(out(x))){
					int y = *next_out[x]++;
					if(!seen(y)){
						seen.set(y, true);
//						p[y] = id++;
						p[id++] = y;
						stack[stack_end++] = x;
						x = y;
					}
					
				}else{
					if(stack_end == 0)
						break;
					x = stack[--stack_end];
				}
			}
		}
	}

	return std::make_pair(p, num_components);
}

#endif

