#ifndef __CHECKED_H
#define __CHECKED_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int _checked(int ret, char* calling_function) {
  if (ret < 0) {
    perror(calling_function);
    exit(EXIT_FAILURE);
  }
  return ret;
}

// The macro allows us to retrieve the name of the calling function
#define checked(call) _checked(call, #call)

#define checked_wr(call) _checked(((call) - 1), #call)

// Macro for read-like calls
#define checked_rd(call) _checked((call), #call)

#endif  // __CHECKED_H
