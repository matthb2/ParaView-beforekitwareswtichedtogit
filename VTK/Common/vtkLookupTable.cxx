/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkLookupTable.h"
#include "vtkBitArray.h"
#include "vtkObjectFactory.h"

#include <math.h>

vtkCxxRevisionMacro(vtkLookupTable, "$Revision$");
vtkStandardNewMacro(vtkLookupTable);

// Construct with range=(0,1); and hsv ranges set up for rainbow color table 
// (from red to blue).
vtkLookupTable::vtkLookupTable(int sze, int ext)
{
  this->NumberOfColors = sze;
  this->Table = vtkUnsignedCharArray::New();
  this->Table->SetNumberOfComponents(4);
  this->Table->Allocate(4*sze,4*ext);

  this->HueRange[0] = 0.0;
  this->HueRange[1] = 0.66667;

  this->SaturationRange[0] = 1.0;
  this->SaturationRange[1] = 1.0;

  this->ValueRange[0] = 1.0;
  this->ValueRange[1] = 1.0;

  this->AlphaRange[0] = 1.0;
  this->AlphaRange[1] = 1.0;
  this->Alpha = 1.0;

  this->TableRange[0] = 0.0;
  this->TableRange[1] = 1.0;

  this->Ramp = VTK_RAMP_SCURVE;
  this->Scale = VTK_SCALE_LINEAR;
}

vtkLookupTable::~vtkLookupTable()
{
  this->Table->Delete();
  this->Table = NULL;
}

// Scalar values greater than maximum range value are clamped to maximum
// range value.
void vtkLookupTable::SetTableRange(float r[2])
{
  this->SetTableRange(r[0],r[1]);
}

// Set the minimum/maximum scalar values for scalar mapping. Scalar values
// less than minimum range value are clamped to minimum range value.
// Scalar values greater than maximum range value are clamped to maximum
// range value.
void vtkLookupTable::SetTableRange(float rmin, float rmax)
{
  if (this->Scale == VTK_SCALE_LOG10 && 
      ((rmin > 0 && rmax < 0) || (rmin < 0 && rmax > 0)))
    {
    vtkErrorMacro("Bad table range for log scale: ["<<rmin<<", "<<rmax<<"]");
    return;
    }
  if (rmax < rmin)
    {
    vtkErrorMacro("Bad table range: ["<<rmin<<", "<<rmax<<"]");
    return;
    }

  if (this->TableRange[0] == rmin && this->TableRange[1] == rmax)
    {
    return;
    }

  this->TableRange[0] = rmin;
  this->TableRange[1] = rmax;

  this->Modified();
}

// Have to be careful about the range if scale is logarithmic
void vtkLookupTable::SetScale(int scale)
{
  if (this->Scale == scale)
    {
    return;
    }
  this->Scale = scale;
  this->Modified();

  float rmin = this->TableRange[0];
  float rmax = this->TableRange[1];

  if (this->Scale == VTK_SCALE_LOG10 && 
      ((rmin > 0 && rmax < 0) || (rmin < 0 && rmax > 0)))
    {
    this->TableRange[0] = 1.0f;
    this->TableRange[1] = 10.0f;
    vtkErrorMacro("Bad table range for log scale: ["<<rmin<<", "<<rmax<<"], "
                  "adjusting to [1, 10]");
    return;
    }
}

// Allocate a color table of specified size.
int vtkLookupTable::Allocate(int sz, int ext)
{
  this->NumberOfColors = sz;
  int a = this->Table->Allocate(4*this->NumberOfColors,4*ext);
  this->Modified();
  return a;
}

