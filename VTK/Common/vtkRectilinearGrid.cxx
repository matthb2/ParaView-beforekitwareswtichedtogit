/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkRectilinearGrid.h"

#include "vtkCellData.h"
#include "vtkExtentTranslator.h"
#include "vtkDoubleArray.h"
#include "vtkGenericCell.h"
#include "vtkLine.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPixel.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkUnsignedCharArray.h"
#include "vtkVertex.h"
#include "vtkVoxel.h"

vtkCxxRevisionMacro(vtkRectilinearGrid, "$Revision$");
vtkStandardNewMacro(vtkRectilinearGrid);

vtkCxxSetObjectMacro(vtkRectilinearGrid,XCoordinates,vtkDataArray);
vtkCxxSetObjectMacro(vtkRectilinearGrid,YCoordinates,vtkDataArray);
vtkCxxSetObjectMacro(vtkRectilinearGrid,ZCoordinates,vtkDataArray);

//----------------------------------------------------------------------------
vtkRectilinearGrid::vtkRectilinearGrid()
{
  this->Vertex = vtkVertex::New();
  this->Line = vtkLine::New();
  this->Pixel = vtkPixel::New();
  this->Voxel = vtkVoxel::New();
  
  this->Dimensions[0] = 0;
  this->Dimensions[1] = 0;
  this->Dimensions[2] = 0;
  this->Extent[0] = this->Extent[2] = this->Extent[4] = 0;
  this->Extent[1] = this->Extent[3] = this->Extent[5] = -1;
  this->DataDescription = VTK_EMPTY;

  vtkDoubleArray *fs=vtkDoubleArray::New(); fs->Allocate(1);
  fs->SetNumberOfTuples(1);
  fs->SetComponent(0, 0, 0.0);
  this->XCoordinates = fs; fs->Register(this);
  this->YCoordinates = fs; fs->Register(this);
  this->ZCoordinates = fs; fs->Register(this);
  fs->Delete();

  this->PointReturn[0] = 0.0;
  this->PointReturn[1] = 0.0;
  this->PointReturn[2] = 0.0;
}

//----------------------------------------------------------------------------
vtkRectilinearGrid::~vtkRectilinearGrid()
{
  this->Initialize();
  this->Vertex->Delete();
  this->Line->Delete();
  this->Pixel->Delete();
  this->Voxel->Delete();

}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::Initialize()
{
  vtkDataSet::Initialize();
  // Superclass does not know about dimensions ivar.
  this->Dimensions[0] = 0;
  this->Dimensions[1] = 0;
  this->Dimensions[2] = 0;

  if ( this->XCoordinates ) 
    {
    this->XCoordinates->UnRegister(this);
    this->XCoordinates = NULL;
    }

  if ( this->YCoordinates ) 
    {
    this->YCoordinates->UnRegister(this);
    this->YCoordinates = NULL;
    }

  if ( this->ZCoordinates ) 
    {
    this->ZCoordinates->UnRegister(this);
    this->ZCoordinates = NULL;
    }
}

//----------------------------------------------------------------------------
// Copy the geometric and topological structure of an input rectilinear grid
// object.
void vtkRectilinearGrid::CopyStructure(vtkDataSet *ds)
{
  vtkRectilinearGrid *rGrid=(vtkRectilinearGrid *)ds;
  int i;
  this->Initialize();

  for (i=0; i<3; i++)
    {
    this->Dimensions[i] = rGrid->Dimensions[i];
    }
  for (i=0; i<6; i++)
    {
    this->Extent[i] = rGrid->Extent[i];
    }
  this->DataDescription = rGrid->DataDescription;

  this->SetXCoordinates(rGrid->XCoordinates);
  this->SetYCoordinates(rGrid->YCoordinates);
  this->SetZCoordinates(rGrid->ZCoordinates);
}

