/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.


     THIS CLASS IS PATENT PENDING.

     Application of this software for commercial purposes requires 
     a license grant from Kitware. Contact:
         Ken Martin
         Kitware
         28 Corporate Drive Suite 204,
         Clifton Park, NY 12065
         Phone:1-518-371-3971 
     for more information.

=========================================================================*/
#include "vtkSynchronizedTemplates3D.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCharArray.h"
#include "vtkDoubleArray.h"
#include "vtkExtentTranslator.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkMath.h"
#include "vtkMultiThreader.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkStructuredPoints.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"

#include <math.h>

vtkCxxRevisionMacro(vtkSynchronizedTemplates3D, "$Revision$");
vtkStandardNewMacro(vtkSynchronizedTemplates3D);

//----------------------------------------------------------------------------
// Description:
// Construct object with initial scalar range (0,1) and single contour value
// of 0.0. The ImageRange are set to extract the first k-plane.
vtkSynchronizedTemplates3D::vtkSynchronizedTemplates3D()
{
  int idx;

  this->NumberOfRequiredInputs = 1;
  this->SetNumberOfInputPorts(1);
  this->ContourValues = vtkContourValues::New();
  this->ComputeNormals = 1;
  this->ComputeGradients = 0;
  this->ComputeScalars = 1;

  this->ExecuteExtent[0] = this->ExecuteExtent[1] 
    = this->ExecuteExtent[2] = this->ExecuteExtent[3] 
    = this->ExecuteExtent[4] = this->ExecuteExtent[5] = 0;

  this->Threader = vtkMultiThreader::New();
  this->NumberOfThreads = this->Threader->GetNumberOfThreads();
  
  for (idx = 0; idx < VTK_MAX_THREADS; ++idx)
    {
    this->Threads[idx] = NULL;
    }
  this->InputScalarsSelection = NULL;
  
  this->ArrayComponent = 0;
}

//----------------------------------------------------------------------------
vtkSynchronizedTemplates3D::~vtkSynchronizedTemplates3D()
{
  this->ContourValues->Delete();
  this->Threader->Delete();
  this->SetInputScalarsSelection(NULL);
}

//----------------------------------------------------------------------------
// Overload standard modified time function. If contour values are modified,
// then this object is modified as well.
unsigned long vtkSynchronizedTemplates3D::GetMTime()
{
  unsigned long mTime=this->vtkPolyDataSource::GetMTime();
  unsigned long mTime2=this->ContourValues->GetMTime();

  mTime = ( mTime2 > mTime ? mTime2 : mTime );
  return mTime;
}


//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3DInitializeOutput(
  vtkSynchronizedTemplates3D *self, int *ext,vtkImageData *input,
  vtkPolyData *o, vtkFloatArray *scalars, vtkFloatArray *normals,
  vtkFloatArray *gradients)
{
  vtkPoints *newPts;
  vtkCellArray *newPolys;
  long estimatedSize;  
  
  estimatedSize = (int) pow ((double) 
      ((ext[1]-ext[0]+1)*(ext[3]-ext[2]+1)*(ext[5]-ext[4]+1)), .75);
  if (estimatedSize < 1024)
    {
    estimatedSize = 1024;
    }
  newPts = vtkPoints::New();
  newPts->Allocate(estimatedSize,estimatedSize);
  newPolys = vtkCellArray::New();
  newPolys->Allocate(newPolys->EstimateSize(estimatedSize,3));

  o->GetPointData()->CopyAllOn();
  // It is more efficient to just create the scalar array 
  // rather than redundantly interpolate the scalars.
  if (self->GetInputScalarsSelection())
    {
    o->GetPointData()->CopyFieldOff(self->GetInputScalarsSelection());
    }
  else
    {
    o->GetPointData()->CopyScalarsOff();
    }

  if (normals)
    {
    normals->SetNumberOfComponents(3);
    normals->Allocate(3*estimatedSize,3*estimatedSize/2);
    normals->SetName("Normals");
    }
  if (gradients)
    {
    gradients->SetNumberOfComponents(3);
    gradients->Allocate(3*estimatedSize,3*estimatedSize/2);
    gradients->SetName("Gradients");
    }
  if (scalars)
    {
    // A temporary name.
    scalars->SetName("Scalars");
    }
  
  o->GetPointData()->InterpolateAllocate(input->GetPointData(),
                                         estimatedSize,estimatedSize/2);
  o->GetCellData()->CopyAllocate(input->GetCellData(),
                                 estimatedSize,estimatedSize/2);
  
  o->SetPoints(newPts);
  newPts->Delete();

  o->SetPolys(newPolys);
  newPolys->Delete();

}


//----------------------------------------------------------------------------
// Calculate the gradient using central difference.
template <class T>
void vtkSTComputePointGradient(int i, int j, int k, T *s, int *wholeExt, 
                               int xInc, int yInc, int zInc,
                               double *spacing, double n[3])
{
  double sp, sm;

  // x-direction
  if ( i == wholeExt[0] )
    {
    sp = *(s+xInc);
    sm = *s;
    n[0] = (sp - sm) / spacing[0];
    }
  else if ( i == wholeExt[1] )
    {
    sp = *s;
    sm = *(s-xInc);
    n[0] = (sp - sm) / spacing[0];
    }
  else
    {
    sp = *(s+xInc);
    sm = *(s-xInc);
    n[0] = 0.5 * (sp - sm) / spacing[0];
    }

  // y-direction
  if ( j == wholeExt[2] )
    {
    sp = *(s+yInc);
    sm = *s;
    n[1] = (sp - sm) / spacing[1];
    }
  else if ( j == wholeExt[3] )
    {
    sp = *s;
    sm = *(s-yInc);
    n[1] = (sp - sm) / spacing[1];
    }
  else
    {
    sp = *(s+yInc);
    sm = *(s-yInc);
    n[1] = 0.5 * (sp - sm) / spacing[1];
    }

  // z-direction
  if ( k == wholeExt[4] )
    {
    sp = *(s+zInc);
    sm = *s;
    n[2] = (sp - sm) / spacing[2];
    }
  else if ( k == wholeExt[5] )
    {
    sp = *s;
    sm = *(s-zInc);
    n[2] = (sp - sm) / spacing[2];
    }
  else
    {
    sp = *(s+zInc);
    sm = *(s-zInc);
    n[2] = 0.5 * (sp - sm) / spacing[2];
    }
}

//----------------------------------------------------------------------------
#define VTK_CSP3PA(i2,j2,k2,s) \
if (NeedGradients) \
{ \
  if (!g0) \
    { \
    vtkSTComputePointGradient(i, j, k, s0, wholeExt, xInc, yInc, zInc, spacing, n0); \
    g0 = 1; \
    } \
  vtkSTComputePointGradient(i2, j2, k2, s, wholeExt, xInc, yInc, zInc, spacing, n1); \
  for (jj=0; jj<3; jj++) \
    { \
    n[jj] = n0[jj] + t * (n1[jj] - n0[jj]); \
    } \
  if (ComputeGradients) \
    { \
    newGradients->InsertNextTuple(n); \
    } \
  if (ComputeNormals) \
    { \
    vtkMath::Normalize(n); \
    n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; \
    newNormals->InsertNextTuple(n); \
    }   \
} \
if (ComputeScalars) \
{ \
  newScalars->InsertNextTuple(&value); \
}

