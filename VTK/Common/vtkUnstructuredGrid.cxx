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
#include "vtkUnstructuredGrid.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCellLinks.h"
#include "vtkConvexPointSet.h"
#include "vtkEmptyCell.h"
#include "vtkGenericCell.h"
#include "vtkHexahedron.h"
#include "vtkIdTypeArray.h"
#include "vtkLine.h"
#include "vtkObjectFactory.h"
#include "vtkPixel.h"
#include "vtkPointData.h"
#include "vtkPolyLine.h"
#include "vtkPolyVertex.h"
#include "vtkPolygon.h"
#include "vtkPyramid.h"
#include "vtkQuad.h"
#include "vtkQuadraticEdge.h"
#include "vtkQuadraticHexahedron.h"
#include "vtkQuadraticWedge.h"
#include "vtkQuadraticPyramid.h"
#include "vtkQuadraticQuad.h"
#include "vtkQuadraticTetra.h"
#include "vtkQuadraticTriangle.h"
#include "vtkTetra.h"
#include "vtkTriangle.h"
#include "vtkTriangleStrip.h"
#include "vtkUnsignedCharArray.h"
#include "vtkVertex.h"
#include "vtkVoxel.h"
#include "vtkWedge.h"

vtkCxxRevisionMacro(vtkUnstructuredGrid, "$Revision$");
vtkStandardNewMacro(vtkUnstructuredGrid);

vtkUnstructuredGrid::vtkUnstructuredGrid ()
{
  this->Vertex = vtkVertex::New();
  this->PolyVertex = vtkPolyVertex::New();
  this->Line = vtkLine::New();
  this->PolyLine = vtkPolyLine::New();
  this->Triangle = vtkTriangle::New();
  this->TriangleStrip = vtkTriangleStrip::New();
  this->Pixel = vtkPixel::New();
  this->Quad = vtkQuad::New();
  this->Polygon = vtkPolygon::New();
  this->Tetra = vtkTetra::New();
  this->Voxel = vtkVoxel::New();
  this->Hexahedron = vtkHexahedron::New();
  this->Wedge = vtkWedge::New();
  this->Pyramid = vtkPyramid::New();
  this->QuadraticEdge = vtkQuadraticEdge::New();
  this->QuadraticTriangle = vtkQuadraticTriangle::New();
  this->QuadraticQuad = vtkQuadraticQuad::New();
  this->QuadraticTetra = vtkQuadraticTetra::New();
  this->QuadraticHexahedron = vtkQuadraticHexahedron::New();
  this->QuadraticWedge = vtkQuadraticWedge::New();
  this->QuadraticPyramid = vtkQuadraticPyramid::New();
  this->ConvexPointSet = vtkConvexPointSet::New();
  this->EmptyCell = vtkEmptyCell::New();

  this->Connectivity = NULL;
  this->Links = NULL;
  this->Types = NULL;
  this->Locations = NULL;
  this->Allocate(1000,1000);
}

// Allocate memory space for data insertion. Execute this method before
// inserting any cells into object.
void vtkUnstructuredGrid::Allocate (vtkIdType numCells, int extSize)
{
  if ( numCells < 1 )
    {
    numCells = 1000;
    }
  if ( extSize < 1 )
    {
    extSize = 1000;
    }

  if ( this->Connectivity )
    {
    this->Connectivity->UnRegister(this);
    }
  this->Connectivity = vtkCellArray::New();
  this->Connectivity->Allocate(numCells,4*extSize);
  this->Connectivity->Register(this);
  this->Connectivity->Delete();

  if ( this->Types )
    {
    this->Types->UnRegister(this);
    }
  this->Types = vtkUnsignedCharArray::New();
  this->Types->Allocate(numCells,extSize);
  this->Types->Register(this);
  this->Types->Delete();

  if ( this->Locations )
    {
    this->Locations->UnRegister(this);
    }
  this->Locations = vtkIdTypeArray::New();
  this->Locations->Allocate(numCells,extSize);
  this->Locations->Register(this);
  this->Locations->Delete();
}

