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
#include "vtkStructuredGrid.h"
#include "vtkVertex.h"
#include "vtkLine.h"
#include "vtkQuad.h"
#include "vtkHexahedron.h"
#include "vtkEmptyCell.h"
#include "vtkExtentTranslator.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkStructuredGrid, "$Revision$");
vtkStandardNewMacro(vtkStructuredGrid);

#define vtkAdjustBoundsMacro( A, B ) \
  A[0] = (B[0] < A[0] ? B[0] : A[0]);   A[1] = (B[0] > A[1] ? B[0] : A[1]); \
  A[2] = (B[1] < A[2] ? B[1] : A[2]);   A[3] = (B[1] > A[3] ? B[1] : A[3]); \
  A[4] = (B[2] < A[4] ? B[2] : A[4]);   A[5] = (B[2] > A[5] ? B[2] : A[5])

vtkStructuredGrid::vtkStructuredGrid()
{
  this->Vertex = vtkVertex::New();
  this->Line = vtkLine::New();
  this->Quad = vtkQuad::New();
  this->Hexahedron = vtkHexahedron::New();
  this->EmptyCell = vtkEmptyCell::New();
  
  this->Dimensions[0] = 0;
  this->Dimensions[1] = 0;
  this->Dimensions[2] = 0;
  this->DataDescription = VTK_EMPTY;
  
  this->Blanking = 0;
  this->PointVisibility = NULL;

  this->Extent[0] = this->Extent[2] = this->Extent[4] = 0;
  this->Extent[1] = this->Extent[3] = this->Extent[5] = -1;
  
}

//----------------------------------------------------------------------------
vtkStructuredGrid::~vtkStructuredGrid()
{
  this->Initialize();

  this->Vertex->Delete();
  this->Line->Delete();
  this->Quad->Delete();
  this->Hexahedron->Delete();
  this->EmptyCell->Delete();
}

