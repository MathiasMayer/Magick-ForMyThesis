/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%     CCCC   OOO   L       OOO   RRRR   SSSSS  PPPP    AAA    CCCC  EEEEE     %
%    C      O   O  L      O   O  R   R  SS     P   P  A   A  C      E         %
%    C      O   O  L      O   O  RRRR    SSS   PPPP   AAAAA  C      EEE       %
%    C      O   O  L      O   O  R R       SS  P      A   A  C      E         %
%     CCCC   OOO   LLLLL   OOO   R  R   SSSSS  P      A   A   CCCC  EEEEE     %
%                                                                             %
%                                                                             %
%                     MagickCore Image Colorspace Methods                     %
%                                                                             %
%                              Software Design                                %
%                                John Cristy                                  %
%                                 July 1992                                   %
%                                                                             %
%                                                                             %
%  Copyright 1999-2012 ImageMagick Studio LLC, a non-profit organization      %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    http://www.imagemagick.org/script/license.php                            %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "magick/studio.h"
#include "magick/property.h"
#include "magick/cache.h"
#include "magick/cache-private.h"
#include "magick/cache-view.h"
#include "magick/color.h"
#include "magick/color-private.h"
#include "magick/colorspace.h"
#include "magick/colorspace-private.h"
#include "magick/exception.h"
#include "magick/exception-private.h"
#include "magick/image.h"
#include "magick/image-private.h"
#include "magick/gem.h"
#include "magick/memory_.h"
#include "magick/monitor.h"
#include "magick/monitor-private.h"
#include "magick/pixel-private.h"
#include "magick/quantize.h"
#include "magick/quantum.h"
#include "magick/resource_.h"
#include "magick/string_.h"
#include "magick/string-private.h"
#include "magick/utility.h"

/*
  Typedef declarations.
*/
typedef struct _TransformPacket
{
  MagickRealType
    x,
    y,
    z;
} TransformPacket;

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
+     R G B T r a n s f o r m I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RGBTransformImage() converts the reference image from sRGB to an alternate
%  colorspace.  The transformation matrices are not the standard ones: the
%  weights are rescaled to normalized the range of the transformed values to
%  be [0..QuantumRange].
%
%  The format of the RGBTransformImage method is:
%
%      MagickBooleanType RGBTransformImage(Image *image,
%        const ColorspaceType colorspace)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o colorspace: the colorspace to transform the image to.
%
*/

static inline void ConvertRGBToXYZ(const Quantum red,const Quantum green,
  const Quantum blue,double *X,double *Y,double *Z)
{
  double
    b,
    g,
    r;

  assert(X != (double *) NULL);
  assert(Y != (double *) NULL);
  assert(Z != (double *) NULL);
  r=QuantumScale*red;
  if (r > 0.0404482362771082)
    r=pow((r+0.055)/1.055,2.4);
  else
    r/=12.92;
  g=QuantumScale*green;
  if (g > 0.0404482362771082)
    g=pow((g+0.055)/1.055,2.4);
  else
    g/=12.92;
  b=QuantumScale*blue;
  if (b > 0.0404482362771082)
    b=pow((b+0.055)/1.055,2.4);
  else
    b/=12.92;
  *X=0.4124240*r+0.3575790*g+0.1804640*b;
  *Y=0.2126560*r+0.7151580*g+0.0721856*b;
  *Z=0.0193324*r+0.1191930*g+0.9504440*b;
}

static double LabF1(double alpha)
{

  if (alpha <= ((24.0/116.0)*(24.0/116.0)*(24.0/116.0)))
    return((841.0/108.0)*alpha+(16.0/116.0));
  return(pow(alpha,1.0/3.0));
}

static inline void ConvertXYZToLab(const double X,const double Y,const double Z,
  double *L,double *a,double *b)
{
#define D50X  (0.9642)
#define D50Y  (1.0)
#define D50Z  (0.8249)

  double
    fx,
    fy,
    fz;

  assert(L != (double *) NULL);
  assert(a != (double *) NULL);
  assert(b != (double *) NULL);
  *L=0.0;
  *a=0.5;
  *b=0.5;
  if ((fabs(X) < MagickEpsilon) && (fabs(Y) < MagickEpsilon) &&
      (fabs(Z) < MagickEpsilon))
    return;
  fx=LabF1(X/D50X);
  fy=LabF1(Y/D50Y);
  fz=LabF1(Z/D50Z);
  *L=(116.0*fy-16.0)/100.0;
  *a=(500.0*(fx-fy))/255.0;
  if (*a < 0.0)
    *a+=1.0;
  *b=(200.0*(fy-fz))/255.0;
  if (*b < 0.0)
    *b+=1.0;
}