//----------------------------------------------------------------------------
vtkCell *vtkRectilinearGrid::GetCell(vtkIdType cellId)
{
  vtkCell *cell = NULL;
  vtkIdType idx, npts;
  int loc[3];
  int iMin, iMax, jMin, jMax, kMin, kMax;
  int d01 = this->Dimensions[0]*this->Dimensions[1];
  double x[3];

  iMin = iMax = jMin = jMax = kMin = kMax = 0;

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      //return this->EmptyCell;
      return NULL;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      cell = this->Vertex;
      break;

    case VTK_X_LINE:
      iMin = cellId;
      iMax = cellId + 1;
      cell = this->Line;
      break;

    case VTK_Y_LINE:
      jMin = cellId;
      jMax = cellId + 1;
      cell = this->Line;
      break;

    case VTK_Z_LINE:
      kMin = cellId;
      kMax = cellId + 1;
      cell = this->Line;
      break;

    case VTK_XY_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      cell = this->Pixel;
      break;

    case VTK_YZ_PLANE:
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      cell = this->Pixel;
      break;

    case VTK_XZ_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      cell = this->Pixel;
      break;

    case VTK_XYZ_GRID:
      iMin = cellId % (this->Dimensions[0] - 1);
      iMax = iMin + 1;
      jMin = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      jMax = jMin + 1;
      kMin = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      kMax = kMin + 1;
      cell = this->Voxel;
      break;
    }


  // Extract point coordinates and point ids
  for (npts=0,loc[2]=kMin; loc[2]<=kMax; loc[2]++)
    {
    x[2] = this->ZCoordinates->GetComponent(loc[2], 0);
    for (loc[1]=jMin; loc[1]<=jMax; loc[1]++)
      {
      x[1] = this->YCoordinates->GetComponent(loc[1], 0);
      for (loc[0]=iMin; loc[0]<=iMax; loc[0]++)
        {
        x[0] = this->XCoordinates->GetComponent(loc[0], 0);

        idx = loc[0] + loc[1]*this->Dimensions[0] + loc[2]*d01;
        cell->PointIds->SetId(npts,idx);
        cell->Points->SetPoint(npts++,x);
        }
      }
    }

  return cell;
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::GetCell(vtkIdType cellId, vtkGenericCell *cell)
{
  vtkIdType idx, npts;
  int loc[3];
  int iMin, iMax, jMin, jMax, kMin, kMax;
  int d01 = this->Dimensions[0]*this->Dimensions[1];
  double x[3];

  iMin = iMax = jMin = jMax = kMin = kMax = 0;

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      cell->SetCellTypeToEmptyCell();
      break;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      cell->SetCellTypeToVertex();
      break;

    case VTK_X_LINE:
      iMin = cellId;
      iMax = cellId + 1;
      cell->SetCellTypeToLine();
      break;

    case VTK_Y_LINE:
      jMin = cellId;
      jMax = cellId + 1;
      cell->SetCellTypeToLine();
      break;

    case VTK_Z_LINE:
      kMin = cellId;
      kMax = cellId + 1;
      cell->SetCellTypeToLine();
      break;

    case VTK_XY_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      cell->SetCellTypeToPixel();
      break;

    case VTK_YZ_PLANE:
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      cell->SetCellTypeToPixel();
      break;

    case VTK_XZ_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      cell->SetCellTypeToPixel();
      break;

    case VTK_XYZ_GRID:
      iMin = cellId % (this->Dimensions[0] - 1);
      iMax = iMin + 1;
      jMin = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      jMax = jMin + 1;
      kMin = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      kMax = kMin + 1;
      cell->SetCellTypeToVoxel();
      break;
    }

  // Extract point coordinates and point ids
  for (npts=0,loc[2]=kMin; loc[2]<=kMax; loc[2]++)
    {
    x[2] = this->ZCoordinates->GetComponent(loc[2], 0);
    for (loc[1]=jMin; loc[1]<=jMax; loc[1]++)
      {
      x[1] = this->YCoordinates->GetComponent(loc[1], 0);
      for (loc[0]=iMin; loc[0]<=iMax; loc[0]++)
        {
        x[0] = this->XCoordinates->GetComponent(loc[0], 0);
        idx = loc[0] + loc[1]*this->Dimensions[0] + loc[2]*d01;
        cell->PointIds->SetId(npts,idx);
        cell->Points->SetPoint(npts++,x);
        }
      }
    }
}

//----------------------------------------------------------------------------
// Fast implementation of GetCellBounds().  Bounds are calculated without
// constructing a cell.
void vtkRectilinearGrid::GetCellBounds(vtkIdType cellId, double bounds[6])
{
  int loc[3];
  int iMin, iMax, jMin, jMax, kMin, kMax;
  double x[3];

  iMin = iMax = jMin = jMax = kMin = kMax = 0;

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      return;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      break;

    case VTK_X_LINE:
      iMin = cellId;
      iMax = cellId + 1;
      break;

    case VTK_Y_LINE:
      jMin = cellId;
      jMax = cellId + 1;
      break;

    case VTK_Z_LINE:
      kMin = cellId;
      kMax = cellId + 1;
      break;

    case VTK_XY_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      break;

    case VTK_YZ_PLANE:
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      break;

    case VTK_XZ_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      break;

    case VTK_XYZ_GRID:
      iMin = cellId % (this->Dimensions[0] - 1);
      iMax = iMin + 1;
      jMin = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      jMax = jMin + 1;
      kMin = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      kMax = kMin + 1;
      break;
    }

  // carefully compute the bounds
  if (kMax >= kMin && jMax >= jMin && iMax >= iMin)
    {
    bounds[0] = bounds[2] = bounds[4] =  VTK_DOUBLE_MAX;
    bounds[1] = bounds[3] = bounds[5] = -VTK_DOUBLE_MAX;
  
    // Extract point coordinates
    for (loc[2]=kMin; loc[2]<=kMax; loc[2]++)
      {
      x[2] = this->ZCoordinates->GetComponent(loc[2], 0);
      bounds[4] = (x[2] < bounds[4] ? x[2] : bounds[4]);
      bounds[5] = (x[2] > bounds[5] ? x[2] : bounds[5]);
      }
    for (loc[1]=jMin; loc[1]<=jMax; loc[1]++)
      {
      x[1] = this->YCoordinates->GetComponent(loc[1], 0);
      bounds[2] = (x[1] < bounds[2] ? x[1] : bounds[2]);
      bounds[3] = (x[1] > bounds[3] ? x[1] : bounds[3]);
      }
    for (loc[0]=iMin; loc[0]<=iMax; loc[0]++)
      {
      x[0] = this->XCoordinates->GetComponent(loc[0], 0);
      bounds[0] = (x[0] < bounds[0] ? x[0] : bounds[0]);
      bounds[1] = (x[0] > bounds[1] ? x[0] : bounds[1]);
      }
    }
  else
    {
    vtkMath::UninitializeBounds(bounds);
    }
}