vtkUnstructuredGrid::~vtkUnstructuredGrid()
{
  vtkUnstructuredGrid::Initialize();
  
  this->Vertex->Delete();
  this->PolyVertex->Delete();
  this->Line->Delete();
  this->PolyLine->Delete();
  this->Triangle->Delete();
  this->TriangleStrip->Delete();
  this->Pixel->Delete();
  this->Quad->Delete();
  this->Polygon->Delete();
  this->Tetra->Delete();
  this->Voxel->Delete();
  this->Hexahedron->Delete();
  this->Wedge->Delete();
  this->Pyramid->Delete();
  this->QuadraticEdge->Delete();
  this->QuadraticTriangle->Delete();
  this->QuadraticQuad->Delete();
  this->QuadraticTetra->Delete();
  this->QuadraticHexahedron->Delete();
  this->QuadraticWedge->Delete();
  this->QuadraticPyramid->Delete();
  this->ConvexPointSet->Delete();
  this->EmptyCell->Delete();
}

// Copy the geometric and topological structure of an input unstructured grid.
void vtkUnstructuredGrid::CopyStructure(vtkDataSet *ds)
{
  vtkUnstructuredGrid *ug=(vtkUnstructuredGrid *)ds;
  vtkPointSet::CopyStructure(ds);

  this->Connectivity = ug->Connectivity;
  if (this->Connectivity)
    {
    this->Connectivity->Register(this);
    }

  this->Links = ug->Links;
  if (this->Links)
    {
    this->Links->Register(this);
    }

  this->Types = ug->Types;
  if (this->Types)
    {
    this->Types->Register(this);
    }

  this->Locations = ug->Locations;
  if (this->Locations)
    {
    this->Locations->Register(this);
    }
}

void vtkUnstructuredGrid::Initialize()
{
  vtkPointSet::Initialize();

  if ( this->Connectivity )
    {
    this->Connectivity->UnRegister(this);
    this->Connectivity = NULL;
    }

  if ( this->Links )
    {
    this->Links->UnRegister(this);
    this->Links = NULL;
    }

  if ( this->Types )
    {
    this->Types->UnRegister(this);
    this->Types = NULL;
    }

  if ( this->Locations )
    {
    this->Locations->UnRegister(this);
    this->Locations = NULL;
    }
}

int vtkUnstructuredGrid::GetCellType(vtkIdType cellId)
{

  vtkDebugMacro(<< "Returning cell type " << (int)this->Types->GetValue(cellId));
  return (int)this->Types->GetValue(cellId);
}

vtkCell *vtkUnstructuredGrid::GetCell(vtkIdType cellId)
{
  int i;
  int loc;
  vtkCell *cell = NULL;
  vtkIdType *pts, numPts;
  
  switch ((int)this->Types->GetValue(cellId))
    {
    case VTK_VERTEX:
      cell = this->Vertex;
      break;

    case VTK_POLY_VERTEX:
      cell = this->PolyVertex;
      break;

    case VTK_LINE: 
      cell = this->Line;
      break;

    case VTK_POLY_LINE:
      cell = this->PolyLine;
      break;

    case VTK_TRIANGLE:
      cell = this->Triangle;
      break;

    case VTK_TRIANGLE_STRIP:
      cell = this->TriangleStrip;
      break;

    case VTK_PIXEL:
      cell = this->Pixel;
      break;

    case VTK_QUAD:
      cell = this->Quad;
      break;

    case VTK_POLYGON:
      cell = this->Polygon;
      break;

    case VTK_TETRA:
      cell = this->Tetra;
      break;

    case VTK_VOXEL:
      cell = this->Voxel;
      break;

    case VTK_HEXAHEDRON:
      cell = this->Hexahedron;
      break;

    case VTK_WEDGE:
      cell = this->Wedge;
      break;

    case VTK_PYRAMID:
      cell = this->Pyramid;
      break;

    case VTK_QUADRATIC_EDGE:
      cell = this->QuadraticEdge;
      break;

    case VTK_QUADRATIC_TRIANGLE:
      cell = this->QuadraticTriangle;
      break;

    case VTK_QUADRATIC_QUAD:
      cell = this->QuadraticQuad;
      break;

    case VTK_QUADRATIC_TETRA:
      cell = this->QuadraticTetra;
      break;

    case VTK_QUADRATIC_HEXAHEDRON:
      cell = this->QuadraticHexahedron;
      break;

    case VTK_QUADRATIC_WEDGE:
      cell = this->QuadraticWedge;
      break;

    case VTK_QUADRATIC_PYRAMID:
      cell = this->QuadraticPyramid;
      break;

    case VTK_CONVEX_POINT_SET:
      cell = this->ConvexPointSet;
      break;

    case VTK_EMPTY_CELL:
      cell = this->EmptyCell;
      break;
    }

  if( !cell )
    {
    return NULL;
    }

  loc = this->Locations->GetValue(cellId);
  vtkDebugMacro(<< "location = " <<  loc);
  this->Connectivity->GetCell(loc,numPts,pts); 

  cell->PointIds->SetNumberOfIds(numPts);
  cell->Points->SetNumberOfPoints(numPts);

  for (i=0; i<numPts; i++)
    {
    cell->PointIds->SetId(i,pts[i]);
    cell->Points->SetPoint(i,this->Points->GetPoint(pts[i]));
    }

  if ( cell->RequiresInitialization() )
    {
    cell->Initialize(); //hack to make sure it retriangulates
    }

  return cell;
}