// Force the lookup table to rebuild
void vtkLookupTable::ForceBuild()
{
  int i, hueCase;
  float hue, sat, val, lx, ly, lz, frac, hinc, sinc, vinc, ainc;
  float rgba[4], alpha;
  unsigned char *c_rgba;

  int maxIndex = this->NumberOfColors - 1;

  hinc = (this->HueRange[1] - this->HueRange[0])/maxIndex;
  sinc = (this->SaturationRange[1] - this->SaturationRange[0])/maxIndex;
  vinc = (this->ValueRange[1] - this->ValueRange[0])/maxIndex;
  ainc = (this->AlphaRange[1] - this->AlphaRange[0])/maxIndex;

  for (i = 0; i <= maxIndex; i++) 
    {
    hue = this->HueRange[0] + i*hinc;
    sat = this->SaturationRange[0] + i*sinc;
    val = this->ValueRange[0] + i*vinc;
    alpha = this->AlphaRange[0] + i*ainc;

    hueCase = static_cast<int>(hue * 6);
    frac = 6*hue - hueCase;
    lx = val*(1.0 - sat);
    ly = val*(1.0 - sat*frac);
    lz = val*(1.0 - sat*(1.0 - frac));

    switch (hueCase) 
      {
      /* 0<hue<1/6 */
      case 0:
      case 6:
        rgba[0] = val;
        rgba[1] = lz;
        rgba[2] = lx;
        break;
        /* 1/6<hue<2/6 */
      case 1:
        rgba[0] = ly;
        rgba[1] = val;
        rgba[2] = lx;
        break;
        /* 2/6<hue<3/6 */
      case 2:
        rgba[0] = lx;
        rgba[1] = val;
        rgba[2] = lz;
        break;
        /* 3/6<hue/4/6 */
      case 3:
        rgba[0] = lx;
        rgba[1] = ly;
        rgba[2] = val;
        break;
        /* 4/6<hue<5/6 */
      case 4:
        rgba[0] = lz;
        rgba[1] = lx;
        rgba[2] = val;
        break;
        /* 5/6<hue<1 */
      case 5:
        rgba[0] = val;
        rgba[1] = lx;
        rgba[2] = ly;
        break;
      }
    rgba[3] = alpha;

    c_rgba = this->Table->WritePointer(4*i,4);

    switch(this->Ramp)
      {
      case VTK_RAMP_SCURVE:
        {
        c_rgba[0] = static_cast<unsigned char> 
          (127.5*(1.0+cos((1.0-static_cast<double>(rgba[0]))*3.141593)));
        c_rgba[1] = static_cast<unsigned char> 
          (127.5*(1.0+cos((1.0-static_cast<double>(rgba[1]))*3.141593)));
        c_rgba[2] = static_cast<unsigned char> 
          (127.5*(1.0+cos((1.0-static_cast<double>(rgba[2]))*3.141593)));
        c_rgba[3] = static_cast<unsigned char> (alpha*255.0);
        /* same code, but with rounding 
           c_rgba[0] = static_cast<unsigned char> 
           (127.5f*(1.0f + (float)cos(double((1.0f-rgba[0])*3.141593f)))+0.5f);
           c_rgba[1] = static_cast<unsigned char> 
           (127.5f*(1.0f + (float)cos(double((1.0f-rgba[1])*3.141593f)))+0.5f);
           c_rgba[2] = static_cast<unsigned char> 
           (127.5f*(1.0f + (float)cos(double((1.0f-rgba[2])*3.141593f)))+0.5f);
           c_rgba[3] = static_cast<unsigned char>(rgba[3]*255.0f + 0.5f);
        */
        }
        break;
      case VTK_RAMP_LINEAR:
        {
        c_rgba[0] = static_cast<unsigned char>((rgba[0])*255.0f + 0.5f);
        c_rgba[1] = static_cast<unsigned char>((rgba[1])*255.0f + 0.5f);
        c_rgba[2] = static_cast<unsigned char>((rgba[2])*255.0f + 0.5f);
        c_rgba[3] = static_cast<unsigned char>(rgba[3]*255.0f + 0.5f);
        }
        break;
      case VTK_RAMP_SQRT:
        {
        c_rgba[0] = static_cast<unsigned char>((sqrt(rgba[0]))*255.0f + 0.5f);
        c_rgba[1] = static_cast<unsigned char>((sqrt(rgba[1]))*255.0f + 0.5f);
        c_rgba[2] = static_cast<unsigned char>((sqrt(rgba[2]))*255.0f + 0.5f);
        c_rgba[3] = static_cast<unsigned char>((sqrt(rgba[3]))*255.0f + 0.5f);
        }
        break;
      }
    
    }
  this->BuildTime.Modified();
}

// Generate lookup table from hue, saturation, value, alpha min/max values. 
// Table is built from linear ramp of each value.
void vtkLookupTable::Build()
{
  if (this->Table->GetNumberOfTuples() < 1 ||
      (this->GetMTime() > this->BuildTime && 
       this->InsertTime <= this->BuildTime))
    {
    this->ForceBuild();
    }
}

// get the color for a scalar value
void vtkLookupTable::GetColor(float v, float rgb[3])
{
  unsigned char *rgb8 = this->MapValue(v);

  rgb[0] = rgb8[0]/255.0;
  rgb[1] = rgb8[1]/255.0;
  rgb[2] = rgb8[2]/255.0;
}