//----------------------------------------------------------------------------
double *vtkRectilinearGrid::GetPoint(vtkIdType ptId)
{
  int loc[3];
  
  switch (this->DataDescription)
    {
    case VTK_EMPTY: 
      this->PointReturn[0] = 0.0;
      this->PointReturn[1] = 0.0;
      this->PointReturn[2] = 0.0;
      vtkErrorMacro("Requesting a point from an empty data set.");
      return this->PointReturn;

    case VTK_SINGLE_POINT: 
      loc[0] = loc[1] = loc[2] = 0;
      break;

    case VTK_X_LINE:
      loc[1] = loc[2] = 0;
      loc[0] = ptId;
      break;

    case VTK_Y_LINE:
      loc[0] = loc[2] = 0;
      loc[1] = ptId;
      break;

    case VTK_Z_LINE:
      loc[0] = loc[1] = 0;
      loc[2] = ptId;
      break;

    case VTK_XY_PLANE:
      loc[2] = 0;
      loc[0] = ptId % this->Dimensions[0];
      loc[1] = ptId / this->Dimensions[0];
      break;

    case VTK_YZ_PLANE:
      loc[0] = 0;
      loc[1] = ptId % this->Dimensions[1];
      loc[2] = ptId / this->Dimensions[1];
      break;

    case VTK_XZ_PLANE:
      loc[1] = 0;
      loc[0] = ptId % this->Dimensions[0];
      loc[2] = ptId / this->Dimensions[0];
      break;

    case VTK_XYZ_GRID:
      loc[0] = ptId % this->Dimensions[0];
      loc[1] = (ptId / this->Dimensions[0]) % this->Dimensions[1];
      loc[2] = ptId / (this->Dimensions[0]*this->Dimensions[1]);
      break;
    }

  this->PointReturn[0] = this->XCoordinates->GetComponent(loc[0], 0);
  this->PointReturn[1] = this->YCoordinates->GetComponent(loc[1], 0);
  this->PointReturn[2] = this->ZCoordinates->GetComponent(loc[2], 0);

  return this->PointReturn;
}

void vtkRectilinearGrid::GetPoint(vtkIdType ptId, double x[3])
{
  int loc[3];
  
  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      vtkErrorMacro("Requesting a point from an empty data set.");
      x[0] = x[1] = x[2] = 0.0;
      return;
       
    case VTK_SINGLE_POINT: 
      loc[0] = loc[1] = loc[2] = 0;
      break;

    case VTK_X_LINE:
      loc[1] = loc[2] = 0;
      loc[0] = ptId;
      break;

    case VTK_Y_LINE:
      loc[0] = loc[2] = 0;
      loc[1] = ptId;
      break;

    case VTK_Z_LINE:
      loc[0] = loc[1] = 0;
      loc[2] = ptId;
      break;

    case VTK_XY_PLANE:
      loc[2] = 0;
      loc[0] = ptId % this->Dimensions[0];
      loc[1] = ptId / this->Dimensions[0];
      break;

    case VTK_YZ_PLANE:
      loc[0] = 0;
      loc[1] = ptId % this->Dimensions[1];
      loc[2] = ptId / this->Dimensions[1];
      break;

    case VTK_XZ_PLANE:
      loc[1] = 0;
      loc[0] = ptId % this->Dimensions[0];
      loc[2] = ptId / this->Dimensions[0];
      break;

    case VTK_XYZ_GRID:
      loc[0] = ptId % this->Dimensions[0];
      loc[1] = (ptId / this->Dimensions[0]) % this->Dimensions[1];
      loc[2] = ptId / (this->Dimensions[0]*this->Dimensions[1]);
      break;
    }

  x[0] = this->XCoordinates->GetComponent(loc[0], 0);
  x[1] = this->YCoordinates->GetComponent(loc[1], 0);
  x[2] = this->ZCoordinates->GetComponent(loc[2], 0);

}