void vtkUnstructuredGrid::GetCell(vtkIdType cellId, vtkGenericCell *cell)
{
  int i;
  int    loc;
  double  x[3];
  vtkIdType *pts, numPts;
  
  cell->SetCellType((int)Types->GetValue(cellId));

  loc = this->Locations->GetValue(cellId);
  this->Connectivity->GetCell(loc,numPts,pts); 

  cell->PointIds->SetNumberOfIds(numPts);
  cell->Points->SetNumberOfPoints(numPts);

  for (i=0; i<numPts; i++)
    {
    cell->PointIds->SetId(i,pts[i]);
    this->Points->GetPoint(pts[i], x);
    cell->Points->SetPoint(i, x);
    }

  if ( cell->RequiresInitialization() )
    {
    cell->Initialize(); //hack to make sure it retriangulates
    }
}

// Fast implementation of GetCellBounds().  Bounds are calculated without
// constructing a cell.
void vtkUnstructuredGrid::GetCellBounds(vtkIdType cellId, double bounds[6])
{
  int i;
  int loc;
  double x[3];
  vtkIdType *pts, numPts;
  
  loc = this->Locations->GetValue(cellId);
  this->Connectivity->GetCell(loc,numPts,pts);

  // carefully compute the bounds
  if (numPts)
    {
    this->Points->GetPoint( pts[0], x );
    bounds[0] = x[0];
    bounds[2] = x[1];
    bounds[4] = x[2];
    bounds[1] = x[0];
    bounds[3] = x[1];
    bounds[5] = x[2];
    for (i=1; i < numPts; i++)
      {
      this->Points->GetPoint( pts[i], x );
      bounds[0] = (x[0] < bounds[0] ? x[0] : bounds[0]);
      bounds[1] = (x[0] > bounds[1] ? x[0] : bounds[1]);
      bounds[2] = (x[1] < bounds[2] ? x[1] : bounds[2]);
      bounds[3] = (x[1] > bounds[3] ? x[1] : bounds[3]);
      bounds[4] = (x[2] < bounds[4] ? x[2] : bounds[4]);
      bounds[5] = (x[2] > bounds[5] ? x[2] : bounds[5]);
      }
    }
  else
    {
    vtkMath::UninitializeBounds(bounds);
    }
  
}

int vtkUnstructuredGrid::GetMaxCellSize()
{
  if (this->Connectivity)
    {
    return this->Connectivity->GetMaxCellSize();
    }
  else
    {
    return 0;
    }
}

vtkIdType vtkUnstructuredGrid::GetNumberOfCells() 
{
  vtkDebugMacro(<< "NUMBER OF CELLS = " <<  (this->Connectivity ? this->Connectivity->GetNumberOfCells() : 0));
  return (this->Connectivity ? this->Connectivity->GetNumberOfCells() : 0);
}

