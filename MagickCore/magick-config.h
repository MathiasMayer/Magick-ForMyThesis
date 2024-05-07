/*
  Copyright @ 1999 ImageMagick Studio LLC, a non-profit organization
  dedicated to making software imaging solutions freely available.

  You may not use this file except in compliance with the License.  You may
  obtain a copy of the License at

    https://imagemagick.org/script/license.php

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  MagickConfig not autogenerated (fixed stuff)
*/
#ifndef MAGICKCORE_MAGICK_CONFIG_H
#define MAGICKCORE_MAGICK_CONFIG_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "MagickCore/magick-baseconfig.h"

#define MAGICKCORE_STRING_QUOTE(str) #str
#define MAGICKCORE_STRING_XQUOTE(str) MAGICKCORE_STRING_QUOTE(str)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# if defined(__GNUC__) || defined(__clang__)
#  define MAGICK_COMPILER_WARNING(w) _Pragma(MAGICKCORE_STRING_QUOTE(GCC warning w))
# elif defined(_MSC_VER)
#  define MAGICK_COMPILER_WARNING(w) _Pragma(MAGICKCORE_STRING_QUOTE(message(w)))
# endif
#endif

#ifndef MAGICK_COMPILER_WARNING
# define MAGICK_COMPILER_WARNING(w)
#endif

#ifdef MAGICKCORE__FILE_OFFSET_BITS
# ifdef _FILE_OFFSET_BITS
#  if _FILE_OFFSET_BITS != MAGICKCORE__FILE_OFFSET_BITS
    MAGICK_COMPILER_WARNING("_FILE_OFFSET_BITS is already defined, but does not match MAGICKCORE__FILE_OFFSET_BITS")
#  else
#   undef _FILE_OFFSET_BITS
#  endif
# endif
# ifndef _FILE_OFFSET_BITS
#  if MAGICKCORE__FILE_OFFSET_BITS == 64
#   define _FILE_OFFSET_BITS 64
#  elif MAGICKCORE__FILE_OFFSET_BITS == 32
#   define _FILE_OFFSET_BITS 32
#  else
#   define _FILE_OFFSET_BITS MAGICKCORE__FILE_OFFSET_BITS
#  endif
# endif
#endif

/* Compatibility block */
#if !defined(MAGICKCORE_QUANTUM_DEPTH) && defined(MAGICKCORE_QUANTUM_DEPTH_OBSOLETE_IN_H)
# warning "you should set MAGICKCORE_QUANTUM_DEPTH to sensible default set it to configure time default"
# warning "this is an obsolete behavior please fix your makefile"
# define MAGICKCORE_QUANTUM_DEPTH MAGICKCORE_QUANTUM_DEPTH_OBSOLETE_IN_H
#endif

/* Number of bits in a pixel Quantum (8/16/32/64) */
#ifndef MAGICKCORE_QUANTUM_DEPTH
# error "you should set MAGICKCORE_QUANTUM_DEPTH"
#endif

/* check values */
#if MAGICKCORE_QUANTUM_DEPTH != 8
# if MAGICKCORE_QUANTUM_DEPTH != 16
#  if MAGICKCORE_QUANTUM_DEPTH != 32
#   if MAGICKCORE_QUANTUM_DEPTH != 64
#    error "MAGICKCORE_QUANTUM_DEPTH is not 8/16/32/64 bits"
#   endif
#  endif
# endif
#endif

/* whether HDRI is enable */
#if !defined(MAGICKCORE_HDRI_ENABLE)
# error "you should set MAGICKCORE_HDRI_ENABLE"
#endif

#if MAGICKCORE_HDRI_ENABLE
# define MAGICKCORE_HDRI_SUPPORT 1
#endif
#if MAGICKCORE_CHANNEL_MASK_DEPTH == 64
# define MAGICKCORE_64BIT_CHANNEL_MASK_SUPPORT 1
#endif

