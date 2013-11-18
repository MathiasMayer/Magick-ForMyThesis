/*
  Copyright 1999-2014 ImageMagick Studio LLC, a non-profit organization
  dedicated to making software imaging solutions freely available.
  
  You may not use this file except in compliance with the License.
  obtain a copy of the License at
  
    http://www.imagemagick.org/script/license.php
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  MagickCore image resize private methods.
*/
#ifndef _MAGICKCORE_RESIZE_PRIVATE_H
#define _MAGICKCORE_RESIZE_PRIVATE_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct _ResizeFilter
  ResizeFilter;

extern MagickPrivate double
  GetResizeFilterSupport(const ResizeFilter *),
  GetResizeFilterWeight(const ResizeFilter *,const double);

extern MagickPrivate ResizeFilter
  *AcquireResizeFilter(const Image *,const FilterTypes,const MagickBooleanType,
    ExceptionInfo *),
  *DestroyResizeFilter(ResizeFilter *);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