// Insert/create cell in object by type and list of point ids defining
// cell topology.
int vtkUnstructuredGrid::InsertNextCell(int type, vtkIdList *ptIds)
{
  int npts = ptIds->GetNumberOfIds();
  // insert connectivity
  this->Connectivity->InsertNextCell(ptIds);
  // insert type and storage information   
  vtkDebugMacro(<< "insert location " 
                << this->Connectivity->GetInsertLocation(npts));
  this->Locations->InsertNextValue(this->Connectivity->GetInsertLocation(npts));
  return this->Types->InsertNextValue((unsigned char) type);

}

// Insert/create cell in object by type and list of point ids defining
// cell topology.
int vtkUnstructuredGrid::InsertNextCell(int type, int npts, vtkIdType *pts)
{
  // insert connectivity
  this->Connectivity->InsertNextCell(npts,pts);
  // insert type and storage information
  vtkDebugMacro(<< "insert location " 
                << this->Connectivity->GetInsertLocation(npts));
  this->Locations->InsertNextValue(this->Connectivity->GetInsertLocation(npts));
  return this->Types->InsertNextValue((unsigned char) type);

}

void vtkUnstructuredGrid::SetCells(int type, vtkCellArray *cells)
{
  int i;
  vtkIdType *pts = 0;
  vtkIdType npts = 0;
  
  // set cell array
  if ( this->Connectivity )
    {
    this->Connectivity->UnRegister(this);
    }
  this->Connectivity = cells;
  if ( this->Connectivity )
    {
    this->Connectivity->Register(this);
    }

  // see whether there are cell types available

  if ( this->Types)
    {
    this->Types->UnRegister(this);
    }
  this->Types = vtkUnsignedCharArray::New();
  this->Types->Allocate(cells->GetNumberOfCells(),1000);
  this->Types->Register(this);
  this->Types->Delete();

  if ( this->Locations)
    {
    this->Locations->UnRegister(this);
    }
  this->Locations = vtkIdTypeArray::New();
  this->Locations->Allocate(cells->GetNumberOfCells(),1000);
  this->Locations->Register(this);
  this->Locations->Delete();

  // build types
  for (i=0, cells->InitTraversal(); cells->GetNextCell(npts,pts); i++)
    {
    this->Types->InsertNextValue((unsigned char) type);
    this->Locations->InsertNextValue(cells->GetTraversalLocation(npts));
    }
}

void vtkUnstructuredGrid::SetCells(int *types, vtkCellArray *cells)
{
  int i;
  vtkIdType *pts = 0;
  vtkIdType npts = 0;
  
  // set cell array
  if ( this->Connectivity )
    {
    this->Connectivity->UnRegister(this);
    }
  this->Connectivity = cells;
  if ( this->Connectivity )
    {
    this->Connectivity->Register(this);
    }

  // see whether there are cell types available

  if ( this->Types)
    {
    this->Types->UnRegister(this);
    }
  this->Types = vtkUnsignedCharArray::New();
  this->Types->Allocate(cells->GetNumberOfCells(),1000);
  this->Types->Register(this);
  this->Types->Delete();

  if ( this->Locations)
    {
    this->Locations->UnRegister(this);
    }
  this->Locations = vtkIdTypeArray::New();
  this->Locations->Allocate(cells->GetNumberOfCells(),1000);
  this->Locations->Register(this);
  this->Locations->Delete();

  // build types
  for (i=0, cells->InitTraversal(); cells->GetNextCell(npts,pts); i++)
    {
    this->Types->InsertNextValue((unsigned char) types[i]);
    this->Locations->InsertNextValue(cells->GetTraversalLocation(npts));
    }
}


