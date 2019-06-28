#ifndef IO_HELPER_H
#define IO_HELPER_H

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include "file_utility.h"

template<class SaveFunc, class ...Args>
void save_binary_file(const std::string&file_name, const SaveFunc&save, Args&&...args){
	std::ofstream out(file_name, std::ios::binary);
	if(!out)
		throw std::runtime_error("Could not open "+file_name+" for binary writing");
	save(out, std::forward<Args>(args)...);
}

template<class SaveFunc, class ...Args>
void save_text_file(const std::string&file_name, const SaveFunc&save, Args&&...args){
	if(file_name == "-"){
		save(std::cout, std::forward<Args>(args)...);
		std::cout << std::flush;
	} else if(file_name == "-null"){
	} else {
		std::ofstream out(file_name);
		if(!out)
			throw std::runtime_error("Could not open "+file_name+" for text writing");
		save(out, std::forward<Args>(args)...);
	}
}

template<class LoadFunc>
auto load_binary_file(const std::string&file_name, const LoadFunc&load)->decltype(load(std::cin, 0)){
	std::ifstream in(file_name, std::ios::binary);
	if(!in)
		throw std::runtime_error("Could not load "+file_name+" for binary reading");

	in.seekg (0, in.end);
	long long size = in.tellg();
	in.seekg (0, in.beg);

	return load(in, size);
}

template<class LoadFunc>
auto load_uncached_text_file(const std::string&file_name, const LoadFunc&load)->decltype(load(std::cin)){
	if(file_name == "-"){
		return load(std::cin);
	} else {
		std::ifstream in(file_name);
		if(!in)
			throw std::runtime_error("Could not load "+file_name+" for text reading");
		return load(in);
	}
}

template<class UncachedLoadFunc, class CachedLoadFunc, class CacheSaveFunc>
auto load_cached_text_file(
	const std::string&file_name,
	const std::string&format_name,
	const UncachedLoadFunc&uncached_load, 
	const CachedLoadFunc&cached_load, 
	const CacheSaveFunc&cache_save
)->decltype(uncached_load(std::cin)){
	if(file_name == "-"){
		return uncached_load(std::cin);
	} else {
		std::string cache_file_name = concat_file_path_and_file_name(
			get_temp_directory_path(), 
			"flow_cutter_cached_"+format_name+ "_" + uniquely_hash_file_name(make_absolute_file_name(file_name))
		);
	
		if(file_exists(cache_file_name))
			if(file_last_modified(file_name) < file_last_modified(cache_file_name)){
				std::ifstream in(cache_file_name, std::ios::binary);
				if(!in)
					throw std::runtime_error("Could not open binary cache file "+cache_file_name+" of "+file_name+" for reading");
				in.seekg (0, in.end);
				long long size = in.tellg();
				in.seekg (0, in.beg);
				return cached_load(in, size);
			}

		std::ifstream in(file_name);
		if(!in)
			throw std::runtime_error("Could not open text file "+file_name+" for reading");
					
		auto data = uncached_load(in);
		in.close();

		std::ofstream out(cache_file_name, std::ios::binary);
		
		if(out)
			cache_save(out, data);
			
		return data;
	}
}

#endif
