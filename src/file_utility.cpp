#include "file_utility.h"
#include <stdexcept>
#include <cctype>
#include <limits>
#include <sstream>

#ifndef _WIN32
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#else
#include <windows.h>
#endif

std::time_t file_last_modified(const std::string&file_name){
	if(file_name.empty())
		throw std::runtime_error("The empty string is no filename.");
	#ifndef _WIN32
	struct stat st;
	if(stat(file_name.c_str(), &st)){
		if(errno == ENOENT)
			throw std::runtime_error("File "+file_name+" not found");
		throw std::runtime_error("Error while reading timestamp of "+file_name+" : "+strerror(errno));
	}
	return st.st_mtime; // I have no idea what unit this is, the docs wont tell...
	#else
	HANDLE file = CreateFile(
		file_name.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if(file ==  INVALID_HANDLE_VALUE){
		int error_code = GetLastError();
		throw std::runtime_error("CreateFile failed with error code "+std::to_string(error_code));
	}else{
		FILETIME filetime;
		int ok = GetFileTime(file, NULL, NULL, &filetime);
		int error_code = GetLastError();
		CloseHandle(file);
		if(!ok)
			throw std::runtime_error("GetFileTime failed with error code "+std::to_string(error_code));
		ULARGE_INTEGER t;
		t.LowPart = filetime.dwLowDateTime;
		t.HighPart = filetime.dwHighDateTime;
		return t.QuadPart/10000; // convert to milliseconds
	}
	#endif
}

std::string concat_file_path_and_file_name(std::string path, const std::string&name){
	if(path.empty() || path[path.length()-1] == 
	#ifndef _WIN32
		'/'
	#else
		'\\'
	#endif
	){
		path += name;
	}else{
		path += 
		#ifndef _WIN32
			"/"
		#else
			"\\"
		#endif
		;
		path += name;	
	}
	return path;
}

std::string make_absolute_file_name(const std::string&file_name){
	#ifndef _WIN32
	char path[PATH_MAX+1];
	if(realpath(file_name.c_str(), path) == nullptr)
		throw std::runtime_error(std::string("realpath failed for file \""+file_name+"\" with the error message : ")+strerror(errno));
	return path;
	#else
	char path[MAX_PATH];
	GetFullPathName(file_name.c_str(), path, MAX_PATH, NULL);
	return path;
	#endif
}

std::string get_temp_directory_path(){
	#ifndef _WIN32
	const char*cache_tmp_file = getenv ("CACHETMPDIR");
	if(cache_tmp_file)
		return cache_tmp_file;
	const char*tmp_file = getenv ("TMPDIR");
	if(tmp_file)
		return tmp_file;
	return "/tmp/";
	#else
	char buf[MAX_PATH];
	GetTempPath(buf, MAX_PATH);
	return buf;
	#endif
}

std::string uniquely_hash_file_name(const std::string&file_name){
	std::ostringstream out;
	for(auto c:file_name)
		if(std::isalnum(c))
			out << c;
	return out.str();
}

bool file_exists(const std::string&file_name){
	#ifndef _WIN32
	struct stat buffer;   
	return stat(file_name.c_str(), &buffer) == 0; 
	#else
	DWORD dwAttrib = GetFileAttributes(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	#endif
}