void vtkUnstructuredGrid::SetCells(vtkUnsignedCharArray *cellTypes, 
                                   vtkIdTypeArray *cellLocations, 
                                   vtkCellArray *cells)
{
  // set cell array
  if ( this->Connectivity )
    {
    this->Connectivity->UnRegister(this);
    }
  this->Connectivity = cells;
  if ( this->Connectivity )
    {
    this->Connectivity->Register(this);
    }

  // see whether there are cell types available

  if ( this->Types )
    {
    this->Types->UnRegister(this);
    }
  this->Types = cellTypes;
  if ( this->Types )
    {
    this->Types->Register(this);
    }

  if ( this->Locations )
    {
    this->Locations->UnRegister(this);
    }
  this->Locations = cellLocations;
  if ( this->Locations )
    {
    this->Locations->Register(this);
    }

}

void vtkUnstructuredGrid::BuildLinks()
{
  this->Links = vtkCellLinks::New();
  this->Links->Allocate(this->GetNumberOfPoints());
  this->Links->Register(this);
  this->Links->BuildLinks(this, this->Connectivity);
  this->Links->Delete();
}

void vtkUnstructuredGrid::GetCellPoints(vtkIdType cellId, vtkIdList *ptIds)
{
  int i;
  int loc;
  vtkIdType *pts, numPts;
  
  loc = this->Locations->GetValue(cellId);
  this->Connectivity->GetCell(loc,numPts,pts); 
  ptIds->SetNumberOfIds(numPts);
  for (i=0; i<numPts; i++)
    {
    ptIds->SetId(i,pts[i]);
    }

}

// Return a pointer to a list of point ids defining cell. (More efficient than alternative
// method.)
void vtkUnstructuredGrid::GetCellPoints(vtkIdType cellId, vtkIdType& npts,
                                        vtkIdType* &pts)
{
  int loc;

  loc = this->Locations->GetValue(cellId);

  this->Connectivity->GetCell(loc,npts,pts);
}

void vtkUnstructuredGrid::GetPointCells(vtkIdType ptId, vtkIdList *cellIds)
{
  vtkIdType *cells;
  int numCells;
  int i;

  if ( ! this->Links )
    {
    this->BuildLinks();
    }
  cellIds->Reset();

  numCells = this->Links->GetNcells(ptId);
  cells = this->Links->GetCells(ptId);

  cellIds->SetNumberOfIds(numCells);
  for (i=0; i < numCells; i++)
    {
    cellIds->SetId(i,cells[i]);
    }
}

void vtkUnstructuredGrid::Reset()
{
  if ( this->Connectivity )
    {
    this->Connectivity->Reset();
    }
  if ( this->Links )
    {
    this->Links->Reset();
    }
  if ( this->Types )
    {
    this->Types->Reset();
    }
  if ( this->Locations )
    {
    this->Locations->Reset();
    }
}

void vtkUnstructuredGrid::Squeeze()
{
  if ( this->Connectivity )
    {
    this->Connectivity->Squeeze();
    }
  if ( this->Links )
    {
    this->Links->Squeeze();
    }
  if ( this->Types )
    {
    this->Types->Squeeze();
    }
  if ( this->Locations )
    {
    this->Locations->Squeeze();
    }

  vtkPointSet::Squeeze();
}

// Remove a reference to a cell in a particular point's link list. You may 
// also consider using RemoveCellReference() to remove the references from 
// all the cell's points to the cell. This operator does not reallocate 
// memory; use the operator ResizeCellList() to do this if necessary.
void vtkUnstructuredGrid::RemoveReferenceToCell(vtkIdType ptId,
                                                vtkIdType cellId)
{
  this->Links->RemoveCellReference(cellId, ptId);  
}

// Add a reference to a cell in a particular point's link list. (You may also
// consider using AddCellReference() to add the references from all the 
// cell's points to the cell.) This operator does not realloc memory; use the
// operator ResizeCellList() to do this if necessary.
void vtkUnstructuredGrid::AddReferenceToCell(vtkIdType ptId, vtkIdType cellId)
{
  this->Links->AddCellReference(cellId, ptId);  
}

// Resize the list of cells using a particular point. (This operator assumes
// that BuildLinks() has been called.)
void vtkUnstructuredGrid::ResizeCellList(vtkIdType ptId, int size)
{
  this->Links->ResizeCellList(ptId,size);
}