/* Compatibility block */
#if !defined(MAGICKCORE_QUANTUM_DEPTH) && defined(MAGICKCORE_QUANTUM_DEPTH_OBSOLETE_IN_H)
# warning "you should set MAGICKCORE_QUANTUM_DEPTH to sensible default set it to configure time default"
# warning "this is an obsolete behavior please fix yours makefile"
# define MAGICKCORE_QUANTUM_DEPTH MAGICKCORE_QUANTUM_DEPTH_OBSOLETE_IN_H
#endif

/* Number of bits in a pixel Quantum (8/16/32/64) */
#ifndef MAGICKCORE_QUANTUM_DEPTH
# error "you should set MAGICKCORE_QUANTUM_DEPTH"
#endif

/* check values */
#if MAGICKCORE_QUANTUM_DEPTH != 8
# if MAGICKCORE_QUANTUM_DEPTH != 16
#  if MAGICKCORE_QUANTUM_DEPTH != 32
#   if MAGICKCORE_QUANTUM_DEPTH != 64
#    error "MAGICKCORE_QUANTUM_DEPTH is not 8/16/32/64 bits"
#   endif
#  endif
# endif
#endif

/* whether HDRI is enable */
#if !defined(MAGICKCORE_HDRI_ENABLE)
# error "you should set MAGICKCORE_HDRI_ENABLE"
#endif

#if MAGICKCORE_HDRI_ENABLE
# define MAGICKCORE_HDRI_SUPPORT 1
#endif

#if defined __CYGWIN32__ && !defined __CYGWIN__
   /* For backwards compatibility with Cygwin b19 and
      earlier, we define __CYGWIN__ here, so that
      we can rely on checking just for that macro. */
#  define __CYGWIN__  __CYGWIN32__
#endif

/*  ABI SUFFIX */
#ifndef MAGICKCORE_HDRI_SUPPORT
#define MAGICKCORE_ABI_SUFFIX  "Q" MAGICKCORE_STRING_XQUOTE(MAGICKCORE_QUANTUM_DEPTH)
#else
#define MAGICKCORE_ABI_SUFFIX "Q" MAGICKCORE_STRING_XQUOTE(MAGICKCORE_QUANTUM_DEPTH) "HDRI"
#endif

/* some path game */
#if !defined __CYGWIN__
# if defined (_WIN32) || defined (_WIN64) || defined (__MSDOS__) || defined (__DJGPP__) || defined (__OS2__)
   /* Use Windows separators on all _WIN32 defining
      environments, except Cygwin. */
#  define MAGICKCORE_DIR_SEPARATOR_CHAR		'\\'
#  define MAGICKCORE_DIR_SEPARATOR		"\\"
#  define MAGICKCORE_PATH_SEPARATOR_CHAR	';'
#  define MAGICKCORE_PATH_SEPARATOR		";"
# endif
#endif

/* posix */
#ifndef MAGICKCORE_DIR_SEPARATOR_CHAR
   /* Assume that not having this is an indicator that all
      are missing. */
#  define MAGICKCORE_DIR_SEPARATOR_CHAR		'/'
#  define MAGICKCORE_DIR_SEPARATOR		"/"
#  define MAGICKCORE_PATH_SEPARATOR_CHAR	':'
#  define MAGICKCORE_PATH_SEPARATOR		":"
#endif /* !DIR_SEPARATOR_CHAR */

# if defined(MAGICKCORE_POSIX_SUPPORT) || defined(__MINGW32__) || defined(MAGICKCORE_POSIX_ON_WINDOWS_SUPPORT)

/* module dir */
#ifndef MAGICKCORE_MODULES_DIRNAME
# define MAGICKCORE_MODULES_DIRNAME MAGICKCORE_MODULES_BASEDIRNAME "-" MAGICKCORE_ABI_SUFFIX
#endif

#ifndef MAGICKCORE_MODULES_PATH
#  define MAGICKCORE_MODULES_PATH MAGICKCORE_LIBRARY_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_MODULES_DIRNAME
#endif

