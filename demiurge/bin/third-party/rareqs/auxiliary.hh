/* 
 * File:   auxiliary.hh
 * Author: mikolas
 *
 * Created on October 12, 2011
 */
#ifndef AUXILIARY_HH
#define	AUXILIARY_HH
#include <vector>
#include <iostream>
#include <sys/time.h>
#include <sys/resource.h>
using std::cerr;
using std::endl;
#define __PL (std::cerr << __FILE__ << ":" << __LINE__ << std::endl).flush();
#define CONSTANT const
#define CONTAINS(s,e) ( ((s).find(e)) != ((s).end()) )
#define FOR_EACH(index,iterated)\
  for (auto index = (iterated).begin(); index != (iterated).end();++index)

#define RANGE(i,s,n) for (size_t i = (s); i<(n); ++i)
#define RANGEA(i,s,n) for (auto i = (s); i<(n); ++i)

static inline double read_cpu_time() {
  struct rusage ru; getrusage(RUSAGE_SELF, &ru);
  return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}
#endif	/* AUXILIARY_HH */