//----------------------------------------------------------------------------
//
// Contouring filter specialized for images
//
template <class T>
void ContourImage(vtkSynchronizedTemplates3D *self, int *exExt,
                  vtkImageData *data, vtkPolyData *output, T *ptr, 
                  int threadId, const char* inputScalars)
{
  int *inExt = self->GetInput()->GetExtent();
  int xdim = exExt[1] - exExt[0] + 1;
  int ydim = exExt[3] - exExt[2] + 1;
  double *values = self->GetValues();
  int numContours = self->GetNumberOfContours();
  T *inPtrX, *inPtrY, *inPtrZ;
  T *s0, *s1, *s2, *s3;
  int xMin, xMax, yMin, yMax, zMin, zMax;
  int xInc, yInc, zInc;
  double *origin = data->GetOrigin();
  double *spacing = data->GetSpacing();
  int *isect1Ptr, *isect2Ptr;
  double y, z, t;
  int i, j, k;
  int zstep, yisectstep;
  int offsets[12];
  int ComputeNormals = self->GetComputeNormals();
  int ComputeGradients = self->GetComputeGradients();
  int ComputeScalars = self->GetComputeScalars();
  int NeedGradients = ComputeGradients || ComputeNormals;
  double n[3], n0[3], n1[3];
  int jj, g0;
  int *tablePtr;
  int idx, vidx;
  double x[3], xz[3];
  int v0, v1, v2, v3;
  vtkIdType ptIds[3];
  double value;
  int *wholeExt;
  // We need to know the edgePointId's for interpolating attributes.
  int edgePtId, inCellId, outCellId;
  vtkPointData *inPD = self->GetInput()->GetPointData();
  vtkCellData *inCD = self->GetInput()->GetCellData();
  vtkPointData *outPD = output->GetPointData();  
  vtkCellData *outCD = output->GetCellData();  
  // Use to be arguments
  vtkFloatArray *newScalars = NULL;
  vtkFloatArray *newNormals = NULL;
  vtkFloatArray *newGradients = NULL;
  vtkPoints *newPts;
  vtkCellArray *newPolys;
  ptr += self->GetArrayComponent();
  
  if (ComputeScalars)
    {
    newScalars = vtkFloatArray::New();
    }
  if (ComputeNormals)
    {
    newNormals = vtkFloatArray::New();
    }
  if (ComputeGradients)
    {
    newGradients = vtkFloatArray::New();
    }
  vtkSynchronizedTemplates3DInitializeOutput(self, exExt, 
                                             self->GetInput(), output, 
                                             newScalars, newNormals, 
                                             newGradients);
  newPts = output->GetPoints();
  newPolys = output->GetPolys();
  
  // this is an exploded execute extent.
  xMin = exExt[0];
  xMax = exExt[1];
  yMin = exExt[2];
  yMax = exExt[3];
  zMin = exExt[4];
  zMax = exExt[5];
  
  // increments to move through scalars. Compute these ourself because
  // we may be contouring an array other than scalars.
  if (inputScalars)
    {
    xInc = inPD->GetArray(inputScalars)->GetNumberOfComponents();
    }
  else
    {
    xInc = self->GetInput()->GetNumberOfScalarComponents();
    }
  yInc = xInc*(inExt[1]-inExt[0]+1);
  zInc = yInc*(inExt[3]-inExt[2]+1);

  wholeExt = self->GetInput()->GetWholeExtent();
  
  // Kens increments, probably to do with edge array
  zstep = xdim*ydim;
  yisectstep = xdim*3;
  // compute offsets probably how to get to the edges in the edge array.
  offsets[0] = -xdim*3;
  offsets[1] = -xdim*3 + 1;
  offsets[2] = -xdim*3 + 2;
  offsets[3] = -xdim*3 + 4;
  offsets[4] = -xdim*3 + 5;
  offsets[5] = 0;
  offsets[6] = 2;
  offsets[7] = 5;
  offsets[8] = (zstep - xdim)*3;
  offsets[9] = (zstep - xdim)*3 + 1;
  offsets[10] = (zstep - xdim)*3 + 4;
  offsets[11] = zstep*3;
    
  // allocate storage array
  int *isect1 = new int [xdim*ydim*3*2];
  // set impossible edges to -1
  for (i = 0; i < ydim; i++)
    {
    isect1[(i+1)*xdim*3-3] = -1;
    isect1[(i+1)*xdim*3*2-3] = -1;
    }
  for (i = 0; i < xdim; i++)
    {
    isect1[((ydim-1)*xdim + i)*3 + 1] = -1;
    isect1[((ydim-1)*xdim + i)*3*2 + 1] = -1;
    }

  // for each contour
  for (vidx = 0; vidx < numContours; vidx++)
    {
    value = values[vidx];
    inPtrZ = ptr;
    s2 = inPtrZ;
    v2 = (*s2 < value ? 0 : 1);

    //==================================================================
    for (k = zMin; k <= zMax; k++)
      {
      if (!threadId)
        {
        self->UpdateProgress((double)vidx/numContours + 
                             (k-zMin)/((zMax - zMin+1.0)*numContours));
        }
      z = origin[2] + spacing[2]*k;
      x[2] = z;

      // swap the buffers
      if (k%2)
        {
        offsets[8] = (zstep - xdim)*3;
        offsets[9] = (zstep - xdim)*3 + 1;
        offsets[10] = (zstep - xdim)*3 + 4;
        offsets[11] = zstep*3;
        isect1Ptr = isect1;
        isect2Ptr = isect1 + xdim*ydim*3;
        }
      else
        { 
        offsets[8] = (-zstep - xdim)*3;
        offsets[9] = (-zstep - xdim)*3 + 1;
        offsets[10] = (-zstep - xdim)*3 + 4;
        offsets[11] = -zstep*3;
        isect1Ptr = isect1 + xdim*ydim*3;
        isect2Ptr = isect1;
        }

      inPtrY = inPtrZ;
      for (j = yMin; j <= yMax; j++)
        {
        // Should not impact perfomance here/
        edgePtId = (j-inExt[2])*yInc + (k-inExt[4])*zInc;
        // Increments are different for cells.  Since the cells are not
        // contoured until the second row of templates, subtract 1 from
        // i,j,and k.  Note: first cube is formed when i=0, j=1, and k=1.
        inCellId = 
          (xMin-inExt[0]) + (inExt[1]-inExt[0])*
          ( (j-inExt[2]-1) + (k-inExt[4]-1)*(inExt[3]-inExt[2]) );

        y = origin[1] + j*spacing[1];
        xz[1] = y;
        s1 = inPtrY;
        v1 = (*s1 < value ? 0 : 1);
        
        inPtrX = inPtrY;
        for (i = xMin; i <= xMax; i++)
          {
          s0 = s1;
          v0 = v1;
          // this flag keeps up from computing gradient for grid point 0 twice.
          g0 = 0;
          if (i < xMax)
            {
            s1 = (inPtrX + xInc);
            v1 = (*s1 < value ? 0 : 1);
            if (v0 ^ v1)
              {
              t = (value - (double)(*s0)) / ((double)(*s1) - (double)(*s0));
              x[0] = origin[0] + spacing[0]*(i+t);
              x[1] = y;
              *isect2Ptr = newPts->InsertNextPoint(x);
              VTK_CSP3PA(i+1,j,k,s1);
              outPD->InterpolateEdge(inPD, *isect2Ptr, edgePtId, edgePtId+1, t);              
              }
            else
              {
              *isect2Ptr = -1;
              }
            }
          if (j < yMax)
            {
            s2 = (inPtrX + yInc);
            v2 = (*s2 < value ? 0 : 1);
            if (v0 ^ v2)
              {
              t = (value - (double)(*s0)) / ((double)(*s2) - (double)(*s0));
              x[0] = origin[0] + spacing[0]*i;
              x[1] = y + spacing[1]*t;
              *(isect2Ptr + 1) = newPts->InsertNextPoint(x);
              VTK_CSP3PA(i,j+1,k,s2);
              outPD->InterpolateEdge(inPD, *(isect2Ptr+1), edgePtId, edgePtId+yInc, t);     
              }
            else
              {
              *(isect2Ptr + 1) = -1;
              }
            }
          if (k < zMax)
            {
            s3 = (inPtrX + zInc);
            v3 = (*s3 < value ? 0 : 1);
            if (v0 ^ v3)
              {
              t = (value - (double)(*s0)) / ((double)(*s3) - (double)(*s0));
              xz[0] = origin[0] + spacing[0]*i;
              xz[2] = z + spacing[2]*t;
              *(isect2Ptr + 2) = newPts->InsertNextPoint(xz);
              VTK_CSP3PA(i,j,k+1,s3);
              outPD->InterpolateEdge(inPD, *(isect2Ptr+2), edgePtId, edgePtId+zInc, t);     
              }
            else
              {
              *(isect2Ptr + 2) = -1;
              }
            }
          // To keep track of ids for interpolating attributes.
          ++edgePtId;
          
          // now add any polys that need to be added
          // basically look at the isect values, 
          // form an index and lookup the polys
          if (j > yMin && i < xMax && k > zMin)
            {
            idx = (v0 ? 4096 : 0);
            idx = idx + (*(isect1Ptr - yisectstep) > -1 ? 2048 : 0);
            idx = idx + (*(isect1Ptr -yisectstep +1) > -1 ? 1024 : 0);
            idx = idx + (*(isect1Ptr -yisectstep +2) > -1 ? 512 : 0);
            idx = idx + (*(isect1Ptr -yisectstep +4) > -1 ? 256 : 0);
            idx = idx + (*(isect1Ptr -yisectstep +5) > -1 ? 128 : 0);
            idx = idx + (*(isect1Ptr) > -1 ? 64 : 0);
            idx = idx + (*(isect1Ptr + 2) > -1 ? 32 : 0);
            idx = idx + (*(isect1Ptr + 5) > -1 ? 16 : 0);
            idx = idx + (*(isect2Ptr -yisectstep) > -1 ? 8 : 0);
            idx = idx + (*(isect2Ptr -yisectstep +1) > -1 ? 4 : 0);
            idx = idx + (*(isect2Ptr -yisectstep +4) > -1 ? 2 : 0);
            idx = idx + (*(isect2Ptr) > -1 ? 1 : 0);
            
            tablePtr = VTK_SYNCHONIZED_TEMPLATES_3D_TABLE_2 
              + VTK_SYNCHONIZED_TEMPLATES_3D_TABLE_1[idx];
            while (*tablePtr != -1)
              {
              ptIds[0] = *(isect1Ptr + offsets[*tablePtr]);
              tablePtr++;
              ptIds[1] = *(isect1Ptr + offsets[*tablePtr]);
              tablePtr++;
              ptIds[2] = *(isect1Ptr + offsets[*tablePtr]);
              tablePtr++;
              outCellId = newPolys->InsertNextCell(3,ptIds);
              outCD->CopyData(inCD, inCellId, outCellId);
              }
            }
          inPtrX += xInc;
          isect2Ptr += 3;
          isect1Ptr += 3;
          // To keep track of ids for copying cell attributes..
          ++inCellId;
          }
        inPtrY += yInc;
        }
      inPtrZ += zInc;
      }
    }
  delete [] isect1;

  if (newScalars)
    {
    // Lets set the name of the scalars here.
    vtkDataArray *inScalars = inPD->GetArray(inputScalars);
    if (inScalars)
      {
      newScalars->SetName(inScalars->GetName());
      }
    idx = output->GetPointData()->AddArray(newScalars);
    output->GetPointData()->SetActiveAttribute(idx, vtkDataSetAttributes::SCALARS);
    newScalars->Delete();
    newScalars = NULL;
    }
  if (newGradients)
    {
    output->GetPointData()->SetVectors(newGradients);
    newGradients->Delete();
    newGradients = NULL;
    }
  if (newNormals)
    {
    output->GetPointData()->SetNormals(newNormals);
    newNormals->Delete();
    newNormals = NULL;
    }
}




