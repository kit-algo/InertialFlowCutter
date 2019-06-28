#ifndef TIMESTAMP_ID_FUNC_H
#define TIMESTAMP_ID_FUNC_H

#include "array_id_func.h"

//! A bool id func with a constant time fill(false). Note that fill(true) is expensive
class TimestampIDFunc{
public:
	TimestampIDFunc(){}
	explicit TimestampIDFunc(int node_count):
		timestamp(node_count){
		timestamp.fill(0);
		current_timestamp = 1;
	}

	bool operator()(int x)const{
		return timestamp(x) == current_timestamp;
	}

	void fill(bool f){
		if(!f){
			++current_timestamp;
			if(current_timestamp == 0){
				timestamp.fill(0);
				current_timestamp = 1;
			}
		}else{
			timestamp.fill(0);
			current_timestamp = 0;
		}
	}

	void set(int x, bool f){
		timestamp.set(x, current_timestamp-!f);	
	}

private:
	ArrayIDFunc<unsigned short>timestamp;
	unsigned short current_timestamp;
};

#endif

