#include "permutation.h"
#include "io_helper.h"
#include "tiny_id_func.h"
#include <stdexcept>
#include <vector>

static
ArrayIDIDFunc load_permutation_impl(std::istream&in){

	std::vector<int>buffer;
	int x;
	while(in >> x)
		buffer.push_back(x);

	const int n = buffer.size();

	BitIDFunc seen(n);
	seen.fill(false);

	for(auto x:buffer){
		if(x < 0)
			throw std::runtime_error("an order position can not be negative");
		else if(x > n)
			throw std::runtime_error("an order position can not be larger than the number of elements");
		else if(seen(x))
			throw std::runtime_error("an order position can only appear once");
		seen.set(x, true);
	}

	ArrayIDIDFunc order(n, n);

	for(int i=0; i<n; ++i){
		order[buffer[i]] = i;
	}

	return order;
}

ArrayIDIDFunc load_binary_permutation_impl(std::istream&in, long long size){
	if(size % 4 != 0)
		throw std::runtime_error("file size of binary cache does not match for order file");
	ArrayIDIDFunc order((int)size/4, (int)size/4);
	in.read((char*)order.begin(), size);
	return order;
}

void save_binary_permutation_impl(std::ostream&out, const ArrayIDIDFunc&order){
	out.write((const char*)order.begin(), order.preimage_count()*4);
}

ArrayIDIDFunc load_permutation(const std::string&file_name){
	return load_cached_text_file(file_name, "dimacs", load_permutation_impl, load_binary_permutation_impl, save_binary_permutation_impl);
}

ArrayIDIDFunc uncached_load_permutation(const std::string&file_name){
	return load_uncached_text_file(file_name, load_permutation_impl);
}

static
void save_permutation_impl(std::ostream&out, const ArrayIDIDFunc&perm){
	for(auto x:inverse_permutation(perm))
			out << x << '\n';
}

void save_permutation(const std::string&file_name, const ArrayIDIDFunc&perm){
	save_text_file(file_name, save_permutation_impl, perm);
}


