#ifndef PERMUTATION_H
#define PERMUTATION_H

#include "tiny_id_func.h"
#include <string>

template<class IDIDFunc>
bool is_permutation(const IDIDFunc&f){
	if(f.preimage_count() != f.image_count())
		return false;

	int id_count = f.preimage_count();

	BitIDFunc already_seen(id_count);
	already_seen.fill(false);
	for(int i=0; i<id_count; ++i){
		int x = f(i);
		if(x < 0 || x >= id_count)
			return false;
		if(already_seen(x))
			return false;
		already_seen.set(x, true);
	}
	return true;
}



template<class IDIDFunc>
ArrayIDIDFunc inverse_permutation(const IDIDFunc&f){
	assert(is_permutation(f));

	int id_count = f.preimage_count();

	ArrayIDIDFunc inv_f(id_count, id_count);
	for(int i=0; i<id_count; ++i)
		inv_f[f(i)] = i;
	return inv_f; // NVRO
}

inline
ArrayIDIDFunc identity_permutation(int id_count){
	ArrayIDIDFunc f(id_count, id_count);
	for(int i=0; i<id_count; ++i)
		f[i] = i;
	return f; // NVRO
}

ArrayIDIDFunc load_permutation(const std::string& order_file);
ArrayIDIDFunc uncached_load_permutation(const std::string& order_file);
void save_permutation(const std::string& order_file, const ArrayIDIDFunc&order);



/*
template<class Permutation, class IDFunc>
typename std::enable_if<
	is_only_id_func<IDFunc>::value,
	ArrayIDFunc<typename id_func_image_type<IDFunc>::type>
>::type apply_permutation(const Permutation&p, const IDFunc&f){
	assert(is_permutation(p));
	assert(p.image_count() == f.preimage_count());

	ArrayIDFunc<typename id_func_image_type<IDFunc>::type> result(p.preimage_count());

	for(int i=0; i<p.preimage_count(); ++i)
		result[i] = f(p(i));

	return result; // NVRO
}

template<class Permutation, class IDIDFunc>
typename std::enable_if<
	is_id_id_func<IDIDFunc>::value,
	ArrayIDIDFunc
>::type apply_permutation(const Permutation&p, const IDIDFunc&f){
	assert(is_permutation(p));
	assert(p.image_count() == f.preimage_count());

	ArrayIDIDFunc result(p.preimage_count(), f.image_count());

	for(int i=0; i<p.preimage_count(); ++i)
		result[i] = f(p(i));

	return result; // NVRO
}
*/

#endif