//----------------------------------------------------------------------------
// Copy the geometric and topological structure of an input structured grid.
void vtkStructuredGrid::CopyStructure(vtkDataSet *ds)
{
  vtkStructuredGrid *sg=(vtkStructuredGrid *)ds;
  vtkPointSet::CopyStructure(ds);
  int i;

  for (i=0; i<3; i++)
    {
    this->Dimensions[i] = sg->Dimensions[i];
    }
  for (i=0; i<6; i++)
    {
    this->Extent[i] = sg->Extent[i];
    }

  this->DataDescription = sg->DataDescription;

  this->Blanking = sg->Blanking;
  this->SetPointVisibility(sg->PointVisibility);
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::Initialize()
{
  vtkPointSet::Initialize(); 
  if ( this->PointVisibility ) 
    {
    this->PointVisibility->UnRegister(this);
    }
  this->PointVisibility = NULL;
  this->Blanking = 0;
  this->SetDimensions(0,0,0);
}

//----------------------------------------------------------------------------
int vtkStructuredGrid::GetCellType(vtkIdType cellId)
{
  // see whether the cell is blanked
  if ( this->Blanking && !this->IsCellVisible(cellId) )
    {
    return VTK_EMPTY_CELL;
    }

  switch (this->DataDescription)
    {
    case VTK_EMPTY: 
      return VTK_EMPTY_CELL;

    case VTK_SINGLE_POINT: 
      return VTK_VERTEX;

    case VTK_X_LINE: case VTK_Y_LINE: case VTK_Z_LINE:
      return VTK_LINE;

    case VTK_XY_PLANE: case VTK_YZ_PLANE: case VTK_XZ_PLANE:
      return VTK_QUAD;

    case VTK_XYZ_GRID:
      return VTK_HEXAHEDRON;

    default:
      vtkErrorMacro(<<"Bad data description!");
      return VTK_EMPTY_CELL;
    }
}

//----------------------------------------------------------------------------
vtkCell *vtkStructuredGrid::GetCell(vtkIdType cellId)
{
  vtkCell *cell = NULL;
  vtkIdType idx;
  int i, j, k;
  int d01, offset1, offset2;
 
  // Make sure data is defined
  if ( ! this->Points )
    {
    vtkErrorMacro (<<"No data");
    return NULL;
    }
 
  // see whether the cell is blanked
  if ( this->Blanking && !this->IsCellVisible(cellId) )
    {
    return this->EmptyCell;
    }

  // Update dimensions
  this->GetDimensions();

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      return this->EmptyCell;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      cell = this->Vertex;
      cell->PointIds->SetId(0,0);
      break;

    case VTK_X_LINE:
      cell = this->Line;
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_Y_LINE:
      cell = this->Line;
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_Z_LINE:
      cell = this->Line;
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_XY_PLANE:
      cell = this->Quad;
      i = cellId % (this->Dimensions[0]-1);
      j = cellId / (this->Dimensions[0]-1);
      idx = i + j*this->Dimensions[0];
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_YZ_PLANE:
      cell = this->Quad;
      j = cellId % (this->Dimensions[1]-1);
      k = cellId / (this->Dimensions[1]-1);
      idx = j + k*this->Dimensions[1];
      offset1 = 1;
      offset2 = this->Dimensions[1];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_XZ_PLANE:
      cell = this->Quad;
      i = cellId % (this->Dimensions[0]-1);
      k = cellId / (this->Dimensions[0]-1);
      idx = i + k*this->Dimensions[0];
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_XYZ_GRID:
      cell = this->Hexahedron;
      d01 = this->Dimensions[0]*this->Dimensions[1];
      i = cellId % (this->Dimensions[0] - 1);
      j = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      k = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      idx = i+ j*this->Dimensions[0] + k*d01;
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      idx += d01;
      cell->PointIds->SetId(4,idx);
      cell->PointIds->SetId(5,idx+offset1);
      cell->PointIds->SetId(6,idx+offset1+offset2);
      cell->PointIds->SetId(7,idx+offset2);
      break;
    }

  // Extract point coordinates and point ids. NOTE: the ordering of the vtkQuad
  // and vtkHexahedron cells are tricky.
  int NumberOfIds = cell->PointIds->GetNumberOfIds();
  for (i=0; i<NumberOfIds; i++)
    {
    idx = cell->PointIds->GetId(i);
    cell->Points->SetPoint(i,this->Points->GetPoint(idx));
    }

  return cell;
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::GetCell(vtkIdType cellId, vtkGenericCell *cell)
{
  vtkIdType   idx;
  int   i, j, k;
  int   d01, offset1, offset2;
  float x[3];
 
  // Make sure data is defined
  if ( ! this->Points )
    {
    vtkErrorMacro (<<"No data");
    }
 
  // see whether the cell is blanked
  if ( this->Blanking && !this->IsCellVisible(cellId) )
    {
    cell->SetCellTypeToEmptyCell();
    return;
    }

  // Update dimensions
  this->GetDimensions();

  switch (this->DataDescription)
    {
    case VTK_EMPTY: 
      cell->SetCellTypeToEmptyCell();
      return;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      cell->SetCellTypeToVertex();
      cell->PointIds->SetId(0,0);
      break;

    case VTK_X_LINE:
      cell->SetCellTypeToLine();
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_Y_LINE:
      cell->SetCellTypeToLine();
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_Z_LINE:
      cell->SetCellTypeToLine();
      cell->PointIds->SetId(0,cellId);
      cell->PointIds->SetId(1,cellId+1);
      break;

    case VTK_XY_PLANE:
      cell->SetCellTypeToQuad();
      i = cellId % (this->Dimensions[0]-1);
      j = cellId / (this->Dimensions[0]-1);
      idx = i + j*this->Dimensions[0];
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_YZ_PLANE:
      cell->SetCellTypeToQuad();
      j = cellId % (this->Dimensions[1]-1);
      k = cellId / (this->Dimensions[1]-1);
      idx = j + k*this->Dimensions[1];
      offset1 = 1;
      offset2 = this->Dimensions[1];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_XZ_PLANE:
      cell->SetCellTypeToQuad();
      i = cellId % (this->Dimensions[0]-1);
      k = cellId / (this->Dimensions[0]-1);
      idx = i + k*this->Dimensions[0];
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      break;

    case VTK_XYZ_GRID:
      cell->SetCellTypeToHexahedron();
      d01 = this->Dimensions[0]*this->Dimensions[1];
      i = cellId % (this->Dimensions[0] - 1);
      j = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      k = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      idx = i+ j*this->Dimensions[0] + k*d01;
      offset1 = 1;
      offset2 = this->Dimensions[0];

      cell->PointIds->SetId(0,idx);
      cell->PointIds->SetId(1,idx+offset1);
      cell->PointIds->SetId(2,idx+offset1+offset2);
      cell->PointIds->SetId(3,idx+offset2);
      idx += d01;
      cell->PointIds->SetId(4,idx);
      cell->PointIds->SetId(5,idx+offset1);
      cell->PointIds->SetId(6,idx+offset1+offset2);
      cell->PointIds->SetId(7,idx+offset2);
      break;
    }

  // Extract point coordinates and point ids. NOTE: the ordering of the vtkQuad
  // and vtkHexahedron cells are tricky.
  int NumberOfIds = cell->PointIds->GetNumberOfIds();
  for (i=0; i<NumberOfIds; i++)
    {
    idx = cell->PointIds->GetId(i);
    this->Points->GetPoint(idx, x);
    cell->Points->SetPoint(i, x);
    }
}


//----------------------------------------------------------------------------
// Fast implementation of GetCellBounds().  Bounds are calculated without
// constructing a cell.
void vtkStructuredGrid::GetCellBounds(vtkIdType cellId, float bounds[6])
{
  vtkIdType idx = 0;
  int i, j, k;
  vtkIdType d01;
  int offset1 = 0;
  int offset2 = 0;
  float x[3];
  
  bounds[0] = bounds[2] = bounds[4] =  VTK_LARGE_FLOAT;
  bounds[1] = bounds[3] = bounds[5] = -VTK_LARGE_FLOAT;
  
  // Make sure data is defined
  if ( ! this->Points )
    {
    vtkErrorMacro (<<"No data");
    return;
    }
 
  // Update dimensions
  this->GetDimensions();

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      return;
    case VTK_SINGLE_POINT: // cellId can only be = 0
      this->Points->GetPoint( 0, x );
      bounds[0] = bounds[1] = x[0];
      bounds[2] = bounds[3] = x[1];
      bounds[4] = bounds[5] = x[2];
      break;

    case VTK_X_LINE:
    case VTK_Y_LINE:
    case VTK_Z_LINE:
      this->Points->GetPoint( cellId, x );
      bounds[0] = bounds[1] = x[0];
      bounds[2] = bounds[3] = x[1];
      bounds[4] = bounds[5] = x[2];

      this->Points->GetPoint( cellId +1, x );
      vtkAdjustBoundsMacro( bounds, x );
      break;

    case VTK_XY_PLANE:
    case VTK_YZ_PLANE:
    case VTK_XZ_PLANE:
      if (this->DataDescription == VTK_XY_PLANE)
        {
        i = cellId % (this->Dimensions[0]-1);
        j = cellId / (this->Dimensions[0]-1);
        idx = i + j*this->Dimensions[0];
        offset1 = 1;
        offset2 = this->Dimensions[0];
        }
      else if (this->DataDescription == VTK_YZ_PLANE)
        {
        j = cellId % (this->Dimensions[1]-1);
        k = cellId / (this->Dimensions[1]-1);
        idx = j + k*this->Dimensions[1];
        offset1 = 1;
        offset2 = this->Dimensions[1];
        }
      else if (this->DataDescription == VTK_XZ_PLANE)
        {
        i = cellId % (this->Dimensions[0]-1);
        k = cellId / (this->Dimensions[0]-1);
        idx = i + k*this->Dimensions[0];
        offset1 = 1;
        offset2 = this->Dimensions[0];
        }

      this->Points->GetPoint(idx, x);
      bounds[0] = bounds[1] = x[0];
      bounds[2] = bounds[3] = x[1];
      bounds[4] = bounds[5] = x[2];

      this->Points->GetPoint( idx+offset1, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset1+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      break;

    case VTK_XYZ_GRID:
      d01 = this->Dimensions[0]*this->Dimensions[1];
      i = cellId % (this->Dimensions[0] - 1);
      j = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      k = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      idx = i+ j*this->Dimensions[0] + k*d01;
      offset1 = 1;
      offset2 = this->Dimensions[0];

      this->Points->GetPoint(idx, x);
      bounds[0] = bounds[1] = x[0];
      bounds[2] = bounds[3] = x[1];
      bounds[4] = bounds[5] = x[2];

      this->Points->GetPoint( idx+offset1, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset1+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      idx += d01;

      this->Points->GetPoint(idx, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset1, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset1+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      this->Points->GetPoint( idx+offset2, x);
      vtkAdjustBoundsMacro( bounds, x );

      break;
    }
}


//----------------------------------------------------------------------------
// Turn on data blanking. Data blanking is the ability to turn off
// portions of the grid when displaying or operating on it. Some data
// (like finite difference data) routinely turns off data to simulate
// solid obstacles.
void vtkStructuredGrid::BlankingOn()
{
  if (!this->Blanking)
    {
    this->Blanking = 1;
    this->Modified();

    if ( !this->PointVisibility )
      {
      this->AllocatePointVisibility();
      }
    }
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::AllocatePointVisibility()
{
  if ( !this->PointVisibility )
    {
    this->PointVisibility = vtkUnsignedCharArray::New();
    this->PointVisibility->Allocate(this->GetNumberOfPoints(),1000);
    for (int i=0; i<this->GetNumberOfPoints(); i++)
      {
      this->PointVisibility->SetValue(i,1);
      }
    }
}

//----------------------------------------------------------------------------
// Turn off data blanking.
void vtkStructuredGrid::BlankingOff()
{
  if (this->Blanking)
    {
    this->Blanking = 0;
    this->Modified();
    }
}

//----------------------------------------------------------------------------
// Turn off data blanking.
void vtkStructuredGrid::SetBlanking(int b)
{
  if ( b )
    {
    this->BlankingOn();
    }
  else
    {
    this->BlankingOff();
    }
}

//----------------------------------------------------------------------------
// Turn off a particular data point.
void vtkStructuredGrid::BlankPoint(vtkIdType ptId)
{
  if ( !this->PointVisibility )
    {
    this->AllocatePointVisibility();
    }
  this->PointVisibility->SetValue(ptId,0);
}

//----------------------------------------------------------------------------
// Turn on a particular data point.
void vtkStructuredGrid::UnBlankPoint(vtkIdType ptId)
{
  if ( !this->PointVisibility )
    {
    this->AllocatePointVisibility();
    }
  this->PointVisibility->SetValue(ptId,1);
}

void vtkStructuredGrid::SetPointVisibility(vtkUnsignedCharArray *ptVis)
{
  if ( ptVis != this->PointVisibility )
    {
    if ( this->PointVisibility )
      {
      this->PointVisibility->Delete();
      }
    this->PointVisibility = ptVis;
    if ( ptVis )
      {
      ptVis->Register(this);
      }
    this->Modified();
    }
}

//----------------------------------------------------------------------------
// Return non-zero if the specified cell is visible (i.e., not blanked)
unsigned char vtkStructuredGrid::IsCellVisible(vtkIdType cellId)
{
  // Update dimensions
  this->GetDimensions();

  int numIds=0;
  vtkIdType ptIds[8];
  int iMin, iMax, jMin, jMax, kMin, kMax;
  vtkIdType d01 = this->Dimensions[0]*this->Dimensions[1];
  iMin = iMax = jMin = jMax = kMin = kMax = 0;

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      return 0;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      numIds = 1;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      break;

    case VTK_X_LINE:
      iMin = cellId;
      iMax = cellId + 1;
      numIds = 2;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMax + jMin*this->Dimensions[0] + kMin*d01;
      break;

    case VTK_Y_LINE:
      jMin = cellId;
      jMax = cellId + 1;
      numIds = 2;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMin + jMax*this->Dimensions[0] + kMin*d01;
      break;

    case VTK_Z_LINE:
      kMin = cellId;
      kMax = cellId + 1;
      numIds = 2;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMin + jMin*this->Dimensions[0] + kMax*d01;
      break;

    case VTK_XY_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      numIds = 4;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMax + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[2] = iMax + jMax*this->Dimensions[0] + kMin*d01;
      ptIds[3] = iMin + jMax*this->Dimensions[0] + kMin*d01;
      break;

    case VTK_YZ_PLANE:
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      numIds = 4;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMin + jMax*this->Dimensions[0] + kMin*d01;
      ptIds[2] = iMin + jMax*this->Dimensions[0] + kMax*d01;
      ptIds[3] = iMin + jMin*this->Dimensions[0] + kMax*d01;
      break;

    case VTK_XZ_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      numIds = 4;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMax + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[2] = iMax + jMin*this->Dimensions[0] + kMax*d01;
      ptIds[3] = iMin + jMin*this->Dimensions[0] + kMax*d01;
      break;

    case VTK_XYZ_GRID:
      iMin = cellId % (this->Dimensions[0] - 1);
      iMax = iMin + 1;
      jMin = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      jMax = jMin + 1;
      kMin = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      kMax = kMin + 1;
      numIds = 8;
      ptIds[0] = iMin + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[1] = iMax + jMin*this->Dimensions[0] + kMin*d01;
      ptIds[2] = iMax + jMax*this->Dimensions[0] + kMin*d01;
      ptIds[3] = iMin + jMax*this->Dimensions[0] + kMin*d01;
      ptIds[4] = iMin + jMin*this->Dimensions[0] + kMax*d01;
      ptIds[5] = iMax + jMin*this->Dimensions[0] + kMax*d01;
      ptIds[6] = iMax + jMax*this->Dimensions[0] + kMax*d01;
      ptIds[7] = iMin + jMax*this->Dimensions[0] + kMax*d01;
      break;
    }

  for (int i=0; i<numIds; i++)
    {
    if ( !this->IsPointVisible(ptIds[i]) )
      {
      return 0;
      }
    }
  
  return 1;
}

//----------------------------------------------------------------------------
// Set dimensions of structured grid dataset.
void vtkStructuredGrid::SetDimensions(int i, int j, int k)
{
  this->SetExtent(0, i-1, 0, j-1, 0, k-1);
}

//----------------------------------------------------------------------------
// Set dimensions of structured grid dataset.
void vtkStructuredGrid::SetDimensions(int dim[3])
{
  this->SetExtent(0, dim[0]-1, 0, dim[1]-1, 0, dim[2]-1);
}

//----------------------------------------------------------------------------
// Get the points defining a cell. (See vtkDataSet for more info.)
void vtkStructuredGrid::GetCellPoints(vtkIdType cellId, vtkIdList *ptIds)
{
  // Update dimensions
  this->GetDimensions();

  int iMin, iMax, jMin, jMax, kMin, kMax;
  vtkIdType d01 = this->Dimensions[0]*this->Dimensions[1];
 
  ptIds->Reset();
  iMin = iMax = jMin = jMax = kMin = kMax = 0;

  switch (this->DataDescription)
    {
    case VTK_EMPTY:
      return;

    case VTK_SINGLE_POINT: // cellId can only be = 0
      ptIds->SetNumberOfIds(1);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      break;

    case VTK_X_LINE:
      iMin = cellId;
      iMax = cellId + 1;
      ptIds->SetNumberOfIds(2);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMax + jMin*this->Dimensions[0] + kMin*d01);
      break;

    case VTK_Y_LINE:
      jMin = cellId;
      jMax = cellId + 1;
      ptIds->SetNumberOfIds(2);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMin + jMax*this->Dimensions[0] + kMin*d01);
      break;

    case VTK_Z_LINE:
      kMin = cellId;
      kMax = cellId + 1;
      ptIds->SetNumberOfIds(2);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMin + jMin*this->Dimensions[0] + kMax*d01);
      break;

    case VTK_XY_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      ptIds->SetNumberOfIds(4);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMax + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(2, iMax + jMax*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(3, iMin + jMax*this->Dimensions[0] + kMin*d01);
      break;

    case VTK_YZ_PLANE:
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      ptIds->SetNumberOfIds(4);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMin + jMax*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(2, iMin + jMax*this->Dimensions[0] + kMax*d01);
      ptIds->SetId(3, iMin + jMin*this->Dimensions[0] + kMax*d01);
      break;

    case VTK_XZ_PLANE:
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      ptIds->SetNumberOfIds(4);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMax + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(2, iMax + jMin*this->Dimensions[0] + kMax*d01);
      ptIds->SetId(3, iMin + jMin*this->Dimensions[0] + kMax*d01);
      break;

    case VTK_XYZ_GRID:
      iMin = cellId % (this->Dimensions[0] - 1);
      iMax = iMin + 1;
      jMin = (cellId / (this->Dimensions[0] - 1)) % (this->Dimensions[1] - 1);
      jMax = jMin + 1;
      kMin = cellId / ((this->Dimensions[0] - 1) * (this->Dimensions[1] - 1));
      kMax = kMin + 1;
      ptIds->SetNumberOfIds(8);
      ptIds->SetId(0, iMin + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(1, iMax + jMin*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(2, iMax + jMax*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(3, iMin + jMax*this->Dimensions[0] + kMin*d01);
      ptIds->SetId(4, iMin + jMin*this->Dimensions[0] + kMax*d01);
      ptIds->SetId(5, iMax + jMin*this->Dimensions[0] + kMax*d01);
      ptIds->SetId(6, iMax + jMax*this->Dimensions[0] + kMax*d01);
      ptIds->SetId(7, iMin + jMax*this->Dimensions[0] + kMax*d01);
      break;
    }
}

//----------------------------------------------------------------------------
// Should we split up cells, or just points.  It does not matter for now.
// Extent of structured data assumes points.
void vtkStructuredGrid::SetUpdateExtent(int piece, int numPieces,
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
void vtkStructuredGrid::SetExtent(int extent[6])
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
void vtkStructuredGrid::SetExtent(int xMin, int xMax, 
                                  int yMin, int yMax,
                                  int zMin, int zMax)
{
  int extent[6];

  extent[0] = xMin; extent[1] = xMax;
  extent[2] = yMin; extent[3] = yMax;
  extent[4] = zMin; extent[5] = zMax;
  
  this->SetExtent(extent);
}

int *vtkStructuredGrid::GetDimensions () 
{
  this->GetDimensions(this->Dimensions);
  return this->Dimensions;
} 

void vtkStructuredGrid::GetDimensions (int dim[3]) 
{ 
  dim[0] = this->Extent[1] - this->Extent[0] + 1;
  dim[1] = this->Extent[3] - this->Extent[2] + 1;
  dim[2] = this->Extent[5] - this->Extent[4] + 1;
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::GetCellNeighbors(vtkIdType cellId, vtkIdList *ptIds,
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
      vtkStructuredData::GetCellNeigbors(cellId, ptIds, 
                                         cellIds, this->GetDimensions());
      break;
      
    default:
      this->vtkDataSet::GetCellNeighbors(cellId, ptIds, cellIds);
    }
  
  // If blanking, remove blanked cells.
  if ( this->Blanking )
    {
    int xcellId;
    for (int i=0; i<cellIds->GetNumberOfIds(); i++)
      {
      xcellId = cellIds->GetId(i);
      if ( !this->IsCellVisible(xcellId) )
        {
        cellIds->DeleteId(xcellId);
        }
      }
    }
}

//----------------------------------------------------------------------------
unsigned long vtkStructuredGrid::GetActualMemorySize()
{
  return this->vtkPointSet::GetActualMemorySize();
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::ShallowCopy(vtkDataObject *dataObject)
{
  vtkStructuredGrid *grid = vtkStructuredGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    this->InternalStructuredGridCopy(grid);
    }

  // Do superclass
  this->vtkPointSet::ShallowCopy(dataObject);
}

//----------------------------------------------------------------------------
void vtkStructuredGrid::DeepCopy(vtkDataObject *dataObject)
{
  vtkStructuredGrid *grid = vtkStructuredGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    this->InternalStructuredGridCopy(grid);
    }

  // Do superclass
  this->vtkPointSet::DeepCopy(dataObject);
}

//----------------------------------------------------------------------------
// This copies all the local variables (but not objects).
void vtkStructuredGrid::InternalStructuredGridCopy(vtkStructuredGrid *src)
{
  int idx;

  this->DataDescription = src->DataDescription;
  this->Blanking = src->Blanking;

  // Update dimensions
  this->GetDimensions();

  for (idx = 0; idx < 3; ++idx)
    {
    this->Dimensions[idx] = src->Dimensions[idx];
    }
}

//----------------------------------------------------------------------------
// Override this method because of blanking
void vtkStructuredGrid::GetScalarRange(float range[2])
{
  vtkDataArray *ptScalars = this->PointData->GetScalars();
  vtkDataArray *cellScalars = this->CellData->GetScalars();
  float ptRange[2];
  float cellRange[2];
  float s;
  int id, num;
  
  ptRange[0] =  VTK_LARGE_FLOAT;
  ptRange[1] =  -VTK_LARGE_FLOAT;
  if ( ptScalars )
    {
    num = this->GetNumberOfPoints();
    for (id=0; id < num; id++)
      {
      if ( this->IsPointVisible(id) )
        {
        s = ptScalars->GetComponent(id,0);
        if ( s < ptRange[0] )
          {
          ptRange[0] = s;
          }
        if ( s > ptRange[1] )
          {
          ptRange[1] = s;
          }
        }
      }
    }

  cellRange[0] =  ptRange[0];
  cellRange[1] =  ptRange[1];
  if ( cellScalars )
    {
    num = this->GetNumberOfCells();
    for (id=0; id < num; id++)
      {
      if ( this->IsCellVisible(id) )
        {
        s = cellScalars->GetComponent(id,0);
        if ( s < cellRange[0] )
          {
          cellRange[0] = s;
          }
        if ( s > cellRange[1] )
          {
          cellRange[1] = s;
          }
        }
      }
    }

  range[0] = (cellRange[0] >= VTK_LARGE_FLOAT ? 0.0 : cellRange[0]);
  range[1] = (cellRange[1] <= -VTK_LARGE_FLOAT ? 1.0 : cellRange[1]);

  this->ComputeTime.Modified();
}


//----------------------------------------------------------------------------
void vtkStructuredGrid::Crop()
{
  int i, j, k;
  int uExt[6];

  // If the update extent is larger than the extent, 
  // we cannot do anything about it here.
  for (i = 0; i < 3; ++i)
    {
    uExt[i*2] = this->UpdateExtent[i*2];
    if (uExt[i*2] < this->Extent[i*2])
      {
      uExt[i*2] = this->Extent[i*2];
      }
    uExt[i*2+1] = this->UpdateExtent[i*2+1];
    if (uExt[i*2+1] > this->Extent[i*2+1])
      {
      uExt[i*2+1] = this->Extent[i*2+1];
      }
    }
  
  // If extents already match, then we need to do nothing.
  if (this->Extent[0] == uExt[0] && this->Extent[1] == uExt[1]
      && this->Extent[2] == uExt[2] && this->Extent[3] == uExt[3]
      && this->Extent[4] == uExt[4] && this->Extent[5] == uExt[5])
    {
    return;
    }
  else
    {
    vtkStructuredGrid *newGrid;
    vtkPointData *inPD, *outPD;
    vtkCellData *inCD, *outCD;
    int outSize, jOffset, kOffset;
    vtkIdType idx, newId;
    vtkPoints *newPts, *inPts;
    int inInc1, inInc2;

    // Get the points.  Protect against empty data objects.
    inPts = this->GetPoints();
    if (inPts == NULL)
      {
      return;
      }

    vtkDebugMacro(<< "Cropping Grid");

    newGrid = vtkStructuredGrid::New();
    inPD  = this->GetPointData();
    inCD  = this->GetCellData();
    outPD = newGrid->GetPointData();
    outCD = newGrid->GetCellData();

    // Allocate necessary objects
    //
    newGrid->SetExtent(uExt);
    outSize = (uExt[1]-uExt[0]+1)*(uExt[3]-uExt[2]+1)*(uExt[5]-uExt[4]+1);
    newPts = (vtkPoints *) inPts->MakeObject(); 
    newPts->SetNumberOfPoints(outSize);
    outPD->CopyAllocate(inPD,outSize,outSize);
    outCD->CopyAllocate(inCD,outSize,outSize);

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
          newPts->SetPoint(newId,inPts->GetPoint(idx));
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
    this->SetPoints(newPts);
    newPts->Delete();
    inPD->ShallowCopy(outPD);
    inCD->ShallowCopy(outCD);
    newGrid->Delete();
    }
}


//----------------------------------------------------------------------------
void vtkStructuredGrid::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  int dim[3];
  this->GetDimensions(dim);
  os << indent << "Dimensions: (" << dim[0] << ", "
                                  << dim[1] << ", "
                                  << dim[2] << ")\n";

  os << indent << "Blanking: " << (this->Blanking ? "On\n" : "Off\n");

  os << indent << "WholeExtent: " << this->WholeExtent[0] << ", "
     << this->WholeExtent[1] << ", " << this->WholeExtent[2] << ", "
     << this->WholeExtent[3] << ", " << this->WholeExtent[4] << ", "
     << this->WholeExtent[5] << endl;
  os << indent << "Extent: " << this->Extent[0] << ", "
     << this->Extent[1] << ", " << this->Extent[2] << ", "
     << this->Extent[3] << ", " << this->Extent[4] << ", "
     << this->Extent[5] << endl;

  os << ")\n";
}




//----------------------------------------------------------------------------
void vtkStructuredGrid::UpdateData()
{
  this->vtkDataObject::UpdateData();

  if (this->UpdateNumberOfPieces == 1)
    {
    // Either the piece has not been used to set the update extent,
    // or the whole image was requested.
    return;
    }

  // Try to avoid generating these if the input has generated them,
  // or the image data is already up to date.
  // I guess we relly need an MTime check.
  if(this->Piece != this->UpdatePiece ||
     this->NumberOfPieces != this->UpdateNumberOfPieces ||
     this->GhostLevel != this->UpdateGhostLevel ||
     !this->PointData->GetArray("vtkGhostLevels"))
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
    
    // Loop through the points in this grid.
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
  
    // Only generate ghost cell levels if zero levels are requested.
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

