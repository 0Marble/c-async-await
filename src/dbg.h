#ifndef __DBG_H__
#define __DBG_H__

#ifdef DEBUG
#include <stdio.h>

#define DBG(fmt, args...)                                                      \
  do {                                                                         \
    fprintf(stderr, "[%s:%d] %s: " fmt "\n", __FILE_NAME__, __LINE__,          \
            __func__, args);                                                   \
  } while (0)
#else
#define DBG(fmt, args...)
#endif

#endif // !__DBG_H__