MagickExport MagickBooleanType RGBTransformImage(Image *image,
  const ColorspaceType colorspace)
{
#define RGBTransformImageTag  "RGBTransform/Image"

  CacheView
    *image_view;

  ExceptionInfo
    *exception;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  PrimaryInfo
    primary_info;

  register ssize_t
    i;

  ssize_t
    y;

  TransformPacket
    *x_map,
    *y_map,
    *z_map;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(colorspace != sRGBColorspace);
  assert(colorspace != TransparentColorspace);
  assert(colorspace != UndefinedColorspace);
  if (IsGrayColorspace(colorspace) != MagickFalse)
    (void) SetImageColorspace(image,sRGBColorspace);
  else
    if (SetImageColorspace(image,colorspace) == MagickFalse)
      return(MagickFalse);
  status=MagickTrue;
  progress=0;
  exception=(&image->exception);
  switch (colorspace)
  {
    case CMYColorspace:
    {
      /*
        Convert RGB to CMY colorspace.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelRed(q))));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelGreen(q))));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelBlue(q))));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->type=image->matte == MagickFalse ? ColorSeparationType :
        ColorSeparationMatteType;
      return(status);
    }
    case CMYKColorspace:
    {
      MagickPixelPacket
        zero;

      /*
        Convert RGB to CMYK colorspace.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      GetMagickPixelPacket(image,&zero);
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        MagickPixelPacket
          pixel;

        register IndexPacket
          *restrict indexes;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        indexes=GetCacheViewAuthenticIndexQueue(image_view);
        pixel=zero;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          SetMagickPixelPacket(image,q,indexes+x,&pixel);
          ConvertRGBToCMYK(&pixel);
          SetPixelPacket(image,&pixel,q,indexes+x);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->type=image->matte == MagickFalse ? ColorSeparationType :
        ColorSeparationMatteType;
      return(status);
    }
    case HSBColorspace:
    {
      /*
        Transform image from RGB to HSB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          brightness,
          hue,
          saturation;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        hue=0.0;
        saturation=0.0;
        brightness=0.0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          ConvertRGBToHSB(GetPixelRed(q),GetPixelGreen(q),
            GetPixelBlue(q),&hue,&saturation,&brightness);
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            QuantumRange*hue));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            QuantumRange*saturation));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            QuantumRange*brightness));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      return(status);
    }
    case HSLColorspace:
    {
      /*
        Transform image from RGB to HSL.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          hue,
          lightness,
          saturation;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        hue=0.0;
        saturation=0.0;
        lightness=0.0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          ConvertRGBToHSL(GetPixelRed(q),GetPixelGreen(q),
            GetPixelBlue(q),&hue,&saturation,&lightness);
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            QuantumRange*hue));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            QuantumRange*saturation));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            QuantumRange*lightness));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      return(status);
    }
    case HWBColorspace:
    {
      /*
        Transform image from RGB to HWB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          blackness,
          hue,
          whiteness;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        hue=0.0;
        whiteness=0.0;
        blackness=0.0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          ConvertRGBToHWB(GetPixelRed(q),GetPixelGreen(q),
            GetPixelBlue(q),&hue,&whiteness,&blackness);
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            QuantumRange*hue));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            QuantumRange*whiteness));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            QuantumRange*blackness));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      return(status);
    }
    case LabColorspace:
    {
      /*
        Transform image from RGB to Lab.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          a,
          b,
          L,
          X,
          Y,
          Z;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        L=0.0;
        a=0.0;
        b=0.0;
        X=0.0;
        Y=0.0;
        Z=0.0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          ConvertRGBToXYZ(GetPixelRed(q),GetPixelGreen(q),
            GetPixelBlue(q),&X,&Y,&Z);
          ConvertXYZToLab(X,Y,Z,&L,&a,&b);
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            QuantumRange*L));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            QuantumRange*a));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            QuantumRange*b));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      return(status);
    }
    case LogColorspace:
    {
#define DisplayGamma  (1.0/1.7)
#define FilmGamma  0.6
#define ReferenceBlack  95.0
#define ReferenceWhite  685.0

      const char
        *value;

      double
        black,
        density,
        film_gamma,
        gamma,
        reference_black,
        reference_white;

      Quantum
        *logmap;

      /*
        Transform RGB to Log colorspace.
      */
      density=DisplayGamma;
      gamma=DisplayGamma;
      value=GetImageProperty(image,"gamma");
      if (value != (const char *) NULL)
        gamma=1.0/StringToDouble(value,(char **) NULL) != 0.0 ? StringToDouble(
          value,(char **) NULL) : 1.0;
      film_gamma=FilmGamma;
      value=GetImageProperty(image,"film-gamma");
      if (value != (const char *) NULL)
        film_gamma=StringToDouble(value,(char **) NULL);
      reference_black=ReferenceBlack;
      value=GetImageProperty(image,"reference-black");
      if (value != (const char *) NULL)
        reference_black=StringToDouble(value,(char **) NULL);
      reference_white=ReferenceWhite;
      value=GetImageProperty(image,"reference-white");
      if (value != (const char *) NULL)
        reference_white=StringToDouble(value,(char **) NULL);
      logmap=(Quantum *) AcquireQuantumMemory((size_t) MaxMap+1UL,
        sizeof(*logmap));
      if (logmap == (Quantum *) NULL)
        ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
          image->filename);
      black=pow(10.0,(reference_black-reference_white)*(gamma/density)*
        0.002/film_gamma);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
        logmap[i]=ScaleMapToQuantum((MagickRealType) (MaxMap*(reference_white+
          log10(black+((MagickRealType) i/MaxMap)*(1.0-black))/((gamma/density)*
          0.002/film_gamma))/1024.0));
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=(ssize_t) image->columns; x != 0; x--)
        {
          SetPixelRed(q,logmap[ScaleQuantumToMap(
            GetPixelRed(q))]);
          SetPixelGreen(q,logmap[ScaleQuantumToMap(
            GetPixelGreen(q))]);
          SetPixelBlue(q,logmap[ScaleQuantumToMap(
            GetPixelBlue(q))]);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      logmap=(Quantum *) RelinquishMagickMemory(logmap);
      return(status);
    }
    default:
      break;
  }
  /*
    Allocate the tables.
  */
  x_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*x_map));
  y_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*y_map));
  z_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*z_map));
  if ((x_map == (TransformPacket *) NULL) ||
      (y_map == (TransformPacket *) NULL) ||
      (z_map == (TransformPacket *) NULL))
    ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
      image->filename);
  (void) ResetMagickMemory(&primary_info,0,sizeof(primary_info));
  switch (colorspace)
  {
    case OHTAColorspace:
    {
      /*
        Initialize OHTA tables:

          I1 = 0.33333*R+0.33334*G+0.33333*B
          I2 = 0.50000*R+0.00000*G-0.50000*B
          I3 =-0.25000*R+0.50000*G-0.25000*B

        I and Q, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.33333f*(MagickRealType) i;
        y_map[i].x=0.33334f*(MagickRealType) i;
        z_map[i].x=0.33333f*(MagickRealType) i;
        x_map[i].y=0.50000f*(MagickRealType) i;
        y_map[i].y=0.00000f*(MagickRealType) i;
        z_map[i].y=(-0.50000f)*(MagickRealType) i;
        x_map[i].z=(-0.25000f)*(MagickRealType) i;
        y_map[i].z=0.50000f*(MagickRealType) i;
        z_map[i].z=(-0.25000f)*(MagickRealType) i;
      }
      break;
    }
    case Rec601LumaColorspace:
    case GRAYColorspace:
    {
      /*
        Initialize Rec601 luma tables:

          G = 0.29900*R+0.58700*G+0.11400*B
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.29900f*(MagickRealType) i;
        y_map[i].x=0.58700f*(MagickRealType) i;
        z_map[i].x=0.11400f*(MagickRealType) i;
        x_map[i].y=0.29900f*(MagickRealType) i;
        y_map[i].y=0.58700f*(MagickRealType) i;
        z_map[i].y=0.11400f*(MagickRealType) i;
        x_map[i].z=0.29900f*(MagickRealType) i;
        y_map[i].z=0.58700f*(MagickRealType) i;
        z_map[i].z=0.11400f*(MagickRealType) i;
      }
      image->type=GrayscaleType;
      break;
    }
    case Rec601YCbCrColorspace:
    case YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables (ITU-R BT.601):

          Y =  0.299000*R+0.587000*G+0.114000*B
          Cb= -0.168736*R-0.331264*G+0.500000*B
          Cr=  0.500000*R-0.418688*G-0.081312*B

        Cb and Cr, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.299000f*(MagickRealType) i;
        y_map[i].x=0.587000f*(MagickRealType) i;
        z_map[i].x=0.114000f*(MagickRealType) i;
        x_map[i].y=(-0.168730f)*(MagickRealType) i;
        y_map[i].y=(-0.331264f)*(MagickRealType) i;
        z_map[i].y=0.500000f*(MagickRealType) i;
        x_map[i].z=0.500000f*(MagickRealType) i;
        y_map[i].z=(-0.418688f)*(MagickRealType) i;
        z_map[i].z=(-0.081312f)*(MagickRealType) i;
      }
      break;
    }
    case Rec709LumaColorspace:
    {
      /*
        Initialize Rec709 luma tables:

          G = 0.21260*R+0.71520*G+0.07220*B
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.21260f*(MagickRealType) i;
        y_map[i].x=0.71520f*(MagickRealType) i;
        z_map[i].x=0.07220f*(MagickRealType) i;
        x_map[i].y=0.21260f*(MagickRealType) i;
        y_map[i].y=0.71520f*(MagickRealType) i;
        z_map[i].y=0.07220f*(MagickRealType) i;
        x_map[i].z=0.21260f*(MagickRealType) i;
        y_map[i].z=0.71520f*(MagickRealType) i;
        z_map[i].z=0.07220f*(MagickRealType) i;
      }
      break;
    }
    case Rec709YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables (ITU-R BT.709):

          Y =  0.212600*R+0.715200*G+0.072200*B
          Cb= -0.114572*R-0.385428*G+0.500000*B
          Cr=  0.500000*R-0.454153*G-0.045847*B

        Cb and Cr, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.212600f*(MagickRealType) i;
        y_map[i].x=0.715200f*(MagickRealType) i;
        z_map[i].x=0.072200f*(MagickRealType) i;
        x_map[i].y=(-0.114572f)*(MagickRealType) i;
        y_map[i].y=(-0.385428f)*(MagickRealType) i;
        z_map[i].y=0.500000f*(MagickRealType) i;
        x_map[i].z=0.500000f*(MagickRealType) i;
        y_map[i].z=(-0.454153f)*(MagickRealType) i;
        z_map[i].z=(-0.045847f)*(MagickRealType) i;
      }
      break;
    }
    case RGBColorspace:
    {
      /*
        Nonlinear sRGB to linear RGB.
        Mostly removal of a gamma function, but with a linear component
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        MagickRealType
          v;

        v=(MagickRealType) i/(MagickRealType) MaxMap;
        if (((MagickRealType) i/(MagickRealType) MaxMap) <= 0.0404482362771082f)
          v/=12.92f;
        else
          v=(MagickRealType) pow((((double) i/MaxMap)+0.055)/1.055,2.4);
        x_map[i].x=1.0f*MaxMap*v;
        y_map[i].x=0.0f*MaxMap*v;
        z_map[i].x=0.0f*MaxMap*v;
        x_map[i].y=0.0f*MaxMap*v;
        y_map[i].y=1.0f*MaxMap*v;
        z_map[i].y=0.0f*MaxMap*v;
        x_map[i].z=0.0f*MaxMap*v;
        y_map[i].z=0.0f*MaxMap*v;
        z_map[i].z=1.0f*MaxMap*v;
      }
      break;
    }
    case XYZColorspace:
    {
      /*
        Initialize CIE XYZ tables (ITU-R 709 RGB):

          X = 0.4124564*R+0.3575761*G+0.1804375*B
          Y = 0.2126729*R+0.7151522*G+0.0721750*B
          Z = 0.0193339*R+0.1191920*G+0.9503041*B
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.4124564f*(MagickRealType) i;
        y_map[i].x=0.3575761f*(MagickRealType) i;
        z_map[i].x=0.1804375f*(MagickRealType) i;
        x_map[i].y=0.2126729f*(MagickRealType) i;
        y_map[i].y=0.7151522f*(MagickRealType) i;
        z_map[i].y=0.0721750f*(MagickRealType) i;
        x_map[i].z=0.0193339f*(MagickRealType) i;
        y_map[i].z=0.1191920f*(MagickRealType) i;
        z_map[i].z=0.9503041f*(MagickRealType) i;
      }
      break;
    }
    case YCCColorspace:
    {
      /*
        Initialize YCC tables:

          Y =  0.29900*R+0.58700*G+0.11400*B
          C1= -0.29900*R-0.58700*G+0.88600*B
          C2=  0.70100*R-0.58700*G-0.11400*B

        YCC is scaled by 1.3584.  C1 zero is 156 and C2 is at 137.
      */
      primary_info.y=(double) ScaleQuantumToMap(ScaleCharToQuantum(156));
      primary_info.z=(double) ScaleQuantumToMap(ScaleCharToQuantum(137));
      for (i=0; i <= (ssize_t) (0.018*MaxMap); i++)
      {
        x_map[i].x=0.003962014134275617f*(MagickRealType) i;
        y_map[i].x=0.007778268551236748f*(MagickRealType) i;
        z_map[i].x=0.001510600706713781f*(MagickRealType) i;
        x_map[i].y=(-0.002426619775463276f)*(MagickRealType) i;
        y_map[i].y=(-0.004763965913702149f)*(MagickRealType) i;
        z_map[i].y=0.007190585689165425f*(MagickRealType) i;
        x_map[i].z=0.006927257754597858f*(MagickRealType) i;
        y_map[i].z=(-0.005800713697502058f)*(MagickRealType) i;
        z_map[i].z=(-0.0011265440570958f)*(MagickRealType) i;
      }
      for ( ; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.2201118963486454*(1.099f*(MagickRealType) i-0.099f);
        y_map[i].x=0.4321260306242638*(1.099f*(MagickRealType) i-0.099f);
        z_map[i].x=0.08392226148409894*(1.099f*(MagickRealType) i-0.099f);
        x_map[i].y=(-0.1348122097479598)*(1.099f*(MagickRealType) i-0.099f);
        y_map[i].y=(-0.2646647729834528)*(1.099f*(MagickRealType) i-0.099f);
        z_map[i].y=0.3994769827314126*(1.099f*(MagickRealType) i-0.099f);
        x_map[i].z=0.3848476530332144*(1.099f*(MagickRealType) i-0.099f);
        y_map[i].z=(-0.3222618720834477)*(1.099f*(MagickRealType) i-0.099f);
        z_map[i].z=(-0.06258578094976668)*(1.099f*(MagickRealType) i-0.099f);
      }
      break;
    }
    case YIQColorspace:
    {
      /*
        Initialize YIQ tables:

          Y = 0.29900*R+0.58700*G+0.11400*B
          I = 0.59600*R-0.27400*G-0.32200*B
          Q = 0.21100*R-0.52300*G+0.31200*B

        I and Q, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.29900f*(MagickRealType) i;
        y_map[i].x=0.58700f*(MagickRealType) i;
        z_map[i].x=0.11400f*(MagickRealType) i;
        x_map[i].y=0.59600f*(MagickRealType) i;
        y_map[i].y=(-0.27400f)*(MagickRealType) i;
        z_map[i].y=(-0.32200f)*(MagickRealType) i;
        x_map[i].z=0.21100f*(MagickRealType) i;
        y_map[i].z=(-0.52300f)*(MagickRealType) i;
        z_map[i].z=0.31200f*(MagickRealType) i;
      }
      break;
    }
    case YPbPrColorspace:
    {
      /*
        Initialize YPbPr tables (ITU-R BT.601):

          Y =  0.299000*R+0.587000*G+0.114000*B
          Pb= -0.168736*R-0.331264*G+0.500000*B
          Pr=  0.500000*R-0.418688*G-0.081312*B

        Pb and Pr, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.299000f*(MagickRealType) i;
        y_map[i].x=0.587000f*(MagickRealType) i;
        z_map[i].x=0.114000f*(MagickRealType) i;
        x_map[i].y=(-0.168736f)*(MagickRealType) i;
        y_map[i].y=(-0.331264f)*(MagickRealType) i;
        z_map[i].y=0.500000f*(MagickRealType) i;
        x_map[i].z=0.500000f*(MagickRealType) i;
        y_map[i].z=(-0.418688f)*(MagickRealType) i;
        z_map[i].z=(-0.081312f)*(MagickRealType) i;
      }
      break;
    }
    case YUVColorspace:
    {
      /*
        Initialize YUV tables:

          Y =  0.29900*R+0.58700*G+0.11400*B
          U = -0.14740*R-0.28950*G+0.43690*B
          V =  0.61500*R-0.51500*G-0.10000*B

        U and V, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.  Note that U = 0.493*(B-Y), V = 0.877*(R-Y).
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.29900f*(MagickRealType) i;
        y_map[i].x=0.58700f*(MagickRealType) i;
        z_map[i].x=0.11400f*(MagickRealType) i;
        x_map[i].y=(-0.14740f)*(MagickRealType) i;
        y_map[i].y=(-0.28950f)*(MagickRealType) i;
        z_map[i].y=0.43690f*(MagickRealType) i;
        x_map[i].z=0.61500f*(MagickRealType) i;
        y_map[i].z=(-0.51500f)*(MagickRealType) i;
        z_map[i].z=(-0.10000f)*(MagickRealType) i;
      }
      break;
    }
    default:
    {
      /*
        Linear conversion tables.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.0f;
        z_map[i].x=0.0f;
        x_map[i].y=0.0f;
        y_map[i].y=(MagickRealType) i;
        z_map[i].y=0.0f;
        x_map[i].z=0.0f;
        y_map[i].z=0.0f;
        z_map[i].z=(MagickRealType) i;
      }
      break;
    }
  }
  /*
    Convert from RGB.
  */
  switch (image->storage_class)
  {
    case DirectClass:
    default:
    {
      /*
        Convert DirectClass image.
      */
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        MagickPixelPacket
          pixel;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        register size_t
          blue,
          green,
          red;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          red=ScaleQuantumToMap(GetPixelRed(q));
          green=ScaleQuantumToMap(GetPixelGreen(q));
          blue=ScaleQuantumToMap(GetPixelBlue(q));
          pixel.red=(x_map[red].x+y_map[green].x+z_map[blue].x)+
            (MagickRealType) primary_info.x;
          pixel.green=(x_map[red].y+y_map[green].y+z_map[blue].y)+
            (MagickRealType) primary_info.y;
          pixel.blue=(x_map[red].z+y_map[green].z+z_map[blue].z)+
            (MagickRealType) primary_info.z;
          SetPixelRed(q,ScaleMapToQuantum(pixel.red));
          SetPixelGreen(q,ScaleMapToQuantum(pixel.green));
          SetPixelBlue(q,ScaleMapToQuantum(pixel.blue));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
        if (image->progress_monitor != (MagickProgressMonitor) NULL)
          {
            MagickBooleanType
              proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
            #pragma omp critical (MagickCore_RGBTransformImage)
#endif
            proceed=SetImageProgress(image,RGBTransformImageTag,progress++,
              image->rows);
            if (proceed == MagickFalse)
              status=MagickFalse;
          }
      }
      image_view=DestroyCacheView(image_view);
      break;
    }
    case PseudoClass:
    {
      register size_t
        blue,
        green,
        red;

      /*
        Convert PseudoClass image.
      */
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        MagickPixelPacket
          pixel;

        red=ScaleQuantumToMap(image->colormap[i].red);
        green=ScaleQuantumToMap(image->colormap[i].green);
        blue=ScaleQuantumToMap(image->colormap[i].blue);
        pixel.red=x_map[red].x+y_map[green].x+z_map[blue].x+primary_info.x;
        pixel.green=x_map[red].y+y_map[green].y+z_map[blue].y+primary_info.y;
        pixel.blue=x_map[red].z+y_map[green].z+z_map[blue].z+primary_info.z;
        image->colormap[i].red=ScaleMapToQuantum(pixel.red);
        image->colormap[i].green=ScaleMapToQuantum(pixel.green);
        image->colormap[i].blue=ScaleMapToQuantum(pixel.blue);
      }
      (void) SyncImage(image);
      break;
    }
  }
  /*
    Relinquish resources.
  */
  z_map=(TransformPacket *) RelinquishMagickMemory(z_map);
  y_map=(TransformPacket *) RelinquishMagickMemory(y_map);
  x_map=(TransformPacket *) RelinquishMagickMemory(x_map);
  if (SetImageColorspace(image,colorspace) == MagickFalse)
    return(MagickFalse);
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   S e t I m a g e C o l o r s p a c e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  SetImageColorspace() sets the colorspace member of the Image structure.
%
%  The format of the SetImageColorspace method is:
%
%      MagickBooleanType SetImageColorspace(Image *image,
%        const ColorspaceType colorspace)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o colorspace: the colorspace.
%
*/
MagickExport MagickBooleanType SetImageColorspace(Image *image,
  const ColorspaceType colorspace)
{
  image->colorspace=colorspace;
  return(SyncImagePixelCache(image,&image->exception));
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   T r a n s f o r m I m a g e C o l o r s p a c e                           %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  TransformImageColorspace() transforms an image colorspace.
%
%  The format of the TransformImageColorspace method is:
%
%      MagickBooleanType TransformImageColorspace(Image *image,
%        const ColorspaceType colorspace)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o colorspace: the colorspace.
%
*/
MagickExport MagickBooleanType TransformImageColorspace(Image *image,
  const ColorspaceType colorspace)
{
  MagickBooleanType
    status;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (colorspace == UndefinedColorspace)
    return(SetImageColorspace(image,colorspace));
  if (image->colorspace == colorspace)
    return(MagickTrue);  /* same colorspace: no op */
  /*
    Convert the reference image from an alternate colorspace to sRGB.
  */
  if ((colorspace == sRGBColorspace) || (colorspace == TransparentColorspace))
    return(TransformRGBImage(image,colorspace));
  status=MagickTrue;
  if (image->colorspace == RGBColorspace)
    status=TransformRGBImage(image,sRGBColorspace);
  if (status == MagickFalse)
    return(status);
  if (IssRGBColorspace(image->colorspace) == MagickFalse)
    status=TransformRGBImage(image,image->colorspace);
  if (status == MagickFalse)
    return(status);
  /*
    Convert the reference image from sRGB to an alternate colorspace.
  */
  if (RGBTransformImage(image,colorspace) == MagickFalse)
    status=MagickFalse;
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
+     T r a n s f o r m R G B I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  TransformRGBImage() converts the reference image from an alternate
%  colorspace to sRGB.  The transformation matrices are not the standard ones:
%  the weights are rescaled to normalize the range of the transformed values to
%  be [0..QuantumRange].
%
%  The format of the TransformRGBImage method is:
%
%      MagickBooleanType TransformRGBImage(Image *image,
%        const ColorspaceType colorspace)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o colorspace: the colorspace to transform the image to.
%
*/

static double LabF2(double alpha)
{
  double
    beta;

  if (alpha > (24.0/116.0))
    return(alpha*alpha*alpha);
  beta=(108.0/841.0)*(alpha-(16.0/116.0));
  if (beta > 0.0)
    return(beta);
  return(0.0);
}

static inline void ConvertLabToXYZ(const double L,const double a,const double b,
  double *X,double *Y,double *Z)
{

  double
    x,
    y,
    z;

  assert(X != (double *) NULL);
  assert(Y != (double *) NULL);
  assert(Z != (double *) NULL);
  *X=0.0;
  *Y=0.0;
  *Z=0.0;
  if (L <= 0.0)
    return;
  y=(100.0*L+16.0)/116.0;
  x=y+255.0*0.002*(a > 0.5 ? a-1.0 : a);
  z=y-255.0*0.005*(b > 0.5 ? b-1.0 : b);
  *X=D50X*LabF2(x);
  *Y=D50Y*LabF2(y);
  *Z=D50Z*LabF2(z);
}

static inline ssize_t RoundToYCC(const MagickRealType value)
{
  if (value <= 0.0)
    return(0);
  if (value >= 1388.0)
    return(1388);
  return((ssize_t) (value+0.5));
}

static inline void ConvertXYZToRGB(const double x,const double y,const double z,
  Quantum *red,Quantum *green,Quantum *blue)
{
  double
    b,
    g,
    r;

  /*
    Convert XYZ to RGB colorspace.
  */
  assert(red != (Quantum *) NULL);
  assert(green != (Quantum *) NULL);
  assert(blue != (Quantum *) NULL);
  r=3.2404542*x-1.5371385*y-0.4985314*z;
  g=(-0.9692660*x+1.8760108*y+0.0415560*z);
  b=0.0556434*x-0.2040259*y+1.0572252*z;
  if (r > 0.00313066844250063)
    r=1.055*pow(r,1.0/2.4)-0.055;
  else
    r*=12.92;
  if (g > 0.00313066844250063)
    g=1.055*pow(g,1.0/2.4)-0.055;
  else
    g*=12.92;
  if (b > 0.00313066844250063)
    b=1.055*pow(b,1.0/2.4)-0.055;
  else
    b*=12.92;
  *red=RoundToQuantum((MagickRealType) QuantumRange*r);
  *green=RoundToQuantum((MagickRealType) QuantumRange*g);
  *blue=RoundToQuantum((MagickRealType) QuantumRange*b);
}

static inline void ConvertCMYKToRGB(MagickPixelPacket *pixel)
{
  pixel->red=(MagickRealType) QuantumRange-(QuantumScale*pixel->red*
    (QuantumRange-pixel->index)+pixel->index);
  pixel->green=(MagickRealType) QuantumRange-(QuantumScale*pixel->green*
    (QuantumRange-pixel->index)+pixel->index);
  pixel->blue=(MagickRealType) QuantumRange-(QuantumScale*pixel->blue*
    (QuantumRange-pixel->index)+pixel->index);
}

MagickExport MagickBooleanType TransformRGBImage(Image *image,
  const ColorspaceType colorspace)
{
#define D50X  (0.9642)
#define D50Y  (1.0)
#define D50Z  (0.8249)
#define TransformRGBImageTag  "Transform/Image"

#if !defined(MAGICKCORE_HDRI_SUPPORT)
  static const float
    YCCMap[1389] =
    {
      0.000000f, 0.000720f, 0.001441f, 0.002161f, 0.002882f, 0.003602f,
      0.004323f, 0.005043f, 0.005764f, 0.006484f, 0.007205f, 0.007925f,
      0.008646f, 0.009366f, 0.010086f, 0.010807f, 0.011527f, 0.012248f,
      0.012968f, 0.013689f, 0.014409f, 0.015130f, 0.015850f, 0.016571f,
      0.017291f, 0.018012f, 0.018732f, 0.019452f, 0.020173f, 0.020893f,
      0.021614f, 0.022334f, 0.023055f, 0.023775f, 0.024496f, 0.025216f,
      0.025937f, 0.026657f, 0.027378f, 0.028098f, 0.028818f, 0.029539f,
      0.030259f, 0.030980f, 0.031700f, 0.032421f, 0.033141f, 0.033862f,
      0.034582f, 0.035303f, 0.036023f, 0.036744f, 0.037464f, 0.038184f,
      0.038905f, 0.039625f, 0.040346f, 0.041066f, 0.041787f, 0.042507f,
      0.043228f, 0.043948f, 0.044669f, 0.045389f, 0.046110f, 0.046830f,
      0.047550f, 0.048271f, 0.048991f, 0.049712f, 0.050432f, 0.051153f,
      0.051873f, 0.052594f, 0.053314f, 0.054035f, 0.054755f, 0.055476f,
      0.056196f, 0.056916f, 0.057637f, 0.058357f, 0.059078f, 0.059798f,
      0.060519f, 0.061239f, 0.061960f, 0.062680f, 0.063401f, 0.064121f,
      0.064842f, 0.065562f, 0.066282f, 0.067003f, 0.067723f, 0.068444f,
      0.069164f, 0.069885f, 0.070605f, 0.071326f, 0.072046f, 0.072767f,
      0.073487f, 0.074207f, 0.074928f, 0.075648f, 0.076369f, 0.077089f,
      0.077810f, 0.078530f, 0.079251f, 0.079971f, 0.080692f, 0.081412f,
      0.082133f, 0.082853f, 0.083573f, 0.084294f, 0.085014f, 0.085735f,
      0.086455f, 0.087176f, 0.087896f, 0.088617f, 0.089337f, 0.090058f,
      0.090778f, 0.091499f, 0.092219f, 0.092939f, 0.093660f, 0.094380f,
      0.095101f, 0.095821f, 0.096542f, 0.097262f, 0.097983f, 0.098703f,
      0.099424f, 0.100144f, 0.100865f, 0.101585f, 0.102305f, 0.103026f,
      0.103746f, 0.104467f, 0.105187f, 0.105908f, 0.106628f, 0.107349f,
      0.108069f, 0.108790f, 0.109510f, 0.110231f, 0.110951f, 0.111671f,
      0.112392f, 0.113112f, 0.113833f, 0.114553f, 0.115274f, 0.115994f,
      0.116715f, 0.117435f, 0.118156f, 0.118876f, 0.119597f, 0.120317f,
      0.121037f, 0.121758f, 0.122478f, 0.123199f, 0.123919f, 0.124640f,
      0.125360f, 0.126081f, 0.126801f, 0.127522f, 0.128242f, 0.128963f,
      0.129683f, 0.130403f, 0.131124f, 0.131844f, 0.132565f, 0.133285f,
      0.134006f, 0.134726f, 0.135447f, 0.136167f, 0.136888f, 0.137608f,
      0.138329f, 0.139049f, 0.139769f, 0.140490f, 0.141210f, 0.141931f,
      0.142651f, 0.143372f, 0.144092f, 0.144813f, 0.145533f, 0.146254f,
      0.146974f, 0.147695f, 0.148415f, 0.149135f, 0.149856f, 0.150576f,
      0.151297f, 0.152017f, 0.152738f, 0.153458f, 0.154179f, 0.154899f,
      0.155620f, 0.156340f, 0.157061f, 0.157781f, 0.158501f, 0.159222f,
      0.159942f, 0.160663f, 0.161383f, 0.162104f, 0.162824f, 0.163545f,
      0.164265f, 0.164986f, 0.165706f, 0.166427f, 0.167147f, 0.167867f,
      0.168588f, 0.169308f, 0.170029f, 0.170749f, 0.171470f, 0.172190f,
      0.172911f, 0.173631f, 0.174352f, 0.175072f, 0.175793f, 0.176513f,
      0.177233f, 0.177954f, 0.178674f, 0.179395f, 0.180115f, 0.180836f,
      0.181556f, 0.182277f, 0.182997f, 0.183718f, 0.184438f, 0.185159f,
      0.185879f, 0.186599f, 0.187320f, 0.188040f, 0.188761f, 0.189481f,
      0.190202f, 0.190922f, 0.191643f, 0.192363f, 0.193084f, 0.193804f,
      0.194524f, 0.195245f, 0.195965f, 0.196686f, 0.197406f, 0.198127f,
      0.198847f, 0.199568f, 0.200288f, 0.201009f, 0.201729f, 0.202450f,
      0.203170f, 0.203890f, 0.204611f, 0.205331f, 0.206052f, 0.206772f,
      0.207493f, 0.208213f, 0.208934f, 0.209654f, 0.210375f, 0.211095f,
      0.211816f, 0.212536f, 0.213256f, 0.213977f, 0.214697f, 0.215418f,
      0.216138f, 0.216859f, 0.217579f, 0.218300f, 0.219020f, 0.219741f,
      0.220461f, 0.221182f, 0.221902f, 0.222622f, 0.223343f, 0.224063f,
      0.224784f, 0.225504f, 0.226225f, 0.226945f, 0.227666f, 0.228386f,
      0.229107f, 0.229827f, 0.230548f, 0.231268f, 0.231988f, 0.232709f,
      0.233429f, 0.234150f, 0.234870f, 0.235591f, 0.236311f, 0.237032f,
      0.237752f, 0.238473f, 0.239193f, 0.239914f, 0.240634f, 0.241354f,
      0.242075f, 0.242795f, 0.243516f, 0.244236f, 0.244957f, 0.245677f,
      0.246398f, 0.247118f, 0.247839f, 0.248559f, 0.249280f, 0.250000f,
      0.250720f, 0.251441f, 0.252161f, 0.252882f, 0.253602f, 0.254323f,
      0.255043f, 0.255764f, 0.256484f, 0.257205f, 0.257925f, 0.258646f,
      0.259366f, 0.260086f, 0.260807f, 0.261527f, 0.262248f, 0.262968f,
      0.263689f, 0.264409f, 0.265130f, 0.265850f, 0.266571f, 0.267291f,
      0.268012f, 0.268732f, 0.269452f, 0.270173f, 0.270893f, 0.271614f,
      0.272334f, 0.273055f, 0.273775f, 0.274496f, 0.275216f, 0.275937f,
      0.276657f, 0.277378f, 0.278098f, 0.278818f, 0.279539f, 0.280259f,
      0.280980f, 0.281700f, 0.282421f, 0.283141f, 0.283862f, 0.284582f,
      0.285303f, 0.286023f, 0.286744f, 0.287464f, 0.288184f, 0.288905f,
      0.289625f, 0.290346f, 0.291066f, 0.291787f, 0.292507f, 0.293228f,
      0.293948f, 0.294669f, 0.295389f, 0.296109f, 0.296830f, 0.297550f,
      0.298271f, 0.298991f, 0.299712f, 0.300432f, 0.301153f, 0.301873f,
      0.302594f, 0.303314f, 0.304035f, 0.304755f, 0.305476f, 0.306196f,
      0.306916f, 0.307637f, 0.308357f, 0.309078f, 0.309798f, 0.310519f,
      0.311239f, 0.311960f, 0.312680f, 0.313401f, 0.314121f, 0.314842f,
      0.315562f, 0.316282f, 0.317003f, 0.317723f, 0.318444f, 0.319164f,
      0.319885f, 0.320605f, 0.321326f, 0.322046f, 0.322767f, 0.323487f,
      0.324207f, 0.324928f, 0.325648f, 0.326369f, 0.327089f, 0.327810f,
      0.328530f, 0.329251f, 0.329971f, 0.330692f, 0.331412f, 0.332133f,
      0.332853f, 0.333573f, 0.334294f, 0.335014f, 0.335735f, 0.336455f,
      0.337176f, 0.337896f, 0.338617f, 0.339337f, 0.340058f, 0.340778f,
      0.341499f, 0.342219f, 0.342939f, 0.343660f, 0.344380f, 0.345101f,
      0.345821f, 0.346542f, 0.347262f, 0.347983f, 0.348703f, 0.349424f,
      0.350144f, 0.350865f, 0.351585f, 0.352305f, 0.353026f, 0.353746f,
      0.354467f, 0.355187f, 0.355908f, 0.356628f, 0.357349f, 0.358069f,
      0.358790f, 0.359510f, 0.360231f, 0.360951f, 0.361671f, 0.362392f,
      0.363112f, 0.363833f, 0.364553f, 0.365274f, 0.365994f, 0.366715f,
      0.367435f, 0.368156f, 0.368876f, 0.369597f, 0.370317f, 0.371037f,
      0.371758f, 0.372478f, 0.373199f, 0.373919f, 0.374640f, 0.375360f,
      0.376081f, 0.376801f, 0.377522f, 0.378242f, 0.378963f, 0.379683f,
      0.380403f, 0.381124f, 0.381844f, 0.382565f, 0.383285f, 0.384006f,
      0.384726f, 0.385447f, 0.386167f, 0.386888f, 0.387608f, 0.388329f,
      0.389049f, 0.389769f, 0.390490f, 0.391210f, 0.391931f, 0.392651f,
      0.393372f, 0.394092f, 0.394813f, 0.395533f, 0.396254f, 0.396974f,
      0.397695f, 0.398415f, 0.399135f, 0.399856f, 0.400576f, 0.401297f,
      0.402017f, 0.402738f, 0.403458f, 0.404179f, 0.404899f, 0.405620f,
      0.406340f, 0.407061f, 0.407781f, 0.408501f, 0.409222f, 0.409942f,
      0.410663f, 0.411383f, 0.412104f, 0.412824f, 0.413545f, 0.414265f,
      0.414986f, 0.415706f, 0.416427f, 0.417147f, 0.417867f, 0.418588f,
      0.419308f, 0.420029f, 0.420749f, 0.421470f, 0.422190f, 0.422911f,
      0.423631f, 0.424352f, 0.425072f, 0.425793f, 0.426513f, 0.427233f,
      0.427954f, 0.428674f, 0.429395f, 0.430115f, 0.430836f, 0.431556f,
      0.432277f, 0.432997f, 0.433718f, 0.434438f, 0.435158f, 0.435879f,
      0.436599f, 0.437320f, 0.438040f, 0.438761f, 0.439481f, 0.440202f,
      0.440922f, 0.441643f, 0.442363f, 0.443084f, 0.443804f, 0.444524f,
      0.445245f, 0.445965f, 0.446686f, 0.447406f, 0.448127f, 0.448847f,
      0.449568f, 0.450288f, 0.451009f, 0.451729f, 0.452450f, 0.453170f,
      0.453891f, 0.454611f, 0.455331f, 0.456052f, 0.456772f, 0.457493f,
      0.458213f, 0.458934f, 0.459654f, 0.460375f, 0.461095f, 0.461816f,
      0.462536f, 0.463256f, 0.463977f, 0.464697f, 0.465418f, 0.466138f,
      0.466859f, 0.467579f, 0.468300f, 0.469020f, 0.469741f, 0.470461f,
      0.471182f, 0.471902f, 0.472622f, 0.473343f, 0.474063f, 0.474784f,
      0.475504f, 0.476225f, 0.476945f, 0.477666f, 0.478386f, 0.479107f,
      0.479827f, 0.480548f, 0.481268f, 0.481988f, 0.482709f, 0.483429f,
      0.484150f, 0.484870f, 0.485591f, 0.486311f, 0.487032f, 0.487752f,
      0.488473f, 0.489193f, 0.489914f, 0.490634f, 0.491354f, 0.492075f,
      0.492795f, 0.493516f, 0.494236f, 0.494957f, 0.495677f, 0.496398f,
      0.497118f, 0.497839f, 0.498559f, 0.499280f, 0.500000f, 0.500720f,
      0.501441f, 0.502161f, 0.502882f, 0.503602f, 0.504323f, 0.505043f,
      0.505764f, 0.506484f, 0.507205f, 0.507925f, 0.508646f, 0.509366f,
      0.510086f, 0.510807f, 0.511527f, 0.512248f, 0.512968f, 0.513689f,
      0.514409f, 0.515130f, 0.515850f, 0.516571f, 0.517291f, 0.518012f,
      0.518732f, 0.519452f, 0.520173f, 0.520893f, 0.521614f, 0.522334f,
      0.523055f, 0.523775f, 0.524496f, 0.525216f, 0.525937f, 0.526657f,
      0.527378f, 0.528098f, 0.528818f, 0.529539f, 0.530259f, 0.530980f,
      0.531700f, 0.532421f, 0.533141f, 0.533862f, 0.534582f, 0.535303f,
      0.536023f, 0.536744f, 0.537464f, 0.538184f, 0.538905f, 0.539625f,
      0.540346f, 0.541066f, 0.541787f, 0.542507f, 0.543228f, 0.543948f,
      0.544669f, 0.545389f, 0.546109f, 0.546830f, 0.547550f, 0.548271f,
      0.548991f, 0.549712f, 0.550432f, 0.551153f, 0.551873f, 0.552594f,
      0.553314f, 0.554035f, 0.554755f, 0.555476f, 0.556196f, 0.556916f,
      0.557637f, 0.558357f, 0.559078f, 0.559798f, 0.560519f, 0.561239f,
      0.561960f, 0.562680f, 0.563401f, 0.564121f, 0.564842f, 0.565562f,
      0.566282f, 0.567003f, 0.567723f, 0.568444f, 0.569164f, 0.569885f,
      0.570605f, 0.571326f, 0.572046f, 0.572767f, 0.573487f, 0.574207f,
      0.574928f, 0.575648f, 0.576369f, 0.577089f, 0.577810f, 0.578530f,
      0.579251f, 0.579971f, 0.580692f, 0.581412f, 0.582133f, 0.582853f,
      0.583573f, 0.584294f, 0.585014f, 0.585735f, 0.586455f, 0.587176f,
      0.587896f, 0.588617f, 0.589337f, 0.590058f, 0.590778f, 0.591499f,
      0.592219f, 0.592939f, 0.593660f, 0.594380f, 0.595101f, 0.595821f,
      0.596542f, 0.597262f, 0.597983f, 0.598703f, 0.599424f, 0.600144f,
      0.600865f, 0.601585f, 0.602305f, 0.603026f, 0.603746f, 0.604467f,
      0.605187f, 0.605908f, 0.606628f, 0.607349f, 0.608069f, 0.608790f,
      0.609510f, 0.610231f, 0.610951f, 0.611671f, 0.612392f, 0.613112f,
      0.613833f, 0.614553f, 0.615274f, 0.615994f, 0.616715f, 0.617435f,
      0.618156f, 0.618876f, 0.619597f, 0.620317f, 0.621037f, 0.621758f,
      0.622478f, 0.623199f, 0.623919f, 0.624640f, 0.625360f, 0.626081f,
      0.626801f, 0.627522f, 0.628242f, 0.628963f, 0.629683f, 0.630403f,
      0.631124f, 0.631844f, 0.632565f, 0.633285f, 0.634006f, 0.634726f,
      0.635447f, 0.636167f, 0.636888f, 0.637608f, 0.638329f, 0.639049f,
      0.639769f, 0.640490f, 0.641210f, 0.641931f, 0.642651f, 0.643372f,
      0.644092f, 0.644813f, 0.645533f, 0.646254f, 0.646974f, 0.647695f,
      0.648415f, 0.649135f, 0.649856f, 0.650576f, 0.651297f, 0.652017f,
      0.652738f, 0.653458f, 0.654179f, 0.654899f, 0.655620f, 0.656340f,
      0.657061f, 0.657781f, 0.658501f, 0.659222f, 0.659942f, 0.660663f,
      0.661383f, 0.662104f, 0.662824f, 0.663545f, 0.664265f, 0.664986f,
      0.665706f, 0.666427f, 0.667147f, 0.667867f, 0.668588f, 0.669308f,
      0.670029f, 0.670749f, 0.671470f, 0.672190f, 0.672911f, 0.673631f,
      0.674352f, 0.675072f, 0.675793f, 0.676513f, 0.677233f, 0.677954f,
      0.678674f, 0.679395f, 0.680115f, 0.680836f, 0.681556f, 0.682277f,
      0.682997f, 0.683718f, 0.684438f, 0.685158f, 0.685879f, 0.686599f,
      0.687320f, 0.688040f, 0.688761f, 0.689481f, 0.690202f, 0.690922f,
      0.691643f, 0.692363f, 0.693084f, 0.693804f, 0.694524f, 0.695245f,
      0.695965f, 0.696686f, 0.697406f, 0.698127f, 0.698847f, 0.699568f,
      0.700288f, 0.701009f, 0.701729f, 0.702450f, 0.703170f, 0.703891f,
      0.704611f, 0.705331f, 0.706052f, 0.706772f, 0.707493f, 0.708213f,
      0.708934f, 0.709654f, 0.710375f, 0.711095f, 0.711816f, 0.712536f,
      0.713256f, 0.713977f, 0.714697f, 0.715418f, 0.716138f, 0.716859f,
      0.717579f, 0.718300f, 0.719020f, 0.719741f, 0.720461f, 0.721182f,
      0.721902f, 0.722622f, 0.723343f, 0.724063f, 0.724784f, 0.725504f,
      0.726225f, 0.726945f, 0.727666f, 0.728386f, 0.729107f, 0.729827f,
      0.730548f, 0.731268f, 0.731988f, 0.732709f, 0.733429f, 0.734150f,
      0.734870f, 0.735591f, 0.736311f, 0.737032f, 0.737752f, 0.738473f,
      0.739193f, 0.739914f, 0.740634f, 0.741354f, 0.742075f, 0.742795f,
      0.743516f, 0.744236f, 0.744957f, 0.745677f, 0.746398f, 0.747118f,
      0.747839f, 0.748559f, 0.749280f, 0.750000f, 0.750720f, 0.751441f,
      0.752161f, 0.752882f, 0.753602f, 0.754323f, 0.755043f, 0.755764f,
      0.756484f, 0.757205f, 0.757925f, 0.758646f, 0.759366f, 0.760086f,
      0.760807f, 0.761527f, 0.762248f, 0.762968f, 0.763689f, 0.764409f,
      0.765130f, 0.765850f, 0.766571f, 0.767291f, 0.768012f, 0.768732f,
      0.769452f, 0.770173f, 0.770893f, 0.771614f, 0.772334f, 0.773055f,
      0.773775f, 0.774496f, 0.775216f, 0.775937f, 0.776657f, 0.777378f,
      0.778098f, 0.778818f, 0.779539f, 0.780259f, 0.780980f, 0.781700f,
      0.782421f, 0.783141f, 0.783862f, 0.784582f, 0.785303f, 0.786023f,
      0.786744f, 0.787464f, 0.788184f, 0.788905f, 0.789625f, 0.790346f,
      0.791066f, 0.791787f, 0.792507f, 0.793228f, 0.793948f, 0.794669f,
      0.795389f, 0.796109f, 0.796830f, 0.797550f, 0.798271f, 0.798991f,
      0.799712f, 0.800432f, 0.801153f, 0.801873f, 0.802594f, 0.803314f,
      0.804035f, 0.804755f, 0.805476f, 0.806196f, 0.806916f, 0.807637f,
      0.808357f, 0.809078f, 0.809798f, 0.810519f, 0.811239f, 0.811960f,
      0.812680f, 0.813401f, 0.814121f, 0.814842f, 0.815562f, 0.816282f,
      0.817003f, 0.817723f, 0.818444f, 0.819164f, 0.819885f, 0.820605f,
      0.821326f, 0.822046f, 0.822767f, 0.823487f, 0.824207f, 0.824928f,
      0.825648f, 0.826369f, 0.827089f, 0.827810f, 0.828530f, 0.829251f,
      0.829971f, 0.830692f, 0.831412f, 0.832133f, 0.832853f, 0.833573f,
      0.834294f, 0.835014f, 0.835735f, 0.836455f, 0.837176f, 0.837896f,
      0.838617f, 0.839337f, 0.840058f, 0.840778f, 0.841499f, 0.842219f,
      0.842939f, 0.843660f, 0.844380f, 0.845101f, 0.845821f, 0.846542f,
      0.847262f, 0.847983f, 0.848703f, 0.849424f, 0.850144f, 0.850865f,
      0.851585f, 0.852305f, 0.853026f, 0.853746f, 0.854467f, 0.855187f,
      0.855908f, 0.856628f, 0.857349f, 0.858069f, 0.858790f, 0.859510f,
      0.860231f, 0.860951f, 0.861671f, 0.862392f, 0.863112f, 0.863833f,
      0.864553f, 0.865274f, 0.865994f, 0.866715f, 0.867435f, 0.868156f,
      0.868876f, 0.869597f, 0.870317f, 0.871037f, 0.871758f, 0.872478f,
      0.873199f, 0.873919f, 0.874640f, 0.875360f, 0.876081f, 0.876801f,
      0.877522f, 0.878242f, 0.878963f, 0.879683f, 0.880403f, 0.881124f,
      0.881844f, 0.882565f, 0.883285f, 0.884006f, 0.884726f, 0.885447f,
      0.886167f, 0.886888f, 0.887608f, 0.888329f, 0.889049f, 0.889769f,
      0.890490f, 0.891210f, 0.891931f, 0.892651f, 0.893372f, 0.894092f,
      0.894813f, 0.895533f, 0.896254f, 0.896974f, 0.897695f, 0.898415f,
      0.899135f, 0.899856f, 0.900576f, 0.901297f, 0.902017f, 0.902738f,
      0.903458f, 0.904179f, 0.904899f, 0.905620f, 0.906340f, 0.907061f,
      0.907781f, 0.908501f, 0.909222f, 0.909942f, 0.910663f, 0.911383f,
      0.912104f, 0.912824f, 0.913545f, 0.914265f, 0.914986f, 0.915706f,
      0.916427f, 0.917147f, 0.917867f, 0.918588f, 0.919308f, 0.920029f,
      0.920749f, 0.921470f, 0.922190f, 0.922911f, 0.923631f, 0.924352f,
      0.925072f, 0.925793f, 0.926513f, 0.927233f, 0.927954f, 0.928674f,
      0.929395f, 0.930115f, 0.930836f, 0.931556f, 0.932277f, 0.932997f,
      0.933718f, 0.934438f, 0.935158f, 0.935879f, 0.936599f, 0.937320f,
      0.938040f, 0.938761f, 0.939481f, 0.940202f, 0.940922f, 0.941643f,
      0.942363f, 0.943084f, 0.943804f, 0.944524f, 0.945245f, 0.945965f,
      0.946686f, 0.947406f, 0.948127f, 0.948847f, 0.949568f, 0.950288f,
      0.951009f, 0.951729f, 0.952450f, 0.953170f, 0.953891f, 0.954611f,
      0.955331f, 0.956052f, 0.956772f, 0.957493f, 0.958213f, 0.958934f,
      0.959654f, 0.960375f, 0.961095f, 0.961816f, 0.962536f, 0.963256f,
      0.963977f, 0.964697f, 0.965418f, 0.966138f, 0.966859f, 0.967579f,
      0.968300f, 0.969020f, 0.969741f, 0.970461f, 0.971182f, 0.971902f,
      0.972622f, 0.973343f, 0.974063f, 0.974784f, 0.975504f, 0.976225f,
      0.976945f, 0.977666f, 0.978386f, 0.979107f, 0.979827f, 0.980548f,
      0.981268f, 0.981988f, 0.982709f, 0.983429f, 0.984150f, 0.984870f,
      0.985591f, 0.986311f, 0.987032f, 0.987752f, 0.988473f, 0.989193f,
      0.989914f, 0.990634f, 0.991354f, 0.992075f, 0.992795f, 0.993516f,
      0.994236f, 0.994957f, 0.995677f, 0.996398f, 0.997118f, 0.997839f,
      0.998559f, 0.999280f, 1.000000f
    };
#endif

  CacheView
    *image_view;

  ExceptionInfo
    *exception;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  register ssize_t
    i;

  ssize_t
    y;

  TransformPacket
    *y_map,
    *x_map,
    *z_map;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=MagickTrue;
  progress=0;
  exception=(&image->exception);
  switch (image->colorspace)
  {
    case CMYColorspace:
    {
      /*
        Transform image from CMY to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          SetPixelRed(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelRed(q))));
          SetPixelGreen(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelGreen(q))));
          SetPixelBlue(q,ClampToQuantum((MagickRealType)
            (QuantumRange-GetPixelBlue(q))));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case CMYKColorspace:
    {
      MagickPixelPacket
        zero;

      /*
        Transform image from CMYK to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      GetMagickPixelPacket(image,&zero);
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        MagickPixelPacket
          pixel;

        register IndexPacket
          *restrict indexes;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        indexes=GetCacheViewAuthenticIndexQueue(image_view);
        pixel=zero;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          SetMagickPixelPacket(image,q,indexes+x,&pixel);
          ConvertCMYKToRGB(&pixel);
          SetPixelPacket(image,&pixel,q,indexes+x);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case HSBColorspace:
    {
      /*
        Transform image from HSB to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          brightness,
          hue,
          saturation;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          Quantum
            blue,
            green,
            red;

          hue=(double) (QuantumScale*GetPixelRed(q));
          saturation=(double) (QuantumScale*GetPixelGreen(q));
          brightness=(double) (QuantumScale*GetPixelBlue(q));
          ConvertHSBToRGB(hue,saturation,brightness,&red,&green,&blue);
          SetPixelRed(q,red);
          SetPixelGreen(q,green);
          SetPixelBlue(q,blue);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case HSLColorspace:
    {
      /*
        Transform image from HSL to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          hue,
          lightness,
          saturation;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          Quantum
            blue,
            green,
            red;

          hue=(double) (QuantumScale*GetPixelRed(q));
          saturation=(double) (QuantumScale*GetPixelGreen(q));
          lightness=(double) (QuantumScale*GetPixelBlue(q));
          ConvertHSLToRGB(hue,saturation,lightness,&red,&green,&blue);
          SetPixelRed(q,red);
          SetPixelGreen(q,green);
          SetPixelBlue(q,blue);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case HWBColorspace:
    {
      /*
        Transform image from HWB to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          blackness,
          hue,
          whiteness;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          Quantum
            blue,
            green,
            red;

          hue=(double) (QuantumScale*GetPixelRed(q));
          whiteness=(double) (QuantumScale*GetPixelGreen(q));
          blackness=(double) (QuantumScale*GetPixelBlue(q));
          ConvertHWBToRGB(hue,whiteness,blackness,&red,&green,&blue);
          SetPixelRed(q,red);
          SetPixelGreen(q,green);
          SetPixelBlue(q,blue);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case LabColorspace:
    {
      /*
        Transform image from Lab to RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        double
          a,
          b,
          L,
          X,
          Y,
          Z;

        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        X=0.0;
        Y=0.0;
        Z=0.0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          Quantum
            blue,
            green,
            red;

          L=QuantumScale*GetPixelRed(q);
          a=QuantumScale*GetPixelGreen(q);
          b=QuantumScale*GetPixelBlue(q);
          ConvertLabToXYZ(L,a,b,&X,&Y,&Z);
          ConvertXYZToRGB(X,Y,Z,&red,&green,&blue);
          SetPixelRed(q,red);
          SetPixelGreen(q,green);
          SetPixelBlue(q,blue);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case LogColorspace:
    {
      const char
        *value;

      double
        black,
        density,
        film_gamma,
        gamma,
        reference_black,
        reference_white;

      Quantum
        *logmap;

      /*
        Transform Log to RGB colorspace.
      */
      density=DisplayGamma;
      gamma=DisplayGamma;
      value=GetImageProperty(image,"gamma");
      if (value != (const char *) NULL)
        gamma=1.0/StringToDouble(value,(char **) NULL) != 0.0 ? StringToDouble(
          value,(char **) NULL) : 1.0;
      film_gamma=FilmGamma;
      value=GetImageProperty(image,"film-gamma");
      if (value != (const char *) NULL)
        film_gamma=StringToDouble(value,(char **) NULL);
      reference_black=ReferenceBlack;
      value=GetImageProperty(image,"reference-black");
      if (value != (const char *) NULL)
        reference_black=StringToDouble(value,(char **) NULL);
      reference_white=ReferenceWhite;
      value=GetImageProperty(image,"reference-white");
      if (value != (const char *) NULL)
        reference_white=StringToDouble(value,(char **) NULL);
      logmap=(Quantum *) AcquireQuantumMemory((size_t) MaxMap+1UL,
        sizeof(*logmap));
      if (logmap == (Quantum *) NULL)
        ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
          image->filename);
      black=pow(10.0,(reference_black-reference_white)*(gamma/density)*
        0.002/film_gamma);
      for (i=0; i <= (ssize_t) (reference_black*MaxMap/1024.0); i++)
        logmap[i]=(Quantum) 0;
      for ( ; i < (ssize_t) (reference_white*MaxMap/1024.0); i++)
        logmap[i]=ClampToQuantum((MagickRealType) QuantumRange/(1.0-black)*
          (pow(10.0,(1024.0*i/MaxMap-reference_white)*
          (gamma/density)*0.002/film_gamma)-black));
      for ( ; i <= (ssize_t) MaxMap; i++)
        logmap[i]=(Quantum) QuantumRange;
      if (SetImageStorageClass(image,DirectClass) == MagickFalse)
        return(MagickFalse);
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=(ssize_t) image->columns; x != 0; x--)
        {
          SetPixelRed(q,logmap[ScaleQuantumToMap(
            GetPixelRed(q))]);
          SetPixelGreen(q,logmap[ScaleQuantumToMap(
            GetPixelGreen(q))]);
          SetPixelBlue(q,logmap[ScaleQuantumToMap(
            GetPixelBlue(q))]);
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      logmap=(Quantum *) RelinquishMagickMemory(logmap);
      if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    default:
      break;
  }
  /*
    Allocate the tables.
  */
  x_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*x_map));
  y_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*y_map));
  z_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*z_map));
  if ((x_map == (TransformPacket *) NULL) ||
      (y_map == (TransformPacket *) NULL) ||
      (z_map == (TransformPacket *) NULL))
    {
      if (z_map != (TransformPacket *) NULL)
        z_map=(TransformPacket *) RelinquishMagickMemory(z_map);
      if (y_map != (TransformPacket *) NULL)
        y_map=(TransformPacket *) RelinquishMagickMemory(y_map);
      if (x_map != (TransformPacket *) NULL)
        x_map=(TransformPacket *) RelinquishMagickMemory(x_map);
      ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
        image->filename);
    }
  switch (image->colorspace)
  {
    case OHTAColorspace:
    {
      /*
        Initialize OHTA tables:

          R = I1+1.00000*I2-0.66668*I3
          G = I1+0.00000*I2+1.33333*I3
          B = I1-1.00000*I2-0.66668*I3

        I and Q, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.500000f*(2.000000*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].x=(-0.333340f)*(2.000000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=0.000000f;
        z_map[i].y=0.666665f*(2.000000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=(-0.500000f)*(2.000000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].z=(-0.333340f)*(2.000000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
      }
      break;
    }
    case Rec601YCbCrColorspace:
    case YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables:

          R = Y            +1.402000*Cr
          G = Y-0.344136*Cb-0.714136*Cr
          B = Y+1.772000*Cb

        Cb and Cr, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.000000f;
        z_map[i].x=(1.402000f*0.500000f)*(2.000000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=(-0.344136f*0.500000f)*(2.000000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        z_map[i].y=(-0.714136f*0.500000f)*(2.000000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=(1.772000f*0.500000f)*(2.000000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        z_map[i].z=0.000000f;
      }
      break;
    }
    case Rec709YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables:

          R = Y            +1.574800*Cr
          G = Y-0.187324*Cb-0.468124*Cr
          B = Y+1.855600*Cb

        Cb and Cr, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.000000f;
        z_map[i].x=(1.574800f*0.50000f)*(2.00000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=(-0.187324f*0.50000f)*(2.00000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        z_map[i].y=(-0.468124f*0.50000f)*(2.00000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=(1.855600f*0.50000f)*(2.00000f*(MagickRealType) i-
          (MagickRealType) MaxMap);
        z_map[i].z=0.00000f;
      }
      break;
    }
    case RGBColorspace:
    {
      /*
        Nonlinear sRGB to linear RGB (http://www.w3.org/Graphics/Color/sRGB):

          R = 1.0*R+0.0*G+0.0*B
          G = 0.0*R+1.0*G+0.0*B
          B = 0.0*R+0.0*G+1.0*B
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        MagickRealType
          v;

        v=(MagickRealType) i/(MagickRealType) MaxMap;
        if (((MagickRealType) i/(MagickRealType) MaxMap) <= 0.00313066844250063)
          v*=12.92f;
        else
          v=(MagickRealType) (1.055*pow((double) i/MaxMap,1.0/2.4)-0.055);
        x_map[i].x=1.0f*MaxMap*v;
        y_map[i].x=0.0f*MaxMap*v;
        z_map[i].x=0.0f*MaxMap*v;
        x_map[i].y=0.0f*MaxMap*v;
        y_map[i].y=1.0f*MaxMap*v;
        z_map[i].y=0.0f*MaxMap*v;
        x_map[i].z=0.0f*MaxMap*v;
        y_map[i].z=0.0f*MaxMap*v;
        z_map[i].z=1.0f*MaxMap*v;
      }
      break;
    }
    case XYZColorspace:
    {
      /*
        Initialize CIE XYZ tables (ITU R-709 RGB):

          R =  3.2404542*X-1.5371385*Y-0.4985314*Z
          G = -0.9692660*X+1.8760108*Y+0.0415560*Z
          B =  0.0556434*X-0.2040259*Y+1.057225*Z
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=3.2404542f*(MagickRealType) i;
        x_map[i].y=(-0.9692660f)*(MagickRealType) i;
        x_map[i].z=0.0556434f*(MagickRealType) i;
        y_map[i].x=(-1.5371385f)*(MagickRealType) i;
        y_map[i].y=1.8760108f*(MagickRealType) i;
        y_map[i].z=(-0.2040259f)*(MagickRealType) i;
        z_map[i].x=(-0.4985314f)*(MagickRealType) i;
        z_map[i].y=0.0415560f*(MagickRealType) i;
        z_map[i].z=1.0572252f*(MagickRealType) i;
      }
      break;
    }
    case YCCColorspace:
    {
      /*
        Initialize YCC tables:

          R = Y            +1.340762*C2
          G = Y-0.317038*C1-0.682243*C2
          B = Y+1.632639*C1

        YCC is scaled by 1.3584.  C1 zero is 156 and C2 is at 137.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=1.3584000f*(MagickRealType) i;
        y_map[i].x=0.0000000f;
        z_map[i].x=1.8215000f*((MagickRealType) i-(MagickRealType)
          ScaleQuantumToMap(ScaleCharToQuantum(137)));
        x_map[i].y=1.3584000f*(MagickRealType) i;
        y_map[i].y=(-0.4302726f)*((MagickRealType) i-(MagickRealType)
          ScaleQuantumToMap(ScaleCharToQuantum(156)));
        z_map[i].y=(-0.9271435f)*((MagickRealType) i-(MagickRealType)
          ScaleQuantumToMap(ScaleCharToQuantum(137)));
        x_map[i].z=1.3584000f*(MagickRealType) i;
        y_map[i].z=2.2179000f*((MagickRealType) i-(MagickRealType)
          ScaleQuantumToMap(ScaleCharToQuantum(156)));
        z_map[i].z=0.0000000f;
      }
      break;
    }
    case YIQColorspace:
    {
      /*
        Initialize YIQ tables:

          R = Y+0.95620*I+0.62140*Q
          G = Y-0.27270*I-0.64680*Q
          B = Y-1.10370*I+1.70060*Q

        I and Q, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.47810f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].x=0.31070f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=(-0.13635f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].y=(-0.32340f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=(-0.55185f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].z=0.85030f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
      }
      break;
    }
    case YPbPrColorspace:
    {
      /*
        Initialize YPbPr tables:

          R = Y            +1.402000*C2
          G = Y-0.344136*C1+0.714136*C2
          B = Y+1.772000*C1

        Pb and Pr, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.000000f;
        z_map[i].x=0.701000f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=(-0.172068f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].y=0.357068f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=0.88600f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].z=0.00000f;
      }
      break;
    }
    case YUVColorspace:
    {
      /*
        Initialize YUV tables:

          R = Y          +1.13980*V
          G = Y-0.39380*U-0.58050*V
          B = Y+2.02790*U

        U and V, normally -0.5 through 0.5, must be normalized to the range 0
        through QuantumRange.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.00000f;
        z_map[i].x=0.56990f*(2.0000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].y=(MagickRealType) i;
        y_map[i].y=(-0.19690f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].y=(-0.29025f)*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        x_map[i].z=(MagickRealType) i;
        y_map[i].z=1.01395f*(2.00000f*(MagickRealType) i-(MagickRealType)
          MaxMap);
        z_map[i].z=0.00000f;
      }
      break;
    }
    default:
    {
      /*
        Linear conversion tables.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) \
        dynamic_num_threads_uno(MaxMap)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) i;
        y_map[i].x=0.0f;
        z_map[i].x=0.0f;
        x_map[i].y=0.0f;
        y_map[i].y=(MagickRealType) i;
        z_map[i].y=0.0f;
        x_map[i].z=0.0f;
        y_map[i].z=0.0f;
        z_map[i].z=(MagickRealType) i;
      }
      break;
    }
  }
  /*
    Convert to RGB.
  */
  switch (image->storage_class)
  {
    case DirectClass:
    default:
    {
      /*
        Convert DirectClass image.
      */
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_dos(image->columns,image->rows)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        MagickPixelPacket
          pixel;

        register ssize_t
          x;

        register PixelPacket
          *restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (PixelPacket *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          register size_t
            blue,
            green,
            red;

          red=ScaleQuantumToMap(GetPixelRed(q));
          green=ScaleQuantumToMap(GetPixelGreen(q));
          blue=ScaleQuantumToMap(GetPixelBlue(q));
          pixel.red=x_map[red].x+y_map[green].x+z_map[blue].x;
          pixel.green=x_map[red].y+y_map[green].y+z_map[blue].y;
          pixel.blue=x_map[red].z+y_map[green].z+z_map[blue].z;
          switch (colorspace)
          {
            case YCCColorspace:
            {
#if !defined(MAGICKCORE_HDRI_SUPPORT)
              pixel.red=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
                pixel.red)];
              pixel.green=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
                pixel.green)];
              pixel.blue=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
                pixel.blue)];
#endif
              break;
            }
            case RGBColorspace:
            {
              if ((QuantumScale*pixel.red) <= 0.00313066844250063)
                pixel.red*=12.92f;
              else
                pixel.red=(MagickRealType) QuantumRange*(1.055*pow(
                  QuantumScale*pixel.red,(1.0/2.4))-0.055);
              if ((QuantumScale*pixel.green) <= 0.00313066844250063)
                pixel.green*=12.92f;
              else
                pixel.green=(MagickRealType) QuantumRange*(1.055*pow(
                  QuantumScale*pixel.green,(1.0/2.4))-0.055);
              if ((QuantumScale*pixel.blue) <= 0.00313066844250063)
                pixel.blue*=12.92f;
              else
                pixel.blue=(MagickRealType) QuantumRange*(1.055*pow(
                  QuantumScale*pixel.blue,(1.0/2.4))-0.055);
            }
            default:
              break;
          }
          SetPixelRed(q,ScaleMapToQuantum(pixel.red));
          SetPixelGreen(q,ScaleMapToQuantum(pixel.green));
          SetPixelBlue(q,ScaleMapToQuantum(pixel.blue));
          q++;
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
        if (image->progress_monitor != (MagickProgressMonitor) NULL)
          {
            MagickBooleanType
              proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
            #pragma omp critical (MagickCore_TransformRGBImage)
#endif
            proceed=SetImageProgress(image,TransformRGBImageTag,progress++,
              image->rows);
            if (proceed == MagickFalse)
              status=MagickFalse;
          }
      }
      image_view=DestroyCacheView(image_view);
      break;
    }
    case PseudoClass:
    {
      /*
        Convert PseudoClass image.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static,4) shared(status) \
        dynamic_num_threads_uno(image->colors)
#endif
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        MagickPixelPacket
          pixel;

        register size_t
          blue,
          green,
          red;

        red=ScaleQuantumToMap(image->colormap[i].red);
        green=ScaleQuantumToMap(image->colormap[i].green);
        blue=ScaleQuantumToMap(image->colormap[i].blue);
        pixel.red=x_map[red].x+y_map[green].x+z_map[blue].x;
        pixel.green=x_map[red].y+y_map[green].y+z_map[blue].y;
        pixel.blue=x_map[red].z+y_map[green].z+z_map[blue].z;
        switch (colorspace)
        {
          case YCCColorspace:
          {
#if !defined(MAGICKCORE_HDRI_SUPPORT)
            pixel.red=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
              pixel.red)];
            pixel.green=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
              pixel.green)];
            pixel.blue=QuantumRange*YCCMap[RoundToYCC(1024.0*QuantumScale*
              pixel.blue)];
#endif
            break;
          }
          case RGBColorspace:
          {
            if ((QuantumScale*pixel.red) <= 0.00313066844250063)
              pixel.red*=12.92f;
            else
              pixel.red=(MagickRealType) QuantumRange*(1.055*
                pow(QuantumScale*pixel.red,(1.0/2.4))-0.055);
            if ((QuantumScale*pixel.green) <= 0.00313066844250063)
              pixel.green*=12.92f;
            else
              pixel.green=(MagickRealType) QuantumRange*(1.055*
                pow(QuantumScale*pixel.green,(1.0/2.4))-0.055);
            if ((QuantumScale*pixel.blue) <= 0.00313066844250063)
              pixel.blue*=12.92f;
            else
              pixel.blue=(MagickRealType) QuantumRange*(1.055*
                pow(QuantumScale*pixel.blue,(1.0/2.4))-0.055);
            break;
          }
          default:
            break;
        }
        image->colormap[i].red=ScaleMapToQuantum(pixel.red);
        image->colormap[i].green=ScaleMapToQuantum(pixel.green);
        image->colormap[i].blue=ScaleMapToQuantum(pixel.blue);
      }
      (void) SyncImage(image);
      break;
    }
  }
  /*
    Relinquish resources.
  */
  z_map=(TransformPacket *) RelinquishMagickMemory(z_map);
  y_map=(TransformPacket *) RelinquishMagickMemory(y_map);
  x_map=(TransformPacket *) RelinquishMagickMemory(x_map);
  if (SetImageColorspace(image,sRGBColorspace) == MagickFalse)
    return(MagickFalse);
  return(MagickTrue);
}