// Replace the points defining cell "cellId" with a new set of points. This
// operator is (typically) used when links from points to cells have not been
// built (i.e., BuildLinks() has not been executed). Use the operator 
// ReplaceLinkedCell() to replace a cell when cell structure has been built.
void vtkUnstructuredGrid::ReplaceCell(vtkIdType cellId, int npts,
                                      vtkIdType *pts)
{
  int loc;

  loc = this->Locations->GetValue(cellId);
  this->Connectivity->ReplaceCell(loc,npts,pts);
}

// Add a new cell to the cell data structure (after cell links have been
// built). This method adds the cell and then updates the links from the points
// to the cells. (Memory is allocated as necessary.)
int vtkUnstructuredGrid::InsertNextLinkedCell(int type, int npts,
                                              vtkIdType *pts)
{
  int i, id;

  id = this->InsertNextCell(type,npts,pts);

  for (i=0; i<npts; i++)
    {
    this->Links->ResizeCellList(pts[i],1);
    this->Links->AddCellReference(id,pts[i]);  
    }

  return id;
}

//----------------------------------------------------------------------------
void vtkUnstructuredGrid::SetUpdateExtent(int piece, int numPieces,
                                          int ghostLevel)
{
  this->UpdatePiece = piece;
  this->UpdateNumberOfPieces = numPieces;
  this->UpdateGhostLevel = ghostLevel;
  this->UpdateExtentInitialized = 1;
}

//----------------------------------------------------------------------------
void vtkUnstructuredGrid::GetUpdateExtent(int &piece, int &numPieces,
                                          int &ghostLevel)
{
  piece = this->UpdatePiece;
  numPieces = this->UpdateNumberOfPieces;
  ghostLevel = this->UpdateGhostLevel;
}

//----------------------------------------------------------------------------
unsigned long vtkUnstructuredGrid::GetActualMemorySize()
{
  unsigned long size=this->vtkPointSet::GetActualMemorySize();
  if ( this->Connectivity )
    {
    size += this->Connectivity->GetActualMemorySize();
    }

  if ( this->Links )
    {
    size += this->Links->GetActualMemorySize();
    }

  if ( this->Types )
    {
    size += this->Types->GetActualMemorySize();
    }

  if ( this->Locations )
    {
    size += this->Locations->GetActualMemorySize();
    }

  return size;
}

//----------------------------------------------------------------------------
void vtkUnstructuredGrid::ShallowCopy(vtkDataObject *dataObject)
{
  vtkUnstructuredGrid *grid = vtkUnstructuredGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    // I do not know if this is correct but.

    if (this->Connectivity)
      {
      this->Connectivity->UnRegister(this);
      }
    this->Connectivity = grid->Connectivity;
    if (this->Connectivity)
      {
      this->Connectivity->Register(this);
      }

    if (this->Links)
      {
      this->Links->Delete();
      }
    this->Links = grid->Links;
    if (this->Links)
      {
      this->Links->Register(this);
      }

    if (this->Types)
      {
      this->Types->UnRegister(this);
      }
    this->Types = grid->Types;
    if (this->Types)
      {
      this->Types->Register(this);
      }

    if (this->Locations)
      {
      this->Locations->UnRegister(this);
      }
    this->Locations = grid->Locations;
    if (this->Locations)
      {
      this->Locations->Register(this);
      }

    }

  // Do superclass
  this->vtkPointSet::ShallowCopy(dataObject);
}

