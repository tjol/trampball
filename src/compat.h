#ifndef COMPAT_H
#define COMPAT_H

#ifdef _MSC_VER
#  include <malloc.h>
#  include <string.h>
#  define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#else
#  include <alloca.h>
#  include <strings.h>
#endif

#include <SDL.h>
#define ntohl(x) SDL_SwapBE32(x)

#endif /* COMPAT_H */