//----------------------------------------------------------------------------
vtkIdType vtkRectilinearGrid::FindPoint(double x[3])
{
  int i, j, loc[3];
  double xPrev, xNext;
  vtkDataArray *scalars[3];

  scalars[0] = this->XCoordinates;
  scalars[1] = this->YCoordinates;
  scalars[2] = this->ZCoordinates;
//
// Find coordinates in x-y-z direction
//
  for ( j=0; j < 3; j++ )
    {
    loc[j] = 0;
    xPrev = scalars[j]->GetComponent(0, 0);
    xNext = scalars[j]->GetComponent(scalars[j]->GetNumberOfTuples()-1, 0);
    if ( x[j] < xPrev || x[j] > xNext )
      {
      return -1;
      }

    for (i=1; i < scalars[j]->GetNumberOfTuples(); i++)
      {
      xNext = scalars[j]->GetComponent(i, 0);
      if ( x[j] >= xPrev && x[j] <= xNext )
        {
        if ( (x[j]-xPrev) < (xNext-x[j]) )
          {
          loc[j] = i-1;
          }
        else
          {
          loc[j] = i;
          }
        }
      xPrev = xNext;
      }
    }
//
//  From this location get the point id
//
  return loc[2]*this->Dimensions[0]*this->Dimensions[1] +
         loc[1]*this->Dimensions[0] + loc[0];
  
}
vtkIdType vtkRectilinearGrid::FindCell(double x[3], vtkCell *vtkNotUsed(cell), 
                                       vtkGenericCell *vtkNotUsed(gencell),
                                       vtkIdType vtkNotUsed(cellId), 
                                       double vtkNotUsed(tol2), 
                                       int& subId, double pcoords[3], 
                                       double *weights)
{
  return
    this->FindCell( x, (vtkCell *)NULL, 0, 0.0, subId, pcoords, weights );
}

//----------------------------------------------------------------------------
vtkIdType vtkRectilinearGrid::FindCell(double x[3], vtkCell *vtkNotUsed(cell), 
                                       vtkIdType vtkNotUsed(cellId),
                                       double vtkNotUsed(tol2), 
                                       int& subId, double pcoords[3],
                                       double *weights)
{
  int loc[3];

  if ( this->ComputeStructuredCoordinates(x, loc, pcoords) == 0 )
    {
    return -1;
    }

  vtkVoxel::InterpolationFunctions(pcoords,weights);

//
//  From this location get the cell id
//
  subId = 0;
  return loc[2] * (this->Dimensions[0]-1)*(this->Dimensions[1]-1) +
         loc[1] * (this->Dimensions[0]-1) + loc[0];
}

//----------------------------------------------------------------------------
vtkCell *vtkRectilinearGrid::FindAndGetCell(double x[3],
                                            vtkCell *vtkNotUsed(cell), 
                                            vtkIdType vtkNotUsed(cellId),
                                            double vtkNotUsed(tol2),
                                            int& subId, 
                                            double pcoords[3], double *weights)
{
  int loc[3];
  vtkIdType cellId;
  
  subId = 0;
  if ( this->ComputeStructuredCoordinates(x, loc, pcoords) == 0 )
    {
    return NULL;
    }
  //
  // Get the parametric coordinates and weights for interpolation
  //
  vtkVoxel::InterpolationFunctions(pcoords,weights);
  //
  // Get the cell
  //
  cellId = loc[2] * (this->Dimensions[0]-1)*(this->Dimensions[1]-1) +
           loc[1] * (this->Dimensions[0]-1) + loc[0];

  return vtkRectilinearGrid::GetCell(cellId);
}