//----------------------------------------------------------------------------
void vtkUnstructuredGrid::DeepCopy(vtkDataObject *dataObject)
{
  vtkUnstructuredGrid *grid = vtkUnstructuredGrid::SafeDownCast(dataObject);

  if ( grid != NULL )
    {
    if ( this->Connectivity )
      {
      this->Connectivity->UnRegister(this);
      this->Connectivity = NULL;
      }
    if (grid->Connectivity)
      {
      this->Connectivity = vtkCellArray::New();
      this->Connectivity->DeepCopy(grid->Connectivity);
      this->Connectivity->Register(this);
      this->Connectivity->Delete();
      }

    if ( this->Links )
      {
      this->Links->UnRegister(this);
      this->Links = NULL;
      }
    if (grid->Links)
      {
      this->Links = vtkCellLinks::New();
      this->Links->DeepCopy(grid->Links);
      this->Links->Register(this);
      this->Links->Delete();
      }

    if ( this->Types )
      {
      this->Types->UnRegister(this);
      this->Types = NULL;
      }
    if (grid->Types)
      {
      this->Types = vtkUnsignedCharArray::New();
      this->Types->DeepCopy(grid->Types);
      this->Types->Register(this);
      this->Types->Delete();
      }

    if ( this->Locations )
      {
      this->Locations->UnRegister(this);
      this->Locations = NULL;
      }
    if (grid->Locations)
      {
      this->Locations = vtkIdTypeArray::New();
      this->Locations->DeepCopy(grid->Locations);
      this->Locations->Register(this);
      this->Locations->Delete();
      }
    }

  // Do superclass
  this->vtkPointSet::DeepCopy(dataObject);
}


void vtkUnstructuredGrid::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Number Of Pieces: " << this->NumberOfPieces << endl;
  os << indent << "Piece: " << this->Piece << endl;
  os << indent << "Ghost Level: " << this->GhostLevel << endl;
  
  os << indent << "UpdateExtent: " << this->UpdateExtent[0] << ", "
     << this->UpdateExtent[1] << ", " << this->UpdateExtent[2] << ", "
     << this->UpdateExtent[3] << ", " << this->UpdateExtent[4] << ", "
     << this->UpdateExtent[5] << endl;
}

// Determine neighbors as follows. Find the (shortest) list of cells that
// uses one of the points in ptIds. For each cell, in the list, see whether
// it contains the other points in the ptIds list. If so, it's a neighbor.
//
void vtkUnstructuredGrid::GetCellNeighbors(vtkIdType cellId, vtkIdList *ptIds,
                                           vtkIdList *cellIds)
{
  int i, j, k;
  int numPts, minNumCells, numCells;
  vtkIdType *pts, ptId, *cellPts, *cells;
  vtkIdType *minCells = NULL;
  int match;
  vtkIdType minPtId = 0, npts;
  
  if ( ! this->Links )
    {
    this->BuildLinks();
    }
  
  cellIds->Reset();
  
  //Find the point used by the fewest number of cells
  //
  numPts = ptIds->GetNumberOfIds();
  pts = ptIds->GetPointer(0);
  for (minNumCells=VTK_LARGE_INTEGER,i=0; i<numPts; i++)
    {
    ptId = pts[i];
    numCells = this->Links->GetNcells(ptId);
    cells = this->Links->GetCells(ptId);
    if ( numCells < minNumCells )
      {
      minNumCells = numCells;
      minCells = cells;
      minPtId = ptId;
      }
    }

  if (minNumCells == VTK_LARGE_INTEGER && numPts == 0) {
    vtkErrorMacro("input point ids empty.");
    minNumCells = 0;
  }
  //Now for each cell, see if it contains all the points
  //in the ptIds list.
  for (i=0; i<minNumCells; i++)
    {
    if ( minCells[i] != cellId ) //don't include current cell
      {
      this->GetCellPoints(minCells[i],npts,cellPts);
      for (match=1, j=0; j<numPts && match; j++) //for all pts in input cell
        {
        if ( pts[j] != minPtId ) //of course minPtId is contained by cell
          {
          for (match=k=0; k<npts; k++) //for all points in candidate cell
            {
            if ( pts[j] == cellPts[k] )
              {
              match = 1; //a match was found
              break; 
              }
            }//for all points in current cell
          }//if not guaranteed match
        }//for all points in input cell
      if ( match )
        {
        cellIds->InsertNextId(minCells[i]);
        }
      }//if not the reference cell
    }//for all candidate cells attached to point
}


int vtkUnstructuredGrid::IsHomogeneous() 
{
  unsigned char type;
  if (this->Types && this->Types->GetMaxId() >= 0)
    {
    type = Types->GetValue(0);
    for (int cellId = 0; cellId < this->GetNumberOfCells(); cellId++)
      {
      if (this->Types->GetValue(cellId) != type)
        {
        return 0;
        }
      }
    return 1;
    }
  return 0;
}

