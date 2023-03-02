/*
 * TimeUtils.h
 *
 *  Created on: 2 Mar 2023
 *      Author: elazarkin
 */

#ifndef INCLUDE_SMART_RTP_UTILS_TIMEUTILS_H_
#define INCLUDE_SMART_RTP_UTILS_TIMEUTILS_H_

#include <stdint.h>
#include <sys/time.h>

inline uint64_t getUseconds()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000000 + tp.tv_usec;
}

#endif /* INCLUDE_SMART_RTP_UTILS_TIMEUTILS_H_ */