#ifndef MAGICKCORE_MODULES_RELATIVE_PATH
#define MAGICKCORE_MODULES_RELATIVE_PATH MAGICKCORE_LIBRARY_RELATIVE_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_MODULES_DIRNAME
#endif

/* Subdirectory under lib to place ImageMagick coder module files */
#ifndef MAGICKCORE_CODER_PATH
# if defined(vms)
#  define MAGICKCORE_CODER_PATH "sys$login:"
# else
#  define MAGICKCORE_CODER_PATH MAGICKCORE_MODULES_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_CODER_DIRNAME
# endif
#endif

#ifndef MAGICKCORE_CODER_RELATIVE_PATH
# define MAGICKCORE_CODER_RELATIVE_PATH MAGICKCORE_MODULES_RELATIVE_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_CODER_DIRNAME
#endif

/* subdirectory under lib to place ImageMagick filter module files */
#ifndef MAGICKCORE_FILTER_PATH
# if defined(vms)
#  define MAGICKCORE_FILTER_PATH  "sys$login:"
# else
#  define MAGICKCORE_FILTER_PATH MAGICKCORE_MODULES_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_FILTER_DIRNAME
# endif
#endif

#ifndef MAGICKCORE_FILTER_RELATIVE_PATH
# define MAGICKCORE_FILTER_RELATIVE_PATH MAGICKCORE_MODULES_RELATIVE_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_FILTER_DIRNAME
#endif

/* sharearch dir */
#ifndef MAGICKCORE_SHAREARCH_DIRNAME
# define MAGICKCORE_SHAREARCH_DIRNAME MAGICKCORE_SHAREARCH_BASEDIRNAME "-" MAGICKCORE_ABI_SUFFIX
#endif

#ifndef MAGICKCORE_SHAREARCH_PATH
#  define MAGICKCORE_SHAREARCH_PATH MAGICKCORE_LIBRARY_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_SHAREARCH_DIRNAME MAGICKCORE_DIR_SEPARATOR
#endif

#ifndef MAGICKCORE_SHAREARCH_RELATIVE_PATH
#define MAGICKCORE_SHAREARCH_RELATIVE_PATH MAGICKCORE_LIBRARY_RELATIVE_PATH MAGICKCORE_DIR_SEPARATOR MAGICKCORE_SHAREARCH_DIRNAME
#endif

#endif

/* for Clang compatibility */
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
# define MAGICKCORE_DIAGNOSTIC_PUSH() \
   _Pragma("GCC diagnostic push")
# define MAGICKCORE_DIAGNOSTIC_IGNORE_MAYBE_UNINITIALIZED() \
   _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define MAGICKCORE_DIAGNOSTIC_POP() \
   _Pragma("GCC diagnostic pop")
#else
# define MAGICKCORE_DIAGNOSTIC_PUSH()
# define MAGICKCORE_DIAGNOSTIC_IGNORE_MAYBE_UNINITIALIZED()
# define MAGICKCORE_DIAGNOSTIC_POP()
#endif

#define MAGICKCORE_BITS_BELOW(power_of_2) \
  ((power_of_2)-1)

#define MAGICKCORE_MAX_ALIGNMENT_PADDING(power_of_2) \
  MAGICKCORE_BITS_BELOW(power_of_2)

#define MAGICKCORE_IS_NOT_ALIGNED(n, power_of_2) \
  ((n) & MAGICKCORE_BITS_BELOW(power_of_2))

#define MAGICKCORE_IS_NOT_POWER_OF_2(n) \
  MAGICKCORE_IS_NOT_ALIGNED((n), (n))

#define MAGICKCORE_ALIGN_DOWN(n, power_of_2) \
  ((n) & ~MAGICKCORE_BITS_BELOW(power_of_2))

#define MAGICKCORE_ALIGN_UP(n, power_of_2) \
  MAGICKCORE_ALIGN_DOWN((n) + MAGICKCORE_MAX_ALIGNMENT_PADDING(power_of_2),power_of_2)

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
