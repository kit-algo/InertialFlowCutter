#ifndef ID_STRING_H
#define ID_STRING_H

#include <string>
#include <stdexcept>
#include "id_func_traits.h"

template<class F>
void forall_in_id_string(const std::string&str, const F&f){
	auto str_begin = str.begin(), str_end = str.end(), str_pos = str_begin;

	int id_begin = -1;
	int current_id = 0;
	bool seen_a_digit = false;
	for(;;){
		if(str_pos != str_end && (*str_pos == ':' || *str_pos == '-')){
			if(!seen_a_digit)
				throw std::runtime_error("integer missing at offset "+std::to_string(str_pos - str_begin + 1));
		}
		if(str_pos == str_end || *str_pos == ':'){
			if(id_begin == -1)
				f(current_id);
			else{
				if(id_begin > current_id)
					throw std::runtime_error("id range "+std::to_string(id_begin)+"-"+std::to_string(current_id)+" is invalid");
				for(int i=id_begin; i<=current_id; ++i)
					f(i);
			}
			if(str_pos == str_end)
				break;
			current_id = 0;
			id_begin = -1;
			seen_a_digit = false;
		}else if(*str_pos == '-'){
			id_begin = current_id;
			current_id = 0;
			seen_a_digit = false;
		}else if('0' <= *str_pos && *str_pos <= '9'){ // stricly speaking undefined behaviour; digits do not have to be in a concecutive range... they just always are.
			current_id *= 10;
			current_id += *str_pos - '0';
			seen_a_digit = true;
		}else
			throw std::runtime_error("Illegal character "+std::string(str_pos, str_pos+1)+" in id list at offset "+std::to_string(str_pos - str_begin + 1));
		++str_pos;
	}
}

template<class F>
void forall_in_id_string(const std::string&str, int id_count, const F&f){
	forall_in_id_string(str, 
		[&](int x){
			if(x < 0 || x >= id_count)
				throw std::runtime_error("id " +std::to_string(x) + " is out of bounds [0,"+std::to_string(id_count)+")");
			f(x);
		}
	);
}

template<class F, REQUIRES(is_id_func<F>)>
std::string make_id_string(const F&f){
	std::string r;

	int begin = -1;
	bool first = true;

	for(int i=0; i<f.preimage_count(); ++i){
		bool in = f(i);
		if(in && begin == -1)
			begin = i;
		if(!in && begin != -1){
			if(first)
				first = false;
			else
				r += ":";
			if(begin != i-1)
				r += std::to_string(begin) + "-" + std::to_string(i-1);
			else
				r += std::to_string(begin);
			begin = -1;
		}
	}

	if(begin != -1){
		if(!first)
			r += ":";
		if(begin != f.preimage_count()-1)
			r += std::to_string(begin) + "-" + std::to_string(f.preimage_count()-1);
		else
			r += std::to_string(begin);
	}

	return r; // NVRO
}

template<class L, class BackArc>
std::string make_id_string_from_list_with_back_arcs(const L&l, const BackArc&back_arc){
	std::string s;

	bool first = true;
	for(auto x:l){
		if(!first)
			s += ":";
		else
			first = false;
		s += std::to_string(x);
		s += ":";
		s += std::to_string(back_arc(x));
	}

	return s; // NVRO
}

template<class L>
std::string make_id_string_from_list(const L&l){
	std::string s;

	bool first = true;
	for(auto x:l){
		if(!first)
			s += ":";
		else
			first = false;
		s += std::to_string(x);
	}

	return s; // NVRO
}

#endif