// Fill container with indices of cells which match given type.
void vtkUnstructuredGrid::GetIdsOfCellsOfType(int type, vtkIdTypeArray *array)
{
  for (int cellId = 0; cellId < this->GetNumberOfCells(); cellId++)
    {
    if ((int)Types->GetValue(cellId) == type)
      {
      array->InsertNextValue(cellId);
      }
    }
}


void vtkUnstructuredGrid::RemoveGhostCells(int level)
{
  vtkUnstructuredGrid* newGrid = vtkUnstructuredGrid::New();
  vtkDataArray* temp;
  unsigned char* cellGhostLevels;

  vtkIdType cellId, newCellId;
  vtkIdList *cellPts, *pointMap;
  vtkIdList *newCellPts;
  vtkCell *cell;
  vtkPoints *newPoints;
  int i, ptId, newId, numPts;
  int numCellPts;
  double *x;
  vtkPointData*   pd    = this->GetPointData();
  vtkPointData*   outPD = newGrid->GetPointData();
  vtkCellData*    cd    = this->GetCellData();
  vtkCellData*    outCD = newGrid->GetCellData();


  // Get a pointer to the cell ghost level array.
  temp = this->CellData->GetArray("vtkGhostLevels");
  if (temp == NULL)
    {
    vtkDebugMacro("Could not find cell ghost level array.");
    newGrid->Delete();
    return;
    }
  if ( (temp->GetDataType() != VTK_UNSIGNED_CHAR)
       || (temp->GetNumberOfComponents() != 1)
       || (temp->GetNumberOfTuples() < this->GetNumberOfCells()))
    {
    vtkErrorMacro("Poorly formed ghost level array.");
    newGrid->Delete();
    return;
    }
  cellGhostLevels =((vtkUnsignedCharArray*)temp)->GetPointer(0);


  // Now threshold based on the cell ghost level array.
  outPD->CopyAllocate(pd);
  outCD->CopyAllocate(cd);

  numPts = this->GetNumberOfPoints();
  newGrid->Allocate(this->GetNumberOfCells());
  newPoints = vtkPoints::New();
  newPoints->Allocate(numPts);

  pointMap = vtkIdList::New(); //maps old point ids into new
  pointMap->SetNumberOfIds(numPts);
  for (i=0; i < numPts; i++)
    {
    pointMap->SetId(i,-1);
    }


  newCellPts = vtkIdList::New();     

  // Check that the scalars of each cell satisfy the threshold criterion
  for (cellId=0; cellId < this->GetNumberOfCells(); cellId++)
    {
    cell = this->GetCell(cellId);
    cellPts = cell->GetPointIds();
    numCellPts = cell->GetNumberOfPoints();
        
    if ( cellGhostLevels[cellId] < level ) // Keep the cell.
      {
      for (i=0; i < numCellPts; i++)
        {
        ptId = cellPts->GetId(i);
        if ( (newId = pointMap->GetId(ptId)) < 0 )
          {
          x = this->GetPoint(ptId);
          newId = newPoints->InsertNextPoint(x);
          pointMap->SetId(ptId,newId);
          outPD->CopyData(pd,ptId,newId);
          }
        newCellPts->InsertId(i,newId);
        }
      newCellId = newGrid->InsertNextCell(cell->GetCellType(),newCellPts);
      outCD->CopyData(cd,cellId,newCellId);
      newCellPts->Reset();
      } // satisfied thresholding
    } // for all cells

  // now clean up / update ourselves
  pointMap->Delete();
  newCellPts->Delete();
  
  newGrid->SetPoints(newPoints);
  newPoints->Delete();

  this->CopyStructure(newGrid);
  this->GetPointData()->ShallowCopy(newGrid->GetPointData());
  this->GetCellData()->ShallowCopy(newGrid->GetCellData());
  newGrid->Delete();
  newGrid = NULL;

  this->Squeeze();
}