//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::SetInputMemoryLimit(
  unsigned long vtkNotUsed(limit) )
{
  vtkErrorMacro( << "This filter no longer supports a memory limit." );
  vtkErrorMacro( << "This filter no longer initiates streaming." );
  vtkErrorMacro( << "Please use a vtkPolyDataStreamer after this filter to achieve similar functionality." );
}


//----------------------------------------------------------------------------
unsigned long vtkSynchronizedTemplates3D::GetInputMemoryLimit()
{
  vtkErrorMacro( << "This filter no longer supports a memory limit." );
  vtkErrorMacro( << "This filter no longer initiates streaming." );
  vtkErrorMacro( << "Please use a vtkPolyDataStreamer after this filter to achieve similar functionality." );

  return 0;
}



//----------------------------------------------------------------------------
//
// Contouring filter specialized for images (or slices from images)
//
void vtkSynchronizedTemplates3D::ThreadedExecute(vtkImageData *data,
                                                 int *exExt, int threadId)
{
  void *ptr;
  vtkPolyData *output;
  vtkDataArray *inScalars;

  vtkDebugMacro(<< "Executing 3D structured contour");
  
  if (this->NumberOfThreads <= 1)
    { // Special case when only one thread (fast, no copy).
    output = this->GetOutput();
    }
  else
    { // For thread saftey, each writes into a separate output which are merged later.
    output = vtkPolyData::New();
    this->Threads[threadId] = output;
    }
  
  if ( exExt[0] >= exExt[1] || exExt[2] >= exExt[3] || exExt[4] >= exExt[5] )
    {
    vtkDebugMacro(<<"3D structured contours requires 3D data");
    return;
    }
  
  //
  // Check data type and execute appropriate function
  //
  inScalars = data->GetPointData()->GetScalars(this->InputScalarsSelection);
  if (inScalars == NULL)
    {
    vtkErrorMacro("No scalars for contouring.");
    return;
    }
  int numComps = inScalars->GetNumberOfComponents();
  
  if (this->ArrayComponent >= numComps)
    {
    vtkErrorMacro("Scalars have " << numComps << " components. "
                  "ArrayComponent must be smaller than " << numComps);
    return;
    }
  
  ptr = data->GetArrayPointerForExtent(inScalars, exExt);
  switch (inScalars->GetDataType())
    {
    vtkTemplateMacro7(ContourImage, this, exExt, data, output, 
                      (VTK_TT *)ptr, threadId, 
                      this->GetInputScalarsSelection());
    }
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::ExecuteInformation()
{
  // Most of this is to compute estimated whole size. Need to
  // convert to update size, and move to another method. The last
  // line which sets the maximum number of pieces of the output is
  // the only necessary thing in here.

  //  vtkImageData *input = this->GetInput();
  //  int *ext, dims[3];
  //  long numPts, numTris;
  //  long sizePt, sizeTri;

  //  // swag at the output memory size
  //  // Outside surface.
  //  ext = input->GetWholeExtent();
  //  dims[0] = ext[1] - ext[0] + 1;
  //  dims[1] = ext[3] - ext[2] + 1;
  //  dims[2] = ext[5] - ext[4] + 1;
  //  numPts = 2 * (dims[0]*dims[1] + dims[0]*dims[2] + dims[1]*dims[2]);
  //  numTris = numPts * 2;
  //  // Determine the memory for each point and triangle.
  //  sizeTri = 4 * sizeof(int);
  //  sizePt = 3 * sizeof(double);
  //  if (this->ComputeNormals)
  //    {
  //    sizePt += 3 * sizeof(double);
  //    }
  //  if (this->ComputeGradients)
  //    {
  //    sizePt += 3 * sizeof(double);
  //    }
  //  if (this->ComputeScalars)
  //    {
  //    sizePt += sizeof(double);
  //    }
  //  // Set the whole output estimated memory size in kBytes.
  //  // be careful not to overflow.
  //  numTris = numTris / 1000;
  //  if (numTris == 0)
  //    {
  //    numTris = 1;
  //    }
  //  numPts = numPts / 1000;
  //  if (numPts == 0)
  //    {
  //    numPts = 1;
  //    }
  //  //  this->GetOutput()->SetEstimatedWholeMemorySize(
  //  //    numTris*sizeTri + numPts*sizePt);
}



//----------------------------------------------------------------------------
VTK_THREAD_RETURN_TYPE vtkSyncTempThreadedExecute( void *arg )
{
  vtkSynchronizedTemplates3D *self;
  int threadId, threadCount;
  int ext[6], *tmp;
  
  threadId = ((vtkMultiThreader::ThreadInfo *)(arg))->ThreadID;
  threadCount = ((vtkMultiThreader::ThreadInfo *)(arg))->NumberOfThreads;
  self = (vtkSynchronizedTemplates3D *)
            (((vtkMultiThreader::ThreadInfo *)(arg))->UserData);


  // we need to breakup the ExecuteExtent based on the threadId/Count
  tmp = self->GetExecuteExtent();
  ext[0] = tmp[0];
  ext[1] = tmp[1];
  ext[2] = tmp[2];
  ext[3] = tmp[3];
  ext[4] = tmp[4];
  ext[5] = tmp[5];
  
  vtkExtentTranslator *translator = self->GetInput()->GetExtentTranslator();
  if (translator == NULL)
    {
    // No translator means only do one thread.
    if (threadId == 0)
      {
      self->ThreadedExecute(self->GetInput(), ext, threadId);
      }
    }
  else
    {
    if (translator->PieceToExtentThreadSafe(threadId, threadCount,0,tmp, ext, 
                                            translator->GetSplitMode(),0))
      {
      self->ThreadedExecute(self->GetInput(), ext, threadId);
      }
    }
  
  return VTK_THREAD_RETURN_VALUE;
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::Execute()
{
  int idx, inId, outId, offset, num, ptIdx, newIdx;
  vtkIdType numCellPts=0;
  vtkIdType newCellPts[3], *cellPts=0;
  vtkPolyData *output = this->GetOutput();
  vtkPointData *outPD;
  vtkCellData *outCD;
  vtkPolyData *threadOut = 0;
  vtkPointData *threadPD;
  vtkCellData *threadCD;
  vtkCellArray *threadTris;

  // Just in case some one changed the maximum number of threads.
  if (this->NumberOfThreads <= 1)
    {
    // Just call the threaded execute directly.
    this->ThreadedExecute(this->GetInput(), this->ExecuteExtent, 0);
    }
  else
    {
    int totalCells, totalPoints;
    vtkPoints *newPts;
    vtkCellArray *newPolys;
    
    this->Threader->SetNumberOfThreads(this->NumberOfThreads);
    // Setup threading and the invoke threadedExecute
    this->Threader->SetSingleMethod(vtkSyncTempThreadedExecute, this);
    this->Threader->SingleMethodExecute();

    // Collect all the data into the output.  Now I cannot use append filter
    // because this filter might be streaming.  (Maybe I could if thread
    // 0 wrote to output, and I copied output to a temp polyData...)
    
    // Determine the total number of points.
    totalCells = totalPoints = 0;
    for (idx = 0; idx < this->NumberOfThreads; ++idx)
      {
      threadOut = this->Threads[idx];
      if (threadOut != NULL)
        {
        totalPoints += threadOut->GetNumberOfPoints();
        totalCells += threadOut->GetNumberOfCells();
        }
      }
    // Allocate the necessary points and polys
    newPts = vtkPoints::New();
    newPts->Allocate(totalPoints, 1000);
    newPolys = vtkCellArray::New();
    newPolys->Allocate(newPolys->EstimateSize(totalCells, 3));
    output->SetPoints(newPts);
    output->SetPolys(newPolys);
  
    // Allocate point data for copying.
    // Could anything bad happen if the piece happens to be empty?
    vtkDataSetAttributes::FieldList ptList(this->NumberOfThreads);
    int firstPD=1;
    for (idx = 0; idx < this->NumberOfThreads; ++idx)
      {
        if ( !this->Threads[idx])
        {
        continue; //no input, just skip
        }

      threadPD = this->Threads[idx]->GetPointData();
      
      if ( firstPD )
        {
        ptList.InitializeFieldList(threadPD);
        firstPD = 0;
        }
      else
        {
        ptList.IntersectFieldList(threadPD);
        }
      }

    vtkDataSetAttributes::FieldList clList(this->NumberOfThreads);
    int firstCD=1;

    for (idx = 0; idx < this->NumberOfThreads; ++idx)
      {
      if ( !this->Threads[idx])
        {
        continue; //no input, just skip
        }

      threadCD = this->Threads[idx]->GetCellData();
      
      if ( firstCD )
        {
        clList.InitializeFieldList(threadCD);
        firstCD = 0;
        }
      else
        {
        clList.IntersectFieldList(threadCD);
        }
      }


    outPD = output->GetPointData();
    outPD->CopyAllocate(ptList, totalPoints);
    outCD = output->GetCellData();
    outCD->CopyAllocate(clList, totalCells);

    // Now copy all.
    for (idx = 0; idx < this->NumberOfThreads; ++idx)
      {
      threadOut = this->Threads[idx];
      // Sanity check? We should never have a null thread output.
      if (threadOut != NULL)
        { 
        offset = output->GetNumberOfPoints();
        threadPD = threadOut->GetPointData();
        threadCD = threadOut->GetCellData();
        num = threadOut->GetNumberOfPoints();
        for (ptIdx = 0; ptIdx < num; ++ptIdx)
          {
          newIdx = ptIdx + offset;
          newPts->InsertPoint(newIdx, threadOut->GetPoint(ptIdx));
          outPD->CopyData(ptList,threadPD,idx,ptIdx,newIdx);
          }
        // copy the triangles.
        threadTris = threadOut->GetPolys();
        threadTris->InitTraversal();
        inId = 0;
        while (threadTris->GetNextCell(numCellPts, cellPts))
          {
          // copy and translate
          if (numCellPts == 3)
            {
            newCellPts[0] = cellPts[0] + offset;
            newCellPts[1] = cellPts[1] + offset;
            newCellPts[2] = cellPts[2] + offset;
            outId = newPolys->InsertNextCell(3, newCellPts); 
            outCD->CopyData(clList,threadCD, idx, inId, outId);
            }
          ++inId;
          }
        threadOut->Delete();
        threadOut = this->Threads[idx] = NULL;         
        }
      }
    newPolys->Delete();
    newPts->Delete();
    }

  output->Squeeze();
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::ComputeInputUpdateExtents(vtkDataObject *out)
{
  vtkImageData *input = this->GetInput();
  vtkPolyData *output = (vtkPolyData *)out;
  int piece, numPieces, ghostLevel;
  int *wholeExt;
  int ext[6];
  vtkExtentTranslator *translator;

  if (input == NULL)
    {
    vtkErrorMacro("Input not set");
    return;
    }
  translator = input->GetExtentTranslator();

  wholeExt = input->GetWholeExtent();

  // Get request from output
  output->GetUpdateExtent(piece, numPieces, ghostLevel);

  // Start with the whole grid.
  input->GetWholeExtent(ext);  

  // get the extent associated with the piece.
  if (translator == NULL)
    {
    // Default behavior
    if (piece != 0)
      {
      ext[0] = ext[2] = ext[4] = 0;
      ext[1] = ext[3] = ext[5] = -1;
      }
    }
  else
    {    
    translator->PieceToExtentThreadSafe(piece, numPieces, 0, wholeExt, ext, 
                                        translator->GetSplitMode(),0);
    }
  
  // As a side product of this call, ExecuteExtent is set.
  // This is the region that we are really updating, although
  // we may require a larger input region in order to generate
  // it if normals / gradients are being computed
  
  this->ExecuteExtent[0] = ext[0];
  this->ExecuteExtent[1] = ext[1];
  this->ExecuteExtent[2] = ext[2];
  this->ExecuteExtent[3] = ext[3];
  this->ExecuteExtent[4] = ext[4];
  this->ExecuteExtent[5] = ext[5];

  // expand if we need to compute gradients
  if (this->ComputeGradients || this->ComputeNormals)
    {
    ext[0] -= 1;
    if (ext[0] < wholeExt[0])
      {
      ext[0] = wholeExt[0];
      }
    ext[1] += 1;
    if (ext[1] > wholeExt[1])
      {
      ext[1] = wholeExt[1];
      }

    ext[2] -= 1;
    if (ext[2] < wholeExt[2])
      {
      ext[2] = wholeExt[2];
      }
    ext[3] += 1;
    if (ext[3] > wholeExt[3])
      {
      ext[3] = wholeExt[3];
      }

    ext[4] -= 1;
    if (ext[4] < wholeExt[4])
      {
      ext[4] = wholeExt[4];
      }
    ext[5] += 1;
    if (ext[5] > wholeExt[5])
      {
      ext[5] = wholeExt[5];
      }
    }

  // Set the update extent of the input.
  input->SetUpdateExtent(ext);
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::SetInput(vtkImageData *input)
{
  this->vtkProcessObject::SetNthInput(0, input);
}

//----------------------------------------------------------------------------
vtkImageData *vtkSynchronizedTemplates3D::GetInput()
{
  if (this->NumberOfInputs < 1)
    {
    return NULL;
    }
  
  return (vtkImageData *)(this->Inputs[0]);
}

//----------------------------------------------------------------------------
int vtkSynchronizedTemplates3D::FillInputPortInformation(int port,
                                                         vtkInformation* info)
{
  if(!this->Superclass::FillInputPortInformation(port, info))
    {
    return 0;
    }
  info->Set(vtkInformation::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates3D::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  this->ContourValues->PrintSelf(os,indent);

  os << indent << "Compute Normals: " << (this->ComputeNormals ? "On\n" : "Off\n");
  os << indent << "Compute Gradients: " << (this->ComputeGradients ? "On\n" : "Off\n");
  os << indent << "Compute Scalars: " << (this->ComputeScalars ? "On\n" : "Off\n");
  os << indent << "Number Of Threads: " << this->NumberOfThreads << "\n";
  if (this->InputScalarsSelection)
    {
    os << indent << "InputScalarsSelection: " 
       << this->InputScalarsSelection << endl;
    }  
  os << indent << "ArrayComponent: " << this->ArrayComponent << endl;
}


// template table.

int VTK_SYNCHONIZED_TEMPLATES_3D_TABLE_1[] = {
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  592, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1312,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  585,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  260,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  948,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  935,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  250,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  620,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,   16,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  530,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1263,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  988,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  288,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  201,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  874,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  106,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  746,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1119,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 404,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  414,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1126,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  736,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,   99,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  786,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 134,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  355,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1064, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1172,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  448,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,   62,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  687,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1211,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 484,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
  44,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  660, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  822,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  161,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  328,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1028,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  441,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1162,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  700,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,   72,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 124,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  773, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1080, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  368,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1018, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  315,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  174,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  838,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  647,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,   34,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  494,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1218,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  278,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  975,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  890, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  214,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   9,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  610, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1276,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  540,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1011,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  305,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  181,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  848,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  637,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,   27,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  504,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1231,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  271,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  965, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  903,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  224,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    5,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  603,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1292, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  553,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1201, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  471,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,   48,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  667,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  809,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 151,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  335,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1038, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  431,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1149,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  710,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,   79,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  117,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  763,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1093,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  378,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  110,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  753,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1109,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 391,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  421,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1136,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  723,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,   89,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  796,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
 141,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,  345,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1051, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1185,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  458,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,   55,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  677,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    1,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,  596,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1299,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  569,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  264,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  955,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,  919, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  237,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,  627,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
  20,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,  517,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1247,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,  998,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,  295,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,  191,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,  861,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 2036,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1316,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2040,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2404,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1641, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1648,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2408,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1993, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2696,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2080,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1344, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1592,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2361,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2460,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1688,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2594,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1837, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1470, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2218,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2211,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1460,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1850,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2598,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1776,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2557,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2270,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1510,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1411,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2168,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2632,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1896,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1384, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2132,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2662,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1941,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1740,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2512,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2315,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1546, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2184,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1424,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1886,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2625,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2564,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1792,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1497,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2254,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1562,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2322,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2499,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1724, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1954,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2666,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2116,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1371,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2374,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1602,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1675,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2444,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2700,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 2000,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1334,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2067,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1572,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2335,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2489,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1711,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1967, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2676,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2103,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1361, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2384,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1615,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1665,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2431,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2707,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 2016,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1327,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2057,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1391,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2142,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2655,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1925,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1750,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2525,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2299,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1533,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2191,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1434,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1873,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2615,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2574,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1805,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1487,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2241,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2587,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1821, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 1477, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2228,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2201,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1447,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1860,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2605,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1763,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2541,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2283,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1520,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1401,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2155,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2642,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1909,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
2711,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0, 2029, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1320,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2047,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2391,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1625,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0, 1655,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2418,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1977,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0, 2683,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0, 2090,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0, 1351,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0, 1579,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0, 2345,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0,    0, 2473,    0,    0,    0,    0,    0,    0,    0, 
   0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 
   0, 1698 };


int VTK_SYNCHONIZED_TEMPLATES_3D_TABLE_2[] = {
 -1,   0,   1,   2,  -1,   0,   4,   3,  -1,   3,   1,   2, 
  4,   3,   2,  -1,   3,   7,   5,  -1,   0,   1,   2,   3, 
  7,   5,  -1,   4,   7,   5,   0,   4,   5,  -1,   5,   1, 
  2,   5,   2,   7,   7,   2,   4,  -1,   1,   5,   6,  -1, 
  0,   5,   6,   2,   0,   6,  -1,   3,   0,   4,   5,   6, 
  1,  -1,   3,   5,   6,   3,   6,   4,   4,   6,   2,  -1, 
  1,   3,   7,   6,   1,   7,  -1,   0,   3,   7,   0,   7, 
  2,   2,   7,   6,  -1,   1,   0,   4,   1,   4,   6,   6, 
  4,   7,  -1,   4,   7,   2,   7,   6,   2,  -1,   8,   2, 
  9,  -1,   8,   0,   1,   9,   8,   1,  -1,   0,   4,   3, 
  2,   9,   8,  -1,   8,   4,   3,   8,   3,   9,   9,   3, 
  1,  -1,   3,   7,   5,   2,   9,   8,  -1,   1,   9,   8, 
  1,   8,   0,   3,   7,   5,  -1,   4,   7,   5,   4,   5, 
  0,   2,   9,   8,  -1,   5,   4,   7,   5,   9,   4,   5, 
  1,   9,   9,   8,   4,  -1,   2,   9,   8,   1,   5,   6, 
 -1,   6,   9,   8,   6,   8,   5,   5,   8,   0,  -1,   4, 
  3,   0,   2,   9,   8,   5,   6,   1,  -1,   8,   6,   9, 
  4,   6,   8,   4,   5,   6,   4,   3,   5,  -1,   1,   3, 
  7,   1,   7,   6,   9,   8,   2,  -1,   3,   7,   6,   3, 
  6,   8,   3,   8,   0,   9,   8,   6,  -1,   8,   2,   9, 
  4,   6,   0,   4,   7,   6,   6,   1,   0,  -1,   8,   6, 
  9,   8,   4,   6,   4,   7,   6,  -1,   4,   8,  10,  -1, 
  4,   8,  10,   0,   1,   2,  -1,   0,   8,  10,   3,   0, 
 10,  -1,   2,   8,  10,   2,  10,   1,   1,  10,   3,  -1, 
  3,   7,   5,   4,   8,  10,  -1,   1,   2,   0,   3,   7, 
  5,   8,  10,   4,  -1,  10,   7,   5,  10,   5,   8,   8, 
  5,   0,  -1,   5,  10,   7,   1,  10,   5,   1,   8,  10, 
  1,   2,   8,  -1,   4,   8,  10,   5,   6,   1,  -1,   0, 
  5,   6,   0,   6,   2,   8,  10,   4,  -1,   0,   8,  10, 
  0,  10,   3,   5,   6,   1,  -1,   5,  10,   3,   5,   2, 
 10,   5,   6,   2,   8,  10,   2,  -1,   7,   6,   1,   7, 
  1,   3,   4,   8,  10,  -1,   8,  10,   4,   0,   3,   2, 
  2,   3,   7,   2,   7,   6,  -1,  10,   0,   8,  10,   6, 
  0,  10,   7,   6,   6,   1,   0,  -1,  10,   2,   8,  10, 
  7,   2,   7,   6,   2,  -1,   4,   2,   9,  10,   4,   9, 
 -1,   4,   0,   1,   4,   1,  10,  10,   1,   9,  -1,   0, 
  2,   9,   0,   9,   3,   3,   9,  10,  -1,   3,   1,  10, 
  1,   9,  10,  -1,   4,   2,   9,   4,   9,  10,   7,   5, 
  3,  -1,   7,   5,   3,   4,   0,  10,  10,   0,   1,  10, 
  1,   9,  -1,   2,   5,   0,   2,  10,   5,   2,   9,  10, 
  7,   5,  10,  -1,   5,  10,   7,   5,   1,  10,   1,   9, 
 10,  -1,   9,  10,   4,   9,   4,   2,   1,   5,   6,  -1, 
  4,   9,  10,   4,   5,   9,   4,   0,   5,   5,   6,   9, 
 -1,   5,   6,   1,   0,   2,   3,   3,   2,   9,   3,   9, 
 10,  -1,   6,   3,   5,   6,   9,   3,   9,  10,   3,  -1, 
  4,   2,  10,   2,   9,  10,   7,   1,   3,   7,   6,   1, 
 -1,  10,   0,   9,  10,   4,   0,   9,   0,   6,   3,   7, 
  0,   6,   0,   7,  -1,   6,   0,   7,   6,   1,   0,   7, 
  0,  10,   2,   9,   0,  10,   0,   9,  -1,   6,  10,   7, 
  9,  10,   6,  -1,   7,  10,  11,  -1,   0,   1,   2,  10, 
 11,   7,  -1,   4,   3,   0,  10,  11,   7,  -1,   3,   1, 
  2,   3,   2,   4,  10,  11,   7,  -1,   3,  10,  11,   5, 
  3,  11,  -1,   3,  10,  11,   3,  11,   5,   1,   2,   0, 
 -1,   4,  10,  11,   4,  11,   0,   0,  11,   5,  -1,  10, 
  2,   4,  10,   5,   2,  10,  11,   5,   1,   2,   5,  -1, 
  5,   6,   1,   7,  10,  11,  -1,   6,   2,   0,   6,   0, 
  5,   7,  10,  11,  -1,   0,   4,   3,   5,   6,   1,  10, 
 11,   7,  -1,  10,  11,   7,   3,   5,   4,   4,   5,   6, 
  4,   6,   2,  -1,  11,   6,   1,  11,   1,  10,  10,   1, 
  3,  -1,   0,   6,   2,   0,  10,   6,   0,   3,  10,  10, 
 11,   6,  -1,   1,  11,   6,   0,  11,   1,   0,  10,  11, 
  0,   4,  10,  -1,  11,   4,  10,  11,   6,   4,   6,   2, 
  4,  -1,  10,  11,   7,   8,   2,   9,  -1,   8,   0,   1, 
  8,   1,   9,  11,   7,  10,  -1,   3,   0,   4,  10,  11, 
  7,   2,   9,   8,  -1,   7,  10,  11,   3,   9,   4,   3, 
  1,   9,   9,   8,   4,  -1,  11,   5,   3,  11,   3,  10, 
  8,   2,   9,  -1,   3,  10,   5,  10,  11,   5,   1,   8, 
  0,   1,   9,   8,  -1,   2,   9,   8,   4,  10,   0,   0, 
 10,  11,   0,  11,   5,  -1,   9,   4,   1,   9,   8,   4, 
  1,   4,   5,  10,  11,   4,   5,   4,  11,  -1,   1,   5, 
  6,   9,   8,   2,   7,  10,  11,  -1,  10,  11,   7,   8, 
  5,   9,   8,   0,   5,   5,   6,   9,  -1,   0,   4,   3, 
  8,   2,   9,   5,   6,   1,  10,  11,   7,  -1,   4,   3, 
  5,   4,   5,   6,   4,   6,   8,   9,   8,   6,  10,  11, 
  7,  -1,   2,   9,   8,   1,  10,   6,   1,   3,  10,  10, 
 11,   6,  -1,  10,   6,   3,  10,  11,   6,   3,   6,   0, 
  9,   8,   6,   0,   6,   8,  -1,   0,   4,  10,   0,  10, 
 11,   0,  11,   1,   6,   1,  11,   2,   9,   8,  -1,  11, 
  4,  10,  11,   6,   4,   8,   4,   9,   9,   4,   6,  -1, 
  7,   4,   8,  11,   7,   8,  -1,   8,  11,   7,   8,   7, 
  4,   0,   1,   2,  -1,   7,   3,   0,   7,   0,  11,  11, 
  0,   8,  -1,   2,   3,   1,   2,  11,   3,   2,   8,  11, 
 11,   7,   3,  -1,   3,   4,   8,   3,   8,   5,   5,   8, 
 11,  -1,   1,   2,   0,   3,   4,   5,   5,   4,   8,   5, 
  8,  11,  -1,   0,   8,   5,   8,  11,   5,  -1,   2,   5, 
  1,   2,   8,   5,   8,  11,   5,  -1,   7,   4,   8,   7, 
  8,  11,   6,   1,   5,  -1,   0,   5,   2,   5,   6,   2, 
  8,   7,   4,   8,  11,   7,  -1,   1,   5,   6,   0,  11, 
  3,   0,   8,  11,  11,   7,   3,  -1,  11,   3,   8,  11, 
  7,   3,   8,   3,   2,   5,   6,   3,   2,   3,   6,  -1, 
  4,   8,  11,   4,  11,   1,   4,   1,   3,   6,   1,  11, 
 -1,   2,   3,   6,   2,   0,   3,   6,   3,  11,   4,   8, 
  3,  11,   3,   8,  -1,   1,  11,   6,   1,   0,  11,   0, 
  8,  11,  -1,  11,   2,   8,   6,   2,  11,  -1,   9,  11, 
  7,   9,   7,   2,   2,   7,   4,  -1,   0,   1,   9,   0, 
  9,   7,   0,   7,   4,  11,   7,   9,  -1,   7,   9,  11, 
  3,   9,   7,   3,   2,   9,   3,   0,   2,  -1,   7,   9, 
 11,   7,   3,   9,   3,   1,   9,  -1,   3,  11,   5,   3, 
  2,  11,   3,   4,   2,   2,   9,  11,  -1,   5,   4,  11, 
  5,   3,   4,  11,   4,   9,   0,   1,   4,   9,   4,   1, 
 -1,   9,   0,   2,   9,  11,   0,  11,   5,   0,  -1,   9, 
  5,   1,  11,   5,   9,  -1,   5,   6,   1,   7,   2,  11, 
  7,   4,   2,   2,   9,  11,  -1,   5,   9,   0,   5,   6, 
  9,   0,   9,   4,  11,   7,   9,   4,   9,   7,  -1,   3, 
  0,   2,   3,   2,   9,   3,   9,   7,  11,   7,   9,   5, 
  6,   1,  -1,   6,   3,   5,   6,   9,   3,   7,   3,  11, 
 11,   3,   9,  -1,   2,  11,   4,   2,   9,  11,   4,  11, 
  3,   6,   1,  11,   3,  11,   1,  -1,   0,   3,   4,   6, 
  9,  11,  -1,   9,   0,   2,   9,  11,   0,   1,   0,   6, 
  6,   0,  11,  -1,   9,  11,   6,  -1,   9,   6,  11,  -1, 
  1,   2,   0,   6,  11,   9,  -1,   0,   4,   3,   6,  11, 
  9,  -1,   2,   4,   3,   2,   3,   1,   6,  11,   9,  -1, 
  7,   5,   3,  11,   9,   6,  -1,   3,   7,   5,   1,   2, 
  0,  11,   9,   6,  -1,   5,   0,   4,   5,   4,   7,  11, 
  9,   6,  -1,  11,   9,   6,   5,   1,   7,   7,   1,   2, 
  7,   2,   4,  -1,   9,   1,   5,  11,   9,   5,  -1,   9, 
  2,   0,   9,   0,  11,  11,   0,   5,  -1,   5,  11,   9, 
  5,   9,   1,   0,   4,   3,  -1,   3,   5,  11,   3,  11, 
  2,   3,   2,   4,   2,  11,   9,  -1,   7,  11,   9,   7, 
  9,   3,   3,   9,   1,  -1,   7,  11,   9,   3,   7,   9, 
  3,   9,   2,   3,   2,   0,  -1,   0,   9,   1,   0,   7, 
  9,   0,   4,   7,  11,   9,   7,  -1,   9,   7,  11,   9, 
  2,   7,   2,   4,   7,  -1,  11,   8,   2,   6,  11,   2, 
 -1,   1,   6,  11,   1,  11,   0,   0,  11,   8,  -1,   2, 
  6,  11,   2,  11,   8,   4,   3,   0,  -1,   4,  11,   8, 
  4,   1,  11,   4,   3,   1,   6,  11,   1,  -1,  11,   8, 
  2,  11,   2,   6,   5,   3,   7,  -1,   3,   7,   5,   1, 
  6,   0,   0,   6,  11,   0,  11,   8,  -1,   8,   2,   6, 
  8,   6,  11,   0,   4,   5,   5,   4,   7,  -1,   7,   1, 
  4,   7,   5,   1,   4,   1,   8,   6,  11,   1,   8,   1, 
 11,  -1,   2,   1,   5,   2,   5,   8,   8,   5,  11,  -1, 
  0,   5,   8,   8,   5,  11,  -1,   3,   0,   4,   5,   8, 
  1,   5,  11,   8,   8,   2,   1,  -1,   3,   8,   4,   3, 
  5,   8,   5,  11,   8,  -1,   2,   1,   3,   2,   3,  11, 
  2,  11,   8,  11,   3,   7,  -1,   7,   0,   3,   7,  11, 
  0,  11,   8,   0,  -1,   8,   1,  11,   8,   2,   1,  11, 
  1,   7,   0,   4,   1,   7,   1,   4,  -1,   7,   8,   4, 
 11,   8,   7,  -1,   8,  10,   4,   9,   6,  11,  -1,   0, 
  1,   2,   8,  10,   4,   6,  11,   9,  -1,  10,   3,   0, 
 10,   0,   8,   9,   6,  11,  -1,   6,  11,   9,   2,   8, 
  1,   1,   8,  10,   1,  10,   3,  -1,   4,   8,  10,   7, 
  5,   3,   9,   6,  11,  -1,  11,   9,   6,   3,   7,   5, 
  0,   1,   2,   8,  10,   4,  -1,   9,   6,  11,  10,   7, 
  8,   8,   7,   5,   8,   5,   0,  -1,   1,   2,   8,   1, 
  8,  10,   1,  10,   5,   7,   5,  10,   6,  11,   9,  -1, 
  9,   1,   5,   9,   5,  11,  10,   4,   8,  -1,   4,   8, 
 10,   0,  11,   2,   0,   5,  11,  11,   9,   2,  -1,   1, 
  5,  11,   1,  11,   9,   3,   0,  10,  10,   0,   8,  -1, 
 11,   2,   5,  11,   9,   2,   5,   2,   3,   8,  10,   2, 
  3,   2,  10,  -1,   4,   8,  10,   7,  11,   3,   3,  11, 
  9,   3,   9,   1,  -1,   3,   7,  11,   3,  11,   9,   3, 
  9,   0,   2,   0,   9,   4,   8,  10,  -1,   8,   7,   0, 
  8,  10,   7,   0,   7,   1,  11,   9,   7,   1,   7,   9, 
 -1,   9,   7,  11,   9,   2,   7,  10,   7,   8,   8,   7, 
  2,  -1,  11,  10,   4,  11,   4,   6,   6,   4,   2,  -1, 
  1,   6,  11,   0,   1,  11,   0,  11,  10,   0,  10,   4, 
 -1,   0,   2,   6,   0,   6,  10,   0,  10,   3,  10,   6, 
 11,  -1,  11,   1,   6,  11,  10,   1,  10,   3,   1,  -1, 
  3,   7,   5,   4,   6,  10,   4,   2,   6,   6,  11,  10, 
 -1,   0,   1,   6,   0,   6,  11,   0,  11,   4,  10,   4, 
 11,   3,   7,   5,  -1,   6,  10,   2,   6,  11,  10,   2, 
 10,   0,   7,   5,  10,   0,  10,   5,  -1,  11,   1,   6, 
 11,  10,   1,   5,   1,   7,   7,   1,  10,  -1,  10,   4, 
  2,  10,   2,   5,  10,   5,  11,   1,   5,   2,  -1,   4, 
 11,  10,   4,   0,  11,   0,   5,  11,  -1,   3,   2,  10, 
  3,   0,   2,  10,   2,  11,   1,   5,   2,  11,   2,   5, 
 -1,   3,  11,  10,   5,  11,   3,  -1,   3,  11,   1,   3, 
  7,  11,   1,  11,   2,  10,   4,  11,   2,  11,   4,  -1, 
  7,   0,   3,   7,  11,   0,   4,   0,  10,  10,   0,  11, 
 -1,   0,   2,   1,  10,   7,  11,  -1,   7,  11,  10,  -1, 
  6,   7,  10,   9,   6,  10,  -1,   6,   7,  10,   6,  10, 
  9,   2,   0,   1,  -1,  10,   9,   6,  10,   6,   7,   3, 
  0,   4,  -1,   7,  10,   9,   7,   9,   6,   4,   3,   2, 
  2,   3,   1,  -1,   6,   5,   3,   6,   3,   9,   9,   3, 
 10,  -1,   0,   1,   2,   3,   9,   5,   3,  10,   9,   9, 
  6,   5,  -1,   4,  10,   9,   4,   9,   5,   4,   5,   0, 
  5,   9,   6,  -1,   9,   5,  10,   9,   6,   5,  10,   5, 
  4,   1,   2,   5,   4,   5,   2,  -1,   5,   7,  10,   5, 
 10,   1,   1,  10,   9,  -1,   2,   0,   5,   2,   5,  10, 
  2,  10,   9,   7,  10,   5,  -1,   4,   3,   0,  10,   1, 
  7,  10,   9,   1,   1,   5,   7,  -1,   4,   5,   2,   4, 
  3,   5,   2,   5,   9,   7,  10,   5,   9,   5,  10,  -1, 
  3,  10,   1,   1,  10,   9,  -1,   0,   9,   2,   0,   3, 
  9,   3,  10,   9,  -1,   4,   1,   0,   4,  10,   1,  10, 
  9,   1,  -1,   4,   9,   2,  10,   9,   4,  -1,  10,   8, 
  2,  10,   2,   7,   7,   2,   6,  -1,  10,   8,   0,  10, 
  0,   6,  10,   6,   7,   6,   0,   1,  -1,   0,   4,   3, 
  2,   7,   8,   2,   6,   7,   7,  10,   8,  -1,   7,   8, 
  6,   7,  10,   8,   6,   8,   1,   4,   3,   8,   1,   8, 
  3,  -1,   5,   3,  10,   5,  10,   2,   5,   2,   6,   8, 
  2,  10,  -1,   0,   6,   8,   0,   1,   6,   8,   6,  10, 
  5,   3,   6,  10,   6,   3,  -1,   0,  10,   5,   0,   4, 
 10,   5,  10,   6,   8,   2,  10,   6,  10,   2,  -1,   4, 
 10,   8,   5,   1,   6,  -1,   5,   7,  10,   1,   5,  10, 
  1,  10,   8,   1,   8,   2,  -1,  10,   5,   7,  10,   8, 
  5,   8,   0,   5,  -1,   1,   5,   7,   1,   7,  10,   1, 
 10,   2,   8,   2,  10,   0,   4,   3,  -1,  10,   5,   7, 
 10,   8,   5,   3,   5,   4,   4,   5,   8,  -1,   2,  10, 
  8,   2,   1,  10,   1,   3,  10,  -1,   0,  10,   8,   3, 
 10,   0,  -1,   2,  10,   8,   2,   1,  10,   4,  10,   0, 
  0,  10,   1,  -1,   4,  10,   8,  -1,   8,   9,   6,   8, 
  6,   4,   4,   6,   7,  -1,   0,   1,   2,   8,   9,   4, 
  4,   9,   6,   4,   6,   7,  -1,   3,   6,   7,   3,   8, 
  6,   3,   0,   8,   9,   6,   8,  -1,   1,   8,   3,   1, 
  2,   8,   3,   8,   7,   9,   6,   8,   7,   8,   6,  -1, 
  8,   9,   6,   4,   8,   6,   4,   6,   5,   4,   5,   3, 
 -1,   4,   8,   9,   4,   9,   6,   4,   6,   3,   5,   3, 
  6,   0,   1,   2,  -1,   6,   8,   9,   6,   5,   8,   5, 
  0,   8,  -1,   6,   8,   9,   6,   5,   8,   2,   8,   1, 
  1,   8,   5,  -1,   5,   7,   4,   5,   4,   9,   5,   9, 
  1,   9,   4,   8,  -1,   4,   9,   7,   4,   8,   9,   7, 
  9,   5,   2,   0,   9,   5,   9,   0,  -1,   1,   7,   9, 
  1,   5,   7,   9,   7,   8,   3,   0,   7,   8,   7,   0, 
 -1,   3,   5,   7,   2,   8,   9,  -1,   8,   3,   4,   8, 
  9,   3,   9,   1,   3,  -1,   8,   3,   4,   8,   9,   3, 
  0,   3,   2,   2,   3,   9,  -1,   8,   1,   0,   9,   1, 
  8,  -1,   8,   9,   2,  -1,   4,   2,   7,   7,   2,   6, 
 -1,   1,   4,   0,   1,   6,   4,   6,   7,   4,  -1,   0, 
  7,   3,   0,   2,   7,   2,   6,   7,  -1,   1,   7,   3, 
  6,   7,   1,  -1,   3,   6,   5,   3,   4,   6,   4,   2, 
  6,  -1,   1,   4,   0,   1,   6,   4,   3,   4,   5,   5, 
  4,   6,  -1,   0,   6,   5,   2,   6,   0,  -1,   1,   6, 
  5,  -1,   5,   2,   1,   5,   7,   2,   7,   4,   2,  -1, 
  4,   5,   7,   0,   5,   4,  -1,   5,   2,   1,   5,   7, 
  2,   0,   2,   3,   3,   2,   7,  -1,   3,   5,   7,  -1, 
  3,   2,   1,   4,   2,   3,  -1,   0,   3,   4,  -1,   0, 
  2,   1,  -1 };