//----------------------------------------------------------------------------
int vtkRectilinearGrid::GetCellType(vtkIdType vtkNotUsed(cellId))
{
  switch (this->DataDescription)
    {
    case VTK_EMPTY: 
      return VTK_EMPTY_CELL;

    case VTK_SINGLE_POINT: 
      return VTK_VERTEX;

    case VTK_X_LINE: case VTK_Y_LINE: case VTK_Z_LINE:
      return VTK_LINE;

    case VTK_XY_PLANE: case VTK_YZ_PLANE: case VTK_XZ_PLANE:
      return VTK_PIXEL;

    case VTK_XYZ_GRID:
      return VTK_VOXEL;

    default:
      vtkErrorMacro(<<"Bad data description!");
      return VTK_EMPTY_CELL;
    }
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::ComputeBounds()
{
  double tmp;

  if (this->XCoordinates == NULL || this->YCoordinates == NULL || 
      this->ZCoordinates == NULL)
    {
    vtkMath::UninitializeBounds(this->Bounds);
    return;
    }

  if ( this->XCoordinates->GetNumberOfTuples() == 0 || 
       this->YCoordinates->GetNumberOfTuples() == 0 || 
       this->ZCoordinates->GetNumberOfTuples() == 0 )
    {
    vtkMath::UninitializeBounds(this->Bounds);
    return;
    }

  this->Bounds[0] = this->XCoordinates->GetComponent(0, 0);
  this->Bounds[2] = this->YCoordinates->GetComponent(0, 0);
  this->Bounds[4] = this->ZCoordinates->GetComponent(0, 0);

  this->Bounds[1] = this->XCoordinates->GetComponent(
                        this->XCoordinates->GetNumberOfTuples()-1, 0);
  this->Bounds[3] = this->YCoordinates->GetComponent(
                        this->YCoordinates->GetNumberOfTuples()-1, 0);
  this->Bounds[5] = this->ZCoordinates->GetComponent(
                        this->ZCoordinates->GetNumberOfTuples()-1, 0);
  // ensure that the bounds are increasing
  for (int i = 0; i < 5; i += 2)
    {
    if (this->Bounds[i + 1] < this->Bounds[i])
      {
      tmp = this->Bounds[i + 1];
      this->Bounds[i + 1] = this->Bounds[i];
      this->Bounds[i] = tmp;
      }
    }
}

//----------------------------------------------------------------------------
// Set dimensions of rectilinear grid dataset.
void vtkRectilinearGrid::SetDimensions(int i, int j, int k)
{
  this->SetExtent(0, i-1, 0, j-1, 0, k-1);
}

//----------------------------------------------------------------------------
// Set dimensions of rectilinear grid dataset.
void vtkRectilinearGrid::SetDimensions(int dim[3])
{
  this->SetExtent(0, dim[0]-1, 0, dim[1]-1, 0, dim[2]-1);
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::SetExtent(int extent[6])
{
  int description;

  description = vtkStructuredData::SetExtent(extent, this->Extent);
  if ( description < 0 ) //improperly specified
    {
    vtkErrorMacro (<< "Bad Extent, retaining previous values");
    }
  
  if (description == VTK_UNCHANGED)
    {
    return;
    }
  
  this->DataDescription = description;
  
  this->Modified();
  this->Dimensions[0] = extent[1] - extent[0] + 1;
  this->Dimensions[1] = extent[3] - extent[2] + 1;
  this->Dimensions[2] = extent[5] - extent[4] + 1;
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::SetExtent(int xMin, int xMax, int yMin, int yMax,
                                   int zMin, int zMax)
{
  int extent[6];

  extent[0] = xMin; extent[1] = xMax;
  extent[2] = yMin; extent[3] = yMax;
  extent[4] = zMin; extent[5] = zMax;
  
  this->SetExtent(extent);
}

//----------------------------------------------------------------------------
// Convenience function computes the structured coordinates for a point x[3].
// The cell is specified by the array ijk[3], and the parametric coordinates
// in the cell are specified with pcoords[3]. The function returns a 0 if the
// point x is outside of the grid, and a 1 if inside the grid.
int vtkRectilinearGrid::ComputeStructuredCoordinates(double x[3], int ijk[3], 
                                                      double pcoords[3])
{
  int i, j;
  double xPrev, xNext, tmp;
  vtkDataArray *scalars[3];

  scalars[0] = this->XCoordinates;
  scalars[1] = this->YCoordinates;
  scalars[2] = this->ZCoordinates;
  //
  // Find locations in x-y-z direction
  //
  ijk[0] = ijk[1] = ijk[2] = 0;
  pcoords[0] = pcoords[1] = pcoords[2] = 0.0;

  for ( j=0; j < 3; j++ )
    {
    xPrev = scalars[j]->GetComponent(0, 0);
    xNext = scalars[j]->GetComponent(scalars[j]->GetNumberOfTuples()-1, 0);
    if (xNext < xPrev)
      {
      tmp = xNext;
      xNext = xPrev;
      xPrev = tmp;
      }
    if ( x[j] < xPrev || x[j] > xNext )
      {
      return 0;
      }
    if (x[j] == xNext  && this->Dimensions[j] != 1)
      {
      return 0;
      }

    for (i=1; i < scalars[j]->GetNumberOfTuples(); i++)
      {
      xNext = scalars[j]->GetComponent(i, 0);
      if ( x[j] >= xPrev && x[j] < xNext )
        {
        ijk[j] = i - 1;
        pcoords[j] = (x[j]-xPrev) / (xNext-xPrev);
        break;
        }

      else if ( x[j] == xNext )
        {
        ijk[j] = i - 1;
        pcoords[j] = 1.0;
        break;
        }
      xPrev = xNext;
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
// Should we split up cells, or just points.  It does not matter for now.
// Extent of structured data assumes points.
void vtkRectilinearGrid::SetUpdateExtent(int piece, int numPieces,
                                         int ghostLevel)
{
  int ext[6];
  
  this->UpdateInformation();
  this->GetWholeExtent(ext);
  this->ExtentTranslator->SetWholeExtent(ext);
  this->ExtentTranslator->SetPiece(piece);
  this->ExtentTranslator->SetNumberOfPieces(numPieces);
  this->ExtentTranslator->SetGhostLevel(ghostLevel);
  this->ExtentTranslator->PieceToExtent();
  this->SetUpdateExtent(this->ExtentTranslator->GetExtent());

  this->UpdatePiece = piece;
  this->UpdateNumberOfPieces = numPieces;
  this->UpdateGhostLevel = ghostLevel;
}


//----------------------------------------------------------------------------
unsigned long vtkRectilinearGrid::GetActualMemorySize()
{
  unsigned long size=this->vtkDataSet::GetActualMemorySize();

  if ( this->XCoordinates ) 
    {
    size += this->XCoordinates->GetActualMemorySize();
    }

  if ( this->YCoordinates ) 
    {
    size += this->YCoordinates->GetActualMemorySize();
    }

  if ( this->ZCoordinates ) 
    {
    size += this->ZCoordinates->GetActualMemorySize();
    }

  return size;
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::GetCellNeighbors(vtkIdType cellId, vtkIdList *ptIds,
                                          vtkIdList *cellIds)
{
  int numPtIds=ptIds->GetNumberOfIds();

  // Use special methods for speed
  switch (numPtIds)
    {
    case 0:
      cellIds->Reset();
      return;

    case 1: case 2: case 4: //vertex, edge, face neighbors
      vtkStructuredData::GetCellNeighbors(cellId, ptIds, 
                                         cellIds, this->Dimensions);
      break;
      
    default:
      this->vtkDataSet::GetCellNeighbors(cellId, ptIds, cellIds);
    }
}
//----------------------------------------------------------------------------
void vtkRectilinearGrid::ShallowCopy(vtkDataObject *dataObject)
{
  vtkRectilinearGrid *grid = vtkRectilinearGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    this->SetDimensions(grid->GetDimensions());
    this->DataDescription = grid->DataDescription;
    
    this->SetXCoordinates(grid->GetXCoordinates());
    this->SetYCoordinates(grid->GetYCoordinates());
    this->SetZCoordinates(grid->GetZCoordinates());
    }

  // Do superclass
  this->vtkDataSet::ShallowCopy(dataObject);
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::DeepCopy(vtkDataObject *dataObject)
{
  vtkRectilinearGrid *grid = vtkRectilinearGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    vtkDoubleArray *s;
    this->SetDimensions(grid->GetDimensions());
    this->DataDescription = grid->DataDescription;
    
    s = vtkDoubleArray::New();
    s->DeepCopy(grid->GetXCoordinates());
    this->SetXCoordinates(s);
    s->Delete();
    s = vtkDoubleArray::New();
    s->DeepCopy(grid->GetYCoordinates());
    this->SetYCoordinates(s);
    s->Delete();
    s = vtkDoubleArray::New();
    s->DeepCopy(grid->GetZCoordinates());
    this->SetZCoordinates(s);
    s->Delete();
    }

  // Do superclass
  this->vtkDataSet::DeepCopy(dataObject);
}

//----------------------------------------------------------------------------
void vtkRectilinearGrid::Crop()
{
  int i, j, k;
  // What we want.
  int uExt[6];
  // What we have.
  int ext[6];

  // If the update extent is larger than the extent, 
  // we cannot do anything about it here.
  for (i = 0; i < 3; ++i)
    {
    uExt[i*2] = this->UpdateExtent[i*2];
    ext[i*2] = this->Extent[i*2];
    if (uExt[i*2] < ext[i*2])
      {
      uExt[i*2] = ext[i*2];
      }
    uExt[i*2+1] = this->UpdateExtent[i*2+1];
    ext[i*2+1] = this->Extent[i*2+1];
    if (uExt[i*2+1] > ext[i*2+1])
      {
      uExt[i*2+1] = ext[i*2+1];
      }
    }
  
  // If extents already match, then we need to do nothing.
  if (ext[0] == uExt[0] && ext[1] == uExt[1]
      && ext[2] == uExt[2] && ext[3] == uExt[3]
      && ext[4] == uExt[4] && ext[5] == uExt[5])
    {
    return;
    }
  else
    {
    vtkRectilinearGrid *newGrid;
    vtkPointData *inPD, *outPD;
    vtkCellData *inCD, *outCD;
    int outSize, jOffset, kOffset;
    vtkIdType idx, newId;
    int inInc1, inInc2;
    vtkDataArray *coords, *newCoords;

    vtkDebugMacro(<< "Cropping Grid");

    newGrid = vtkRectilinearGrid::New();

    inPD  = this->GetPointData();
    inCD  = this->GetCellData();
    outPD = newGrid->GetPointData();
    outCD = newGrid->GetCellData();

    // Allocate necessary objects
    //
    newGrid->SetExtent(uExt);
    outSize = (uExt[1]-uExt[0]+1)*(uExt[3]-uExt[2]+1)*(uExt[5]-uExt[4]+1);
    outPD->CopyAllocate(inPD,outSize,outSize);
    outCD->CopyAllocate(inCD,outSize,outSize);

    // Create the coordinate arrays.
    // X
    coords = this->GetXCoordinates();
    newCoords = coords->NewInstance();
    newCoords->SetNumberOfComponents(coords->GetNumberOfComponents());
    newCoords->SetNumberOfTuples(uExt[1] - uExt[0] + 1);
    for (idx = uExt[0]; idx <= uExt[1]; ++idx)
      {
      newCoords->InsertComponent(idx-(vtkIdType)uExt[0], 0,
                    coords->GetComponent(idx-(vtkIdType)ext[0],0));
      }
    newGrid->SetXCoordinates(newCoords);
    newCoords->Delete();
    // Y
    coords = this->GetYCoordinates();
    newCoords = coords->NewInstance();
    newCoords->SetNumberOfComponents(coords->GetNumberOfComponents());
    newCoords->SetNumberOfTuples(uExt[3] - uExt[2] + 1);
    for (idx = uExt[2]; idx <= uExt[3]; ++idx)
      {
      newCoords->InsertComponent(idx-(vtkIdType)uExt[2], 0,
                    coords->GetComponent(idx-(vtkIdType)ext[2],0));
      }
    newGrid->SetYCoordinates(newCoords);
    newCoords->Delete();
    // Z
    coords = this->GetZCoordinates();
    newCoords = coords->NewInstance();
    newCoords->SetNumberOfComponents(coords->GetNumberOfComponents());
    newCoords->SetNumberOfTuples(uExt[5] - uExt[4] + 1);
    for (idx = uExt[4]; idx <= uExt[5]; ++idx)
      {
      newCoords->InsertComponent(idx-(vtkIdType)uExt[4], 0,
                    coords->GetComponent(idx-(vtkIdType)ext[4],0));
      }
    newGrid->SetZCoordinates(newCoords);
    newCoords->Delete();


    // Traverse this data and copy point attributes to output
    newId = 0;
    inInc1 = (this->Extent[1]-this->Extent[0]+1);
    inInc2 = inInc1*(this->Extent[3]-this->Extent[2]+1);
    for ( k=uExt[4]; k <= uExt[5]; ++k)
      { 
      kOffset = (k - this->Extent[4]) * inInc2;
      for ( j=uExt[2]; j <= uExt[3]; ++j)
        {
        jOffset = (j - this->Extent[2]) * inInc1;
        for ( i=uExt[0]; i <= uExt[1]; ++i)
          {
          idx = (i - this->Extent[0]) + jOffset + kOffset;
          outPD->CopyData(inPD, idx, newId++);
          }
        }
      }

    // Traverse input data and copy cell attributes to output
    newId = 0;
    inInc1 = (this->Extent[1] - this->Extent[0]);
    inInc2 = inInc1*(this->Extent[3] - this->Extent[2]);
    for ( k=uExt[4]; k < uExt[5]; ++k )
      {
      kOffset = (k - this->Extent[4]) * inInc2;
      for ( j=uExt[2]; j < uExt[3]; ++j )
        {
        jOffset = (j - this->Extent[2]) * inInc1;
        for ( i=uExt[0]; i < uExt[1]; ++i )
          {
          idx = (i - this->Extent[0]) + jOffset + kOffset;
          outCD->CopyData(inCD, idx, newId++);
          }
        }
      }

    this->SetExtent(uExt);
    this->SetXCoordinates(newGrid->GetXCoordinates());
    this->SetYCoordinates(newGrid->GetYCoordinates());
    this->SetZCoordinates(newGrid->GetZCoordinates());    
    inPD->ShallowCopy(outPD);
    inCD->ShallowCopy(outCD);
    newGrid->Delete();
    }
}



//----------------------------------------------------------------------------
void vtkRectilinearGrid::UpdateData()
{
  this->vtkDataObject::UpdateData();

  // This stuff should really be ImageToStructuredPoints ....

  if (this->UpdateNumberOfPieces == 1)
    {
    // Either the piece has not been used to set the update extent,
    // or the whole image was requested.
    return;
    }

  //if (this->Extent[0] < this->UpdateExtent[0] ||
  //  this->Extent[1] > this->UpdateExtent[1] ||
  //  this->Extent[2] < this->UpdateExtent[2] ||
  //  this->Extent[3] > this->UpdateExtent[3] ||
  //  this->Extent[4] < this->UpdateExtent[4] ||
  //  this->Extent[5] > this->UpdateExtent[5])
  //  {
  //  vtkImageData *image = vtkImageData::New();
  //  image->DeepCopy(this);
  //  this->SetExtent(this->UpdateExtent);
  //  this->AllocateScalars();
  //  this->CopyAndCastFrom(image, this->UpdateExtent);
  //  image->Delete();
  //  }

  // Try to avoid generating these if the input has generated them,
  // or the image data is already up to date.
  // I guess we relly need an MTime check.
  if(this->Piece != this->UpdatePiece ||
     this->NumberOfPieces != this->UpdateNumberOfPieces ||
     this->GhostLevel != this->UpdateGhostLevel ||
     !this->PointData->GetArray("vtkGhostLevels") ||
     !this->CellData->GetArray("vtkGhostLevels"))
    { // Create ghost levels for cells and points.
    vtkUnsignedCharArray *levels;
    int zeroExt[6], extent[6];
    int i, j, k, di, dj, dk, dist;
    
    this->GetExtent(extent);
    // Get the extent with ghost level 0.
    this->ExtentTranslator->SetWholeExtent(this->WholeExtent);
    this->ExtentTranslator->SetPiece(this->UpdatePiece);
    this->ExtentTranslator->SetNumberOfPieces(this->UpdateNumberOfPieces);
    this->ExtentTranslator->SetGhostLevel(0);
    this->ExtentTranslator->PieceToExtent();
    this->ExtentTranslator->GetExtent(zeroExt);

    // ---- POINTS ----
    // Allocate the appropriate number levels (number of points).
    levels = vtkUnsignedCharArray::New();
    levels->Allocate((this->Extent[1]-this->Extent[0] + 1) *
                     (this->Extent[3]-this->Extent[2] + 1) *
                     (this->Extent[5]-this->Extent[4] + 1));
    
    //cerr << "max: " << extent[0] << ", " << extent[1] << ", " 
    //   << extent[2] << ", " << extent[3] << ", " 
    //   << extent[4] << ", " << extent[5] << endl;
    //cerr << "zero: " << zeroExt[0] << ", " << zeroExt[1] << ", " 
    //   << zeroExt[2] << ", " << zeroExt[3] << ", "
    //   << zeroExt[4] << ", " << zeroExt[5] << endl;
    
    // Loop through the points in this image.
    for (k = extent[4]; k <= extent[5]; ++k)
      { 
      dk = 0;
      if (k < zeroExt[4])
        {
        dk = zeroExt[4] - k;
        }
      if (k >= zeroExt[5] && k < this->WholeExtent[5])
        { // Special case for last tile.
        dk = k - zeroExt[5] + 1;
        }
      for (j = extent[2]; j <= extent[3]; ++j)
        { 
        dj = 0;
        if (j < zeroExt[2])
          {
          dj = zeroExt[2] - j;
          }
        if (j >= zeroExt[3] && j < this->WholeExtent[3])
          { // Special case for last tile.
          dj = j - zeroExt[3] + 1;
          }
        for (i = extent[0]; i <= extent[1]; ++i)
          { 
          di = 0;
          if (i < zeroExt[0])
            {
            di = zeroExt[0] - i;
            }
          if (i >= zeroExt[1] && i < this->WholeExtent[1])
            { // Special case for last tile.
            di = i - zeroExt[1] + 1;
            }
          // Compute Manhatten distance.
          dist = di;
          if (dj > dist)
            {
            dist = dj;
            }
          if (dk > dist)
            {
            dist = dk;
            }

          //cerr << "   " << i << ", " << j << ", " << k << endl;
          //cerr << "   " << di << ", " << dj << ", " << dk << endl;
          //cerr << dist << endl;
          
          levels->InsertNextValue((unsigned char)dist);
          }
        }
      }
    levels->SetName("vtkGhostLevels");
    this->PointData->AddArray(levels);
    levels->Delete();
    levels = NULL;
  
    // Only generate ghost call levels if zero levels are requested.
    // (Although we still need ghost points.)
    if (this->UpdateGhostLevel == 0)
      {
      return;
      }
    
    // ---- CELLS ----
    // Allocate the appropriate number levels (number of cells).
    levels = vtkUnsignedCharArray::New();
    levels->Allocate((this->Extent[1]-this->Extent[0]) *
                     (this->Extent[3]-this->Extent[2]) *
                     (this->Extent[5]-this->Extent[4]));
    
    // Loop through the cells in this image.
    // Cells may be 2d or 1d ... Treat all as 3D
    if (extent[0] == extent[1])
      {
      ++extent[1];
      ++zeroExt[1];
      }
    if (extent[2] == extent[3])
      {
      ++extent[3];
      ++zeroExt[3];
      }
    if (extent[4] == extent[5])
      {
      ++extent[5];
      ++zeroExt[5];
      }
    
    // Loop
    for (k = extent[4]; k < extent[5]; ++k)
      { // Determine the Manhatten distances to zero extent.
      dk = 0;
      if (k < zeroExt[4])
        {
        dk = zeroExt[4] - k;
        }
      if (k >= zeroExt[5])
        {
        dk = k - zeroExt[5] + 1;
        }
      for (j = extent[2]; j < extent[3]; ++j)
        {
        dj = 0;
        if (j < zeroExt[2])
          {
          dj = zeroExt[2] - j;
          }
        if (j >= zeroExt[3])
          {
          dj = j - zeroExt[3] + 1;
          }
        for (i = extent[0]; i < extent[1]; ++i)
          {
          di = 0;
          if (i < zeroExt[0])
            {
            di = zeroExt[0] - i;
            }
          if (i >= zeroExt[1])
            {
            di = i - zeroExt[1] + 1;
            }
          // Compute Manhatten distance.
          dist = di;
          if (dj > dist)
            {
            dist = dj;
            }
          if (dk > dist)
            {
            dist = dk;
            }

          levels->InsertNextValue((unsigned char)dist);
          }
        }
      }
    levels->SetName("vtkGhostLevels");
    this->CellData->AddArray(levels);
    levels->Delete();
    levels = NULL;
    }

}




//----------------------------------------------------------------------------
void vtkRectilinearGrid::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Dimensions: (" << this->Dimensions[0] << ", "
                                  << this->Dimensions[1] << ", "
                                  << this->Dimensions[2] << ")\n";

  os << indent << "X Coordinates: " << this->XCoordinates << "\n";
  os << indent << "Y Coordinates: " << this->YCoordinates << "\n";
  os << indent << "Z Coordinates: " << this->ZCoordinates << "\n";

  os << indent << "WholeExtent: " << this->WholeExtent[0] << ", "
     << this->WholeExtent[1] << ", " << this->WholeExtent[2] << ", "
     << this->WholeExtent[3] << ", " << this->WholeExtent[4] << ", "
     << this->WholeExtent[5] << endl;

  os << indent << "Extent: " << this->Extent[0] << ", "
     << this->Extent[1] << ", " << this->Extent[2] << ", "
     << this->Extent[3] << ", " << this->Extent[4] << ", "
     << this->Extent[5] << endl;

}