// get the opacity (alpha) for a scalar value
float vtkLookupTable::GetOpacity(float v)
{
  unsigned char *rgb8 = this->MapValue(v);

  return rgb8[3]/255.0;
}

// There is a little more to this than simply taking the log10 of the
// two range values: we do conversion of negative ranges to positive
// ranges, and conversion of zero to a 'very small number'
void vtkLookupTableLogRange(float range[2], float logRange[2])
{
  float rmin = range[0];
  float rmax = range[1];

  if (rmin == 0)
    {
    rmin = 1.0e-6*(rmax - rmin);
    if (rmax < 0)
      {
      rmin = -rmin;
      }
    }
  if (rmax == 0)
    {
    rmax = 1.0e-6*(rmin - rmax);
    if (rmin < 0)
      {
      rmax = -rmax;
      }
    }
  if (rmin < 0 && rmax < 0)
    {
    logRange[0] = log10(-(double)rmin);
    logRange[1] = log10(-(double)rmax);
    }
  else if (rmin > 0 && rmax > 0)
    {
    logRange[0] = log10((double)rmin);
    logRange[1] = log10((double)rmax);
    }
}

// Apply log to value, with appropriate constraints.
inline float vtkApplyLogScale(float v, float range[2], 
                                 float logRange[2])
{
  // is the range set for negative numbers?
  if (range[0] < 0)
    {
    if (v < 0)
      {
      v = log10(-static_cast<double>(v));
      }
    else if (range[0] > range[1])
      {
      v = logRange[0];
      }
    else
      {
      v = logRange[1];
      }
    }
  else
    {
    if (v > 0)
      {
      v = log10(static_cast<double>(v));
      }
    else if (range[0] < range[1])
      {
      v = logRange[0];
      }
    else
      {
      v = logRange[1];
      }
    }
  return v;
}                 

// Apply shift/scale to the scalar value v and do table lookup.
inline unsigned char *vtkLinearLookup(float v,   
                                          unsigned char *table,
                                          float maxIndex,
                                          float shift, float scale)
{
  float findx = (v + shift)*scale;
  if (findx < 0)
    {
    findx = 0;
    }
  if (findx > maxIndex)
    {
    findx = maxIndex;
    }
  return &table[4*static_cast<int>(findx)];
  /* round
  return &table[4*(int)(findx + 0.5f)];
  */
}

// Given a scalar value v, return an index into the lookup table
vtkIdType vtkLookupTable::GetIndex(float v)
{
  float maxIndex = this->NumberOfColors - 1;
  float shift, scale;

  if (this->Scale == VTK_SCALE_LOG10)
    {   // handle logarithmic scale
    float logRange[2];
    vtkLookupTableLogRange(this->TableRange, logRange);
    shift = -logRange[0];
    if (logRange[1] <= logRange[0])
      {
      scale = VTK_LARGE_FLOAT;
      }
    else
      {
      scale = (maxIndex + 1)/(logRange[1] - logRange[0]);
      }
    /* correct scale
    scale = maxIndex/(logRange[1] - logRange[0]);
    */
    v = vtkApplyLogScale(v, this->TableRange, logRange);
    }
  else
    {   // plain old linear
    shift = -this->TableRange[0];
    if (this->TableRange[1] <= this->TableRange[0])
      {
      scale = VTK_LARGE_FLOAT;
      }
    else
      {
      scale = (maxIndex + 1)/(this->TableRange[1] - this->TableRange[0]);
      }
    /* correct scale
    scale = maxIndex/(this->TableRange[1] - this->TableRange[0]);
    */
    }

  // map to an index
  float findx = (v + shift)*scale;
  if (findx < 0)
    {
    findx = 0;
    }
  if (findx > maxIndex)
    {
    findx = maxIndex;
    }
  return static_cast<int>(findx);
}

// Given a scalar value v, return an rgba color value from lookup table.
unsigned char *vtkLookupTable::MapValue(float v)
{
  float maxIndex = this->NumberOfColors - 1;
  float shift, scale;

  if (this->Scale == VTK_SCALE_LOG10)
    {   // handle logarithmic scale
    float logRange[2];
    vtkLookupTableLogRange(this->TableRange, logRange);
    shift = -logRange[0];
    if (logRange[1] <= logRange[0])
      {
      scale = VTK_LARGE_FLOAT;
      }
    else
      {
      scale = (maxIndex + 1)/(logRange[1] - logRange[0]);
      }
    /* correct scale
    scale = maxIndex/(logRange[1] - logRange[0]);
    */
    v = vtkApplyLogScale(v, this->TableRange, logRange);
    }
  else
    {   // plain old linear
    shift = -this->TableRange[0];
    if (this->TableRange[1] <= this->TableRange[0])
      {
      scale = VTK_LARGE_FLOAT;
      }
    else
      {
      scale = (maxIndex + 1)/(this->TableRange[1] - this->TableRange[0]);
      }
    /* correct scale
    scale = maxIndex/(this->TableRange[1] - this->TableRange[0]);
    */
    }

  // this is the same for log or linear
  return vtkLinearLookup(v, this->Table->GetPointer(0), maxIndex, 
                         shift, scale); 
}

