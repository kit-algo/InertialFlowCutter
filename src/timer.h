#ifndef TIMER_H
#define TIMER_H

#include <sys/time.h>

inline
long long get_micro_time(){
	timeval t;
	gettimeofday(&t, 0);
	return t.tv_sec*1000000ll+t.tv_usec;
}

#endif
