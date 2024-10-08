//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#ifndef DOS16_INTTYPES_H
#define DOS16_INTTYPES_H

#if !defined(__TURBOC__) && !defined(MSDOS)
#include <inttypes.h>
#else

// A rough and incomplete version of C99's inttypes.h that can be used
// with old DOS 16-bit compilers like Turbo C.

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;

#define INT8_MIN               (-128)
#define INT16_MIN              (-32767-1)
#define INT32_MIN              (-2147483647L-1)
#define INT8_MAX               (127)
#define INT16_MAX              (32767)
#define INT32_MAX              (2147483647L)
#define UINT8_MAX              (255)
#define UINT16_MAX             (65535)
#define UINT32_MAX             (4294967295UL)

#endif

#endif /* #ifndef DOS16_INTTYPES_H */