// Although this is a relatively expensive calculation,
// it is only done on the first render. Colors are cached
// for subsequent renders.
template<class T>
void vtkLookupTableMapMag(vtkLookupTable *self, T *input, 
                          unsigned char *output, int length, 
                          int inIncr, int outFormat)
{
  double tmp, sum;
  double *mag;
  int i, j;

  mag = new double[length];
  for (i = 0; i < length; ++i)
    {
    sum = 0;
    for (j = 0; j < inIncr; ++j)
      {
      tmp = (double)(*input);  
      sum += (tmp * tmp);
      ++input;
      }
    mag[i] = sqrt(sum);
    }

  vtkLookupTableMapData(self, mag, output, length, 1, outFormat);

  delete [] mag;
}


// accelerate the mapping by copying the data in 32-bit chunks instead
// of 8-bit chunks
template<class T>
void vtkLookupTableMapData(vtkLookupTable *self, T *input, 
                           unsigned char *output, int length, 
                           int inIncr, int outFormat)
{
  int i = length;
  float *range = self->GetTableRange();
  float maxIndex = self->GetNumberOfColors() - 1;
  float shift, scale;
  unsigned char *table = self->GetPointer(0);
  unsigned char *cptr;
  float alpha;

  if ( (alpha=self->GetAlpha()) >= 1.0 ) //no blending required 
    {
    if (self->GetScale() == VTK_SCALE_LOG10)
      {
      float val;
      float logRange[2];
      vtkLookupTableLogRange(range, logRange);
      shift = -logRange[0];
      if (logRange[1] <= logRange[0])
        {
        scale = VTK_LARGE_FLOAT;
        }
      else
        {
        scale = (maxIndex + 1)/(logRange[1] - logRange[0]);
        }
      /* correct scale
      scale = maxIndex/(logRange[1] - logRange[0]);
      */
      if (outFormat == VTK_RGBA)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;     
          input += inIncr;
          }
        }
      else if (outFormat == VTK_RGB)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_LUMINANCE_ALPHA)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          *output++ = cptr[3];
          input += inIncr;
          }
        }
      else // outFormat == VTK_LUMINANCE
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          input += inIncr;
          }
        }
      }//if log scale

    else //not log scale
      {
      shift = -range[0];
      if (range[1] <= range[0])
        {
        scale = VTK_LARGE_FLOAT;
        }
      else
        {
        scale = (maxIndex + 1)/(range[1] - range[0]);
        }
      /* correct scale
      scale = maxIndex/(range[1] - range[0]);
      */

      if (outFormat == VTK_RGBA)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;     
          input += inIncr;
          }
        }
      else if (outFormat == VTK_RGB)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_LUMINANCE_ALPHA)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          *output++ = cptr[3];
          input += inIncr;
          }
        }
      else // outFormat == VTK_LUMINANCE
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          input += inIncr;
          }
        }
      }//if not log lookup
    }//if blending not needed

  else //blend with the specified alpha
    {
    if (self->GetScale() == VTK_SCALE_LOG10)
      {
      float val;
      float logRange[2];
      vtkLookupTableLogRange(range, logRange);
      shift = -logRange[0];
      if (logRange[1] <= logRange[0])
        {
        scale = VTK_LARGE_FLOAT;
        }
      else
        {
        scale = (maxIndex + 1)/(logRange[1] - logRange[0]);
        }
      /* correct scale
      scale = maxIndex/(logRange[1] - logRange[0]);
      */
      if (outFormat == VTK_RGBA)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = static_cast<unsigned char>((*cptr)*alpha); cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_RGB)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_LUMINANCE_ALPHA)
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          *output++ = static_cast<unsigned char>(alpha*cptr[3]);
          input += inIncr;
          }
        }
      else // outFormat == VTK_LUMINANCE
        {
        while (--i >= 0) 
          {
          val = vtkApplyLogScale(*input, range, logRange);
          cptr = vtkLinearLookup(val, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          input += inIncr;
          }
        }
      }//log scale with blending

    else //no log scale with blending
      {
      shift = -range[0];
      if (range[1] <= range[0])
        {
        scale = VTK_LARGE_FLOAT;
        }
      else
        {
        scale = (maxIndex + 1)/(range[1] - range[0]);
        }
      /* correct scale
      scale = maxIndex/(range[1] - range[0]);
      */

      if (outFormat == VTK_RGBA)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = static_cast<unsigned char>((*cptr)*alpha); cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_RGB)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = *cptr++;
          *output++ = *cptr++;
          *output++ = *cptr++;
          input += inIncr;
          }
        }
      else if (outFormat == VTK_LUMINANCE_ALPHA)
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          *output++ = static_cast<unsigned char>(cptr[3]*alpha);
          input += inIncr;
          }
        }
      else // outFormat == VTK_LUMINANCE
        {
        while (--i >= 0) 
          {
          cptr = vtkLinearLookup(*input, table, maxIndex, shift, scale); 
          *output++ = static_cast<unsigned char>(cptr[0]*0.30 + cptr[1]*0.59 + 
                                                 cptr[2]*0.11 + 0.5);
          input += inIncr;
          }
        }
      }//no log scale
    }//alpha blending
}

