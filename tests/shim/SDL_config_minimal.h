/* Minimal SDL_config for a host build where SDL2 was never configured:
 * only what the editor needs to compile. Test shim only, not shipped. */
#ifndef SDL_config_minimal_h_
#define SDL_config_minimal_h_

#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_MEMORY_H 1
#define HAVE_MATH_H 1
#define HAVE_CTYPE_H 1
#define HAVE_LIMITS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_ALLOCA_H 1
#define HAVE_ALLOCA 1
#define HAVE_ABS 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
#define HAVE_STRCHR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOD 1
#define HAVE_ATOI 1
#define HAVE_ATOF 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
#define HAVE_SSCANF 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_M_PI 1
#define HAVE_ACOS 1
#define HAVE_ASIN 1
#define HAVE_ATAN 1
#define HAVE_ATAN2 1
#define HAVE_CEIL 1
#define HAVE_COPYSIGN 1
#define HAVE_COS 1
#define HAVE_EXP 1
#define HAVE_FABS 1
#define HAVE_FLOOR 1
#define HAVE_LOG 1
#define HAVE_POW 1
#define HAVE_SIN 1
#define HAVE_SQRT 1
#define HAVE_TAN 1

#endif