void vtkLookupTable::MapScalarsThroughTable2(void *input, 
                                             unsigned char *output,
                                             int inputDataType, 
                                             int numberOfValues,
                                             int inputIncrement,
                                             int outputFormat)
{
  if (this->UseMagnitude && inputIncrement > 1)
    {
    switch (inputDataType)
      {
      case VTK_BIT:
        vtkErrorMacro("Cannot comput magnitude of bit array.");
        break;
      case VTK_CHAR:
        vtkLookupTableMapMag(this,static_cast<char *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return; 
      case VTK_UNSIGNED_CHAR:
        vtkLookupTableMapMag(this,static_cast<unsigned char *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_SHORT:
        vtkLookupTableMapMag(this,static_cast<short *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_UNSIGNED_SHORT:
        vtkLookupTableMapMag(this,static_cast<unsigned short *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_INT:
        vtkLookupTableMapMag(this,static_cast<int *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_UNSIGNED_INT:
        vtkLookupTableMapMag(this,static_cast<unsigned int *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_LONG:
        vtkLookupTableMapMag(this,static_cast<long *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_UNSIGNED_LONG:
        vtkLookupTableMapMag(this,static_cast<unsigned long *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_FLOAT:
        vtkLookupTableMapMag(this,static_cast<float *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      case VTK_DOUBLE:
        vtkLookupTableMapMag(this,static_cast<double *>(input),output,
                             numberOfValues,inputIncrement,outputFormat);
        return;
      default:
        vtkErrorMacro(<< "MapImageThroughTable: Unknown input ScalarType");
        return;
      }
    }

  switch (inputDataType)
    {
    case VTK_BIT:
      {
      vtkIdType i, id;
      vtkBitArray *bitArray = vtkBitArray::New();
      bitArray->SetVoidArray(input,numberOfValues,1);
      vtkUnsignedCharArray *newInput = vtkUnsignedCharArray::New();
      newInput->SetNumberOfValues(numberOfValues);
      for (id=i=0; i<numberOfValues; i++, id+=inputIncrement)
        {
        newInput->SetValue(i, bitArray->GetValue(id));
        }
      vtkLookupTableMapData(this,
                            static_cast<unsigned char*>(newInput->GetPointer(0)),
                            output,numberOfValues,
                            inputIncrement,outputFormat);
      newInput->Delete();
      bitArray->Delete();
      }
      break;
      
    case VTK_CHAR:
      vtkLookupTableMapData(this,static_cast<char *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_UNSIGNED_CHAR:
      vtkLookupTableMapData(this,static_cast<unsigned char *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_SHORT:
      vtkLookupTableMapData(this,static_cast<short *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
    break;
      
    case VTK_UNSIGNED_SHORT:
      vtkLookupTableMapData(this,static_cast<unsigned short *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_INT:
      vtkLookupTableMapData(this,static_cast<int *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_UNSIGNED_INT:
      vtkLookupTableMapData(this,static_cast<unsigned int *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_LONG:
      vtkLookupTableMapData(this,static_cast<long *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_UNSIGNED_LONG:
      vtkLookupTableMapData(this,static_cast<unsigned long *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_FLOAT:
      vtkLookupTableMapData(this,static_cast<float *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    case VTK_DOUBLE:
      vtkLookupTableMapData(this,static_cast<double *>(input),output,
                            numberOfValues,inputIncrement,outputFormat);
      break;
      
    default:
      vtkErrorMacro(<< "MapImageThroughTable: Unknown input ScalarType");
      return;
    }
}  

// Specify the number of values (i.e., colors) in the lookup
// table. This method simply allocates memory and prepares the table
// for use with SetTableValue(). It differs from Build() method in
// that the allocated memory is not initialized according to HSVA ramps.
void vtkLookupTable::SetNumberOfTableValues(vtkIdType number)
{
  if (this->NumberOfColors == number)
    {
    return;
    }
  this->Modified();
  this->NumberOfColors = number;
  this->Table->SetNumberOfTuples(number);
}

// Directly load color into lookup table. Use [0,1] float values for color
// component specification. Make sure that you've either used the
// Build() method or used SetNumberOfTableValues() prior to using this method.
void vtkLookupTable::SetTableValue(vtkIdType indx, float rgba[4])
{
  // Check the index to make sure it is valid
  if (indx < 0)
    {
    vtkErrorMacro("Can't set the table value for negative index " << indx);
    return;
    }
  if (indx >= this->NumberOfColors)
    {
    vtkErrorMacro("Index " << indx << 
                  " is greater than the number of colors " << 
                  this->NumberOfColors);
    return;
    }

  unsigned char *_rgba = this->Table->WritePointer(4*indx,4);

  _rgba[0] = static_cast<unsigned char>(rgba[0]*255.0f + 0.5f);
  _rgba[1] = static_cast<unsigned char>(rgba[1]*255.0f + 0.5f);
  _rgba[2] = static_cast<unsigned char>(rgba[2]*255.0f + 0.5f);
  _rgba[3] = static_cast<unsigned char>(rgba[3]*255.0f + 0.5f);

  this->InsertTime.Modified();
  this->Modified();
}

// Directly load color into lookup table. Use [0,1] float values for color 
// component specification.
void vtkLookupTable::SetTableValue(vtkIdType indx, float r, float g, float b, 
                                   float a)
{
  float rgba[4];
  rgba[0] = r; rgba[1] = g; rgba[2] = b; rgba[3] = a;
  this->SetTableValue(indx,rgba);
}

// Return a rgba color value for the given index into the lookup Table. Color
// components are expressed as [0,1] float values.
void vtkLookupTable::GetTableValue(vtkIdType indx, float rgba[4])
{
  unsigned char *_rgba;

  indx = (indx < 0 ? 0 : (indx >= this->NumberOfColors ? 
                          this->NumberOfColors-1 : indx));

  _rgba = this->Table->GetPointer(indx*4);

  rgba[0] = _rgba[0]/255.0;
  rgba[1] = _rgba[1]/255.0;
  rgba[2] = _rgba[2]/255.0;
  rgba[3] = _rgba[3]/255.0;
}

// Return a rgba color value for the given index into the lookup table. Color
// components are expressed as [0,1] float values.
float *vtkLookupTable::GetTableValue(vtkIdType indx)
{
  this->GetTableValue(indx, this->RGBA);
  return this->RGBA;
}

void vtkLookupTable::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "TableRange: (" << this->TableRange[0] << ", "
     << this->TableRange[1] << ")\n";
  os << indent << "Scale: " 
     << (this->Scale == VTK_SCALE_LOG10 ? "Log10\n" : "Linear\n");    
  os << indent << "HueRange: (" << this->HueRange[0] << ", "
     << this->HueRange[1] << ")\n";
  os << indent << "SaturationRange: (" << this->SaturationRange[0] << ", "
     << this->SaturationRange[1] << ")\n";
  os << indent << "ValueRange: (" << this->ValueRange[0] << ", "
     << this->ValueRange[1] << ")\n";
  os << indent << "AlphaRange: (" << this->AlphaRange[0] << ", "
     << this->AlphaRange[1] << ")\n";
  os << indent << "NumberOfTableValues: " 
     << this->GetNumberOfTableValues() << "\n";
  os << indent << "NumberOfColors: " << this->NumberOfColors << "\n";
  os << indent << "Ramp: "
     << (this->Ramp == VTK_RAMP_SCURVE ? "SCurve\n" : "Linear\n");  
  os << indent << "InsertTime: " <<this->InsertTime.GetMTime() << "\n";
  os << indent << "BuildTime: " <<this->BuildTime.GetMTime() << "\n";
}


