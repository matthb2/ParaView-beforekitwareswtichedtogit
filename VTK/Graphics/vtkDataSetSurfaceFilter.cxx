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
#include "vtkDataSetSurfaceFilter.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkGenericCell.h"
#include "vtkHexahedron.h"
#include "vtkMergePoints.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPyramid.h"
#include "vtkRectilinearGrid.h"
#include "vtkStructuredGrid.h"
#include "vtkStructuredGridGeometryFilter.h"
#include "vtkStructuredPoints.h"
#include "vtkTetra.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkVoxel.h"
#include "vtkWedge.h"

vtkCxxRevisionMacro(vtkDataSetSurfaceFilter, "$Revision$");
vtkStandardNewMacro(vtkDataSetSurfaceFilter);

//----------------------------------------------------------------------------
vtkDataSetSurfaceFilter::vtkDataSetSurfaceFilter()
{
  this->QuadHash = NULL;
  this->PointMap = NULL;
  this->QuadHashLength = 0;
  this->UseStrips = 0;
  this->NumberOfNewCells = 0;

  // Quad allocation stuff.
  this->FastGeomQuadArrayLength = 0;
  this->NumberOfFastGeomQuadArrays = 0;
  this->FastGeomQuadArrays = NULL;
  this->NextArrayIndex = 0;
  this->NextQuadIndex = 0;

}

//----------------------------------------------------------------------------
vtkDataSetSurfaceFilter::~vtkDataSetSurfaceFilter()
{
  if (this->QuadHash)
    {
    this->DeleteQuadHash();
    }
}



//----------------------------------------------------------------------------
// Information for output type of PolyData
void vtkDataSetSurfaceFilter::ExecuteInformation()
{
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::Execute()
{
  vtkDataSet *input= this->GetInput();
  vtkIdType numCells = input->GetNumberOfCells();
  int *ext;

  if (input->CheckAttributes())
    {
    return;
    }

  if (numCells == 0)
    {
    return;
    }

  switch (input->GetDataObjectType())
    {
    case  VTK_UNSTRUCTURED_GRID:
      {
      this->UnstructuredGridExecute();
      if (this->GetOutput()->CheckAttributes())
        {
        return;
        }

      return;
      }
    case VTK_RECTILINEAR_GRID:
      {
      vtkRectilinearGrid *grid = vtkRectilinearGrid::SafeDownCast(input);
      ext = grid->GetExtent();      
      this->StructuredExecute(grid, ext);
      return;
      }
    case VTK_STRUCTURED_GRID:
      {
      vtkStructuredGrid *grid = vtkStructuredGrid::SafeDownCast(input);
      ext = grid->GetExtent();      
      this->StructuredExecute(grid, ext);
      return;
      }
    case VTK_STRUCTURED_POINTS:      
      {
      vtkStructuredPoints *image = vtkStructuredPoints::SafeDownCast(input);
      ext = image->GetExtent();      
      this->StructuredExecute(image, ext);
      return;
      }
     case VTK_IMAGE_DATA:      
      {
      vtkImageData *image = vtkImageData::SafeDownCast(input);
      ext = image->GetExtent();      
      this->StructuredExecute(image, ext);
      return;
      }
    case VTK_POLY_DATA:      
      {
      vtkPolyData *inPd = vtkPolyData::SafeDownCast(input);
      vtkPolyData *output = this->GetOutput();
      output->ShallowCopy(inPd);
      return;
      }
    default:
      this->DataSetExecute();
      return;
    }
}





//----------------------------------------------------------------------------
// It is a pain that structured data sets do not share a common super class
// other than data set, and data set does not allow access to extent!
void vtkDataSetSurfaceFilter::StructuredExecute(vtkDataSet *input, int *ext)
{
  int *wholeExt;
  vtkIdType numPoints, cellArraySize;
  vtkCellArray *outStrips;
  vtkCellArray *outPolys;
  vtkPoints *outPoints;

  // Cell Array Size is a pretty good estimate.  
  // Does not consider direction of strip.

  wholeExt = input->GetWholeExtent();

  // Lets figure out how many cells and points we are going to have.
  // It may be overkill comptuing the exact amount, but we can do it, so ...
  cellArraySize = numPoints = 0;
  // xMin face
  if (ext[0] == wholeExt[0] && ext[2] != ext[3] && ext[4] != ext[5] && ext[0] != ext[1])
    {
    cellArraySize += 2*(ext[3]-ext[2]+1)*(ext[5]-ext[4]+1);
    numPoints += (ext[3]-ext[2]+1)*(ext[5]-ext[4]+1);
    }
  // xMax face
  if (ext[1] == wholeExt[1] && ext[2] != ext[3] && ext[4] != ext[5])
    {
    cellArraySize += 2*(ext[3]-ext[2]+1)*(ext[5]-ext[4]+1);
    numPoints += (ext[3]-ext[2]+1)*(ext[5]-ext[4]+1);
    }
  // yMin face
  if (ext[2] == wholeExt[2] && ext[0] != ext[1] && ext[4] != ext[5] && ext[2] != ext[3])
    {
    cellArraySize += 2*(ext[1]-ext[0]+1)*(ext[5]-ext[4]+1);
    numPoints += (ext[1]-ext[0]+1)*(ext[5]-ext[4]+1);
    }
  // yMax face
  if (ext[3] == wholeExt[3] && ext[0] != ext[1] && ext[4] != ext[5])
    {
    cellArraySize += 2*(ext[1]-ext[0]+1)*(ext[5]-ext[4]+1);
    numPoints += (ext[1]-ext[0]+1)*(ext[5]-ext[4]+1);
    }
  // zMin face
  if (ext[4] == wholeExt[4] && ext[0] != ext[1] && ext[2] != ext[3] && ext[4] != ext[5])
    {
    cellArraySize += 2*(ext[1]-ext[0]+1)*(ext[3]-ext[2]+1);
    numPoints += (ext[1]-ext[0]+1)*(ext[3]-ext[2]+1);
    }
  // zMax face
  if (ext[5] == wholeExt[5] && ext[0] != ext[1] && ext[2] != ext[3])
    {
    cellArraySize += 2*(ext[1]-ext[0]+1)*(ext[3]-ext[2]+1);
    numPoints += (ext[1]-ext[0]+1)*(ext[3]-ext[2]+1);
    }
  
  if (this->UseStrips)
    {
    outStrips = vtkCellArray::New();
    outStrips->Allocate(cellArraySize);
    this->GetOutput()->SetStrips(outStrips);
    outStrips->Delete();
    outStrips = NULL;
    }
  else
    {
    outPolys = vtkCellArray::New();
    outPolys->Allocate(cellArraySize);
    this->GetOutput()->SetPolys(outPolys);
    outPolys->Delete();
    outPolys = NULL;
    }
  outPoints = vtkPoints::New();
  outPoints->Allocate(numPoints);
  this->GetOutput()->SetPoints(outPoints);
  outPoints->Delete();
  outPoints = NULL;
  

  // Allocate attributes for copying.
  this->GetOutput()->GetPointData()->CopyAllocate(input->GetPointData());
  this->GetOutput()->GetCellData()->CopyAllocate(input->GetCellData());

  if (this->UseStrips)
    {
    // xMin face
    this->ExecuteFaceStrips(input, 0, ext, 0,1,2);
    // xMax face
    this->ExecuteFaceStrips(input, 1, ext, 0,2,1);
    // yMin face
    this->ExecuteFaceStrips(input, 0, ext, 1,2,0);
    // yMax face
    this->ExecuteFaceStrips(input, 1, ext, 1,0,2);
    // zMin face
    this->ExecuteFaceStrips(input, 0, ext, 2,0,1);
    // zMax face
    this->ExecuteFaceStrips(input, 1, ext, 2,1,0);
    }
  else
    {
    // xMin face
    this->ExecuteFaceQuads(input, 0, ext, 0,1,2);
    // xMax face
    this->ExecuteFaceQuads(input, 1, ext, 0,2,1);
    // yMin face
    this->ExecuteFaceQuads(input, 0, ext, 1,2,0);
    // yMax face
    this->ExecuteFaceQuads(input, 1, ext, 1,0,2);
    // zMin face
    this->ExecuteFaceQuads(input, 0, ext, 2,0,1);
    // zMax face
    this->ExecuteFaceQuads(input, 1, ext, 2,1,0);
    }
  this->GetOutput()->Squeeze();
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::ExecuteFaceStrips(vtkDataSet *input, 
                                              int maxFlag, int *ext,
                                              int aAxis, int bAxis, int cAxis)
{
  vtkPolyData  *output;
  vtkPoints    *outPts;
  vtkCellArray *outStrips;
  vtkPointData *inPD, *outPD;
  int          *wholeExt;
  int          pInc[3];
  int          qInc[3];
  int          cOutInc;
  float        pt[3];
  vtkIdType    inStartPtId;
  vtkIdType    outStartPtId;
  vtkIdType    outPtId;
  vtkIdType    inId, outId;
  int          ib, ic;
  int          aA2, bA2, cA2;
  int          rotatedFlag;
  vtkIdType    *stripArray;
  vtkIdType    stripArrayIdx;

  output = this->GetOutput();
  outPts = output->GetPoints();
  outPD = output->GetPointData();
  inPD = input->GetPointData();

  wholeExt = input->GetWholeExtent();
  pInc[0] = 1;
  pInc[1] = (ext[1]-ext[0]+1);
  pInc[2] = (ext[3]-ext[2]+1) * pInc[1];
  // quad increments (cell incraments, but cInc could be confused with c axis).
  qInc[0] = 1;
  qInc[1] = ext[1]-ext[0];
  qInc[2] = (ext[3]-ext[2]) * qInc[1];

  // Tempoprary variables to avoid many multiplications.
  aA2 = aAxis * 2;
  bA2 = bAxis * 2;
  cA2 = cAxis * 2;

  // We might as well put the test for this face here.
  if (ext[bA2] == ext[bA2+1] || ext[cA2] == ext[cA2+1])
    {
    return;
    }
  if (maxFlag)
    { // max faces have a slightly different condition to avoid coincident faces.
    if (ext[aA2] == ext[aA2+1] || ext[aA2+1] < wholeExt[aA2+1])
      {
      return;
      } 
    }
  else
    {
    if (ext[aA2] > wholeExt[aA2])
      {
      return;
      }
    }

  // Lets rotate the image to make b the longest axis.
  // This will make the tri strips longer.
  rotatedFlag = 0;
  if (ext[bA2+1]-ext[bA2] < ext[cA2+1]-ext[cA2])
    {
    int tmp;
    rotatedFlag = 1;
    tmp = cAxis;
    cAxis = bAxis;
    bAxis = tmp;
    bA2 = bAxis * 2;
    cA2 = cAxis * 2;
    }
  
  // Assuming no ghost cells ...
  inStartPtId = 0;
  if (maxFlag)
    {
    inStartPtId = pInc[aAxis]*(ext[aA2+1]-ext[aA2]);
    }

  outStartPtId = outPts->GetNumberOfPoints();
  // Make the points for this face.
  for (ic = ext[cA2]; ic <= ext[cA2+1]; ++ic)
    {
    for (ib = ext[bA2]; ib <= ext[bA2+1]; ++ib)
      {
      inId = inStartPtId + (ib-ext[bA2])*pInc[bAxis] 
                         + (ic-ext[cA2])*pInc[cAxis];
      input->GetPoint(inId, pt);
      outId = outPts->InsertNextPoint(pt);
      // Copy point data.
      outPD->CopyData(inPD,inId,outId);
      }
    }

  // Do the cells.
  cOutInc = ext[bA2+1] - ext[bA2] + 1;

  // Tri Strips (no cell data ...).
  // Allocate the temporary array user to create the tri strips.
  stripArray = new vtkIdType[2*(ext[bA2+1]-ext[bA2]+1)];
  // Make the cells for this face.
  outStrips = output->GetStrips();
  for (ic = ext[cA2]; ic < ext[cA2+1]; ++ic)
    {
    // Fill in the array describing the strips.
    stripArrayIdx = 0;
    outPtId = outStartPtId + (ic-ext[cA2])*cOutInc;
    if (rotatedFlag)
      {
      for (ib = ext[bA2]; ib <= ext[bA2+1]; ++ib)
        {
        stripArray[stripArrayIdx++] = outPtId+cOutInc;
        stripArray[stripArrayIdx++] = outPtId;
        ++outPtId;
        }
      }
    else
      { // Faster to justto dupicate the inner most loop.
      for (ib = ext[bA2]; ib <= ext[bA2+1]; ++ib)
        {
        stripArray[stripArrayIdx++] = outPtId;
        stripArray[stripArrayIdx++] = outPtId+cOutInc;
        ++outPtId;
        }
      }
    outStrips->InsertNextCell(stripArrayIdx, stripArray);
    }
  delete [] stripArray;
}


//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::ExecuteFaceQuads(vtkDataSet *input, 
                                             int maxFlag, int *ext,
                                             int aAxis, int bAxis, int cAxis)
{
  vtkPolyData  *output;
  vtkPoints    *outPts;
  vtkCellArray *outPolys;
  vtkPointData *inPD, *outPD;
  vtkCellData  *inCD, *outCD;
  int          *wholeExt;
  int          pInc[3];
  int          qInc[3];
  int          cOutInc;
  float        pt[3];
  vtkIdType    inStartPtId;
  vtkIdType    inStartCellId;
  vtkIdType    outStartPtId;
  vtkIdType    outPtId;
  vtkIdType    inId, outId;
  int          ib, ic;
  int          aA2, bA2, cA2;

  output = this->GetOutput();
  outPts = output->GetPoints();
  outPD = output->GetPointData();
  inPD = input->GetPointData();
  outCD = output->GetCellData();
  inCD = input->GetCellData();

  wholeExt = input->GetWholeExtent();
  pInc[0] = 1;
  pInc[1] = (ext[1]-ext[0]+1);
  pInc[2] = (ext[3]-ext[2]+1) * pInc[1];
  // quad increments (cell incraments, but cInc could be confused with c axis).
  qInc[0] = 1;
  qInc[1] = ext[1]-ext[0];
  // The conditions are for when we have one or more degenerate axes (2d or 1d cells).
  if (qInc[1] == 0)
    {
    qInc[1] = 1;
    }
  qInc[2] = (ext[3]-ext[2]) * qInc[1];
  if (qInc[2] == 0)
    {
    qInc[2] = qInc[1];
    }


  // Tempoprary variables to avoid many multiplications.
  aA2 = aAxis * 2;
  bA2 = bAxis * 2;
  cA2 = cAxis * 2;

  // We might as well put the test for this face here.
  if (ext[bA2] == ext[bA2+1] || ext[cA2] == ext[cA2+1])
    {
    return;
    }
  if (maxFlag)
    { 
    if (ext[aA2+1] < wholeExt[aA2+1])
      {
      return;
      } 
    }
  else
    { // min faces have a slightly different condition to avoid coincident faces.
    if (ext[aA2] == ext[aA2+1] || ext[aA2] > wholeExt[aA2])
      {
      return;
      }
    }

  // Assuming no ghost cells ...
  inStartPtId = inStartCellId = 0;
  // I put this confusing conditional to fix a regression test.
  // If we are creating a maximum face, then we indeed have to offset the input cell Ids.
  // However, vtkGeometryFilter created a 2d image as a max face, but the cells are copied
  // as a min face (no offset).  Hence maxFlag = 1 and there should be no offset.
  if (maxFlag && ext[aA2] < ext[1+aA2])
    {
    inStartPtId = pInc[aAxis]*(ext[aA2+1]-ext[aA2]);
    inStartCellId = qInc[aAxis]*(ext[aA2+1]-ext[aA2]-1);
    }

  outStartPtId = outPts->GetNumberOfPoints();
  // Make the points for this face.
  for (ic = ext[cA2]; ic <= ext[cA2+1]; ++ic)
    {
    for (ib = ext[bA2]; ib <= ext[bA2+1]; ++ib)
      {
      inId = inStartPtId + (ib-ext[bA2])*pInc[bAxis] 
                         + (ic-ext[cA2])*pInc[cAxis];
      input->GetPoint(inId, pt);
      outId = outPts->InsertNextPoint(pt);
      // Copy point data.
      outPD->CopyData(inPD,inId,outId);
      }
    }

  // Do the cells.
  cOutInc = ext[bA2+1] - ext[bA2] + 1;

  outPolys = output->GetPolys();

  // Old method for creating quads (needed for cell data.).
  for (ic = ext[cA2]; ic < ext[cA2+1]; ++ic)
    {
    for (ib = ext[bA2]; ib < ext[bA2+1]; ++ib)
      {
      outPtId = outStartPtId + (ib-ext[bA2]) + (ic-ext[cA2])*cOutInc;
      inId = inStartCellId + (ib-ext[bA2])*qInc[bAxis] + (ic-ext[cA2])*qInc[cAxis];

      outId = outPolys->InsertNextCell(4);
      outPolys->InsertCellPoint(outPtId);
      outPolys->InsertCellPoint(outPtId+cOutInc);
      outPolys->InsertCellPoint(outPtId+cOutInc+1);
      outPolys->InsertCellPoint(outPtId+1);
      // Copy cell data.
      outCD->CopyData(inCD,inId,outId);
      }
    }
}



//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::DataSetExecute()
{
  vtkIdType cellId, newCellId;
  int i, j;
  vtkDataSet *input= this->GetInput();
  vtkIdType numPts=input->GetNumberOfPoints();
  vtkIdType numCells=input->GetNumberOfCells();
  vtkGenericCell *cell;
  vtkCell *face;
  float *x;
  vtkIdList *cellIds;
  vtkIdList *pts;
  vtkPoints *newPts;
  vtkIdType ptId, pt;
  int npts;
  vtkPointData *pd = input->GetPointData();
  vtkCellData *cd = input->GetCellData();
  vtkPolyData *output = this->GetOutput();
  vtkPointData *outputPD = output->GetPointData();
  vtkCellData *outputCD = output->GetCellData();
  
  if (numCells == 0)
    {
    return;
    }

  cellIds = vtkIdList::New();
  pts = vtkIdList::New();

  vtkDebugMacro(<<"Executing geometry filter");

  cell = vtkGenericCell::New();


  // Allocate
  //
  newPts = vtkPoints::New();
  newPts->Allocate(numPts,numPts/2);
  output->Allocate(4*numCells,numCells/2);
  outputPD->CopyAllocate(pd,numPts,numPts/2);
  outputCD->CopyAllocate(cd,numCells,numCells/2);

  // Traverse cells to extract geometry
  //
  int abort=0;
  vtkIdType progressInterval = numCells/20 + 1;

  for(cellId=0; cellId < numCells && !abort; cellId++)
    {
    //Progress and abort method support
    if ( !(cellId % progressInterval) )
      {
      vtkDebugMacro(<<"Process cell #" << cellId);
      this->UpdateProgress ((float)cellId/numCells);
      abort = this->GetAbortExecute();
      }
    
    input->GetCell(cellId,cell);
    switch (cell->GetCellDimension())
      {
      // create new points and then cell
      case 0: case 1: case 2:
        
        npts = cell->GetNumberOfPoints();
        pts->Reset();
        for ( i=0; i < npts; i++)
          {
          ptId = cell->GetPointId(i);
          x = input->GetPoint(ptId);
          pt = newPts->InsertNextPoint(x);
          outputPD->CopyData(pd,ptId,pt);
          pts->InsertId(i,pt);
          }
        newCellId = output->InsertNextCell(cell->GetCellType(), pts);
        outputCD->CopyData(cd,cellId,newCellId);
        break;
       case 3:
        for (j=0; j < cell->GetNumberOfFaces(); j++)
          {
          face = cell->GetFace(j);
          input->GetCellNeighbors(cellId, face->PointIds, cellIds);
          if ( cellIds->GetNumberOfIds() <= 0)
            {
            npts = face->GetNumberOfPoints();
            pts->Reset();
            for ( i=0; i < npts; i++)
              {
              ptId = face->GetPointId(i);
              x = input->GetPoint(ptId);
              pt = newPts->InsertNextPoint(x);
              outputPD->CopyData(pd,ptId,pt);
              pts->InsertId(i,pt);
              }
            newCellId = output->InsertNextCell(face->GetCellType(), pts);
            outputCD->CopyData(cd,cellId,newCellId);
            }
          }
      break;
      } //switch
    } //for all cells

  vtkDebugMacro(<<"Extracted " << newPts->GetNumberOfPoints() << " points,"
                << output->GetNumberOfCells() << " cells.");

  // Update ourselves and release memory
  //
  cell->Delete();
  output->SetPoints(newPts);
  newPts->Delete();

  //free storage
  output->Squeeze();

  cellIds->Delete();
  pts->Delete();
}


//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::ComputeInputUpdateExtents(vtkDataObject *output)
{
  int piece, numPieces, ghostLevels;
  vtkDataSet *input = this->GetInput();
  
  if (this->GetInput() == NULL)
    {
    return;
    }
  piece = output->GetUpdatePiece();
  numPieces = output->GetUpdateNumberOfPieces();
  ghostLevels = output->GetUpdateGhostLevel();
  
  if (numPieces > 1)
    {
    // The special execute for structured data handle boundaries internally.
    // PolyData does not need any ghost levels.
    if (input->GetDataObjectType() == VTK_UNSTRUCTURED_GRID)
      { // Processing does nothing fo ghost levels yet so ...
      // Be careful to set output ghost level value one less than default
      // when they are implemented.  I had trouble with multiple executes.
      //++ghostLevels;
      }
    }
  

  this->GetInput()->SetUpdateExtent(piece, numPieces, ghostLevels);

  this->GetInput()->RequestExactExtentOn();
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if (this->UseStrips)
    {
    os << indent << "UseStripsOn\n";
    }
  else
    {
    os << indent << "UseStripsOff\n";
    }
}

//========================================================================
// Tris are now degenerate quads so we only need one hash table.
// We might want to change the method names from QuadHash to just Hash.



//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::UnstructuredGridExecute()
{
  vtkCellArray *newVerts;
  vtkCellArray *newLines;
  vtkCellArray *newPolys;
  vtkPoints *newPts;
  vtkIdType *ids;
  int progressCount;
  vtkIdType cellId;
  int i, j;
  vtkIdType *cellPointer;
  int cellType;
  vtkUnstructuredGrid *input =
    vtkUnstructuredGrid::SafeDownCast(this->GetInput());
  vtkIdType numPts=input->GetNumberOfPoints();
  vtkIdType numCells=input->GetNumberOfCells();
  vtkGenericCell *cell;
  int numFacePts, numCellPts;
  vtkIdType inPtId, outPtId;
  vtkPointData *inputPD = input->GetPointData();
  vtkCellData *inputCD = input->GetCellData();
  vtkCellData *cd = input->GetCellData();
  vtkPolyData *output = this->GetOutput();
  vtkPointData *outputPD = output->GetPointData();
  vtkCellData *outputCD = output->GetCellData();
  vtkIdType outPts[4];
  vtkFastGeomQuad *q;
  unsigned char* cellTypes = input->GetCellTypesArray()->GetPointer(0);

  // These are for the default case/
  vtkIdList *pts;
  vtkPoints *coords;
  vtkCell *face;
  int flag2D = 0;
  
  pts = vtkIdList::New();  
  coords = vtkPoints::New();
  cell = vtkGenericCell::New();

  this->NumberOfNewCells = 0;
  this->InitializeQuadHash(numPts);

  // Allocate
  //
  newPts = vtkPoints::New();
  newPts->Allocate(numPts);
  newPolys = vtkCellArray::New();
  newPolys->Allocate(4*numCells,numCells/2);
  newVerts = vtkCellArray::New();
  newLines = vtkCellArray::New();

  outputPD->CopyAllocate(inputPD, numPts, numPts/2);
  outputCD->CopyAllocate(inputCD, numCells, numCells/2);
  
  // Traverse cells to extract geometry
  //
  progressCount = 0;
  int abort=0;
  vtkIdType progressInterval = numCells/20 + 1;

  // First insert all points lines in output and 3D geometry in hash.
  // Save 2D geometry for second pass.
  // initialize the pointer to the cells for fast traversal.
  cellPointer = input->GetCells()->GetPointer();
  for(cellId=0; cellId < numCells && !abort; cellId++)
    {
    //Progress and abort method support
    if ( progressCount >= progressInterval )
      {
      vtkDebugMacro(<<"Process cell #" << cellId);
      this->UpdateProgress ((float)cellId/numCells);
      abort = this->GetAbortExecute();
      progressCount = 0;
      }
    progressCount++;
  
    // Direct access to cells.
    cellType = cellTypes[cellId];
    numCellPts = cellPointer[0];
    ids = cellPointer+1;
    // Move to the next cell.
    cellPointer += (1 + *cellPointer);

    // A couple of common cases to see if things go faster.
    if (cellType == VTK_VERTEX || cellType == VTK_POLY_VERTEX)
      {
      newVerts->InsertNextCell(numCellPts);
      for (i = 0; i < numCellPts; ++i)
        {
        inPtId = ids[i];
        outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
        newVerts->InsertCellPoint(outPtId);
        }
      outputCD->CopyData(cd, cellId, this->NumberOfNewCells++);
      }
    else if (cellType == VTK_LINE || cellType == VTK_POLY_LINE)
      {
      newLines->InsertNextCell(numCellPts);
      for (i = 0; i < numCellPts; ++i)
        {
        inPtId = ids[i];
        outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
        newLines->InsertCellPoint(outPtId);
        }
//      outputCD->CopyData(cd,cellId,newCellId);
      outputCD->CopyData(cd, cellId, this->NumberOfNewCells++);
      }
    else if (cellType == VTK_HEXAHEDRON)
      {
      this->InsertQuadInHash(ids[0], ids[1], ids[5], ids[4], cellId);
      this->InsertQuadInHash(ids[0], ids[3], ids[2], ids[1], cellId);
      this->InsertQuadInHash(ids[0], ids[4], ids[7], ids[3], cellId);
      this->InsertQuadInHash(ids[1], ids[2], ids[6], ids[5], cellId);
      this->InsertQuadInHash(ids[2], ids[3], ids[7], ids[6], cellId);
      this->InsertQuadInHash(ids[4], ids[5], ids[6], ids[7], cellId);
      }
    else if (cellType == VTK_VOXEL)
      {
      this->InsertQuadInHash(ids[0], ids[1], ids[5], ids[4], cellId);
      this->InsertQuadInHash(ids[0], ids[2], ids[3], ids[1], cellId);
      this->InsertQuadInHash(ids[0], ids[4], ids[6], ids[2], cellId);
      this->InsertQuadInHash(ids[1], ids[3], ids[7], ids[5], cellId);
      this->InsertQuadInHash(ids[2], ids[6], ids[7], ids[3], cellId);
      this->InsertQuadInHash(ids[4], ids[5], ids[7], ids[6], cellId);
      }
    else if (cellType == VTK_TETRA)
      {
      this->InsertTriInHash(ids[0], ids[1], ids[3], cellId);
      this->InsertTriInHash(ids[0], ids[2], ids[1], cellId);
      this->InsertTriInHash(ids[0], ids[3], ids[2], cellId);
      this->InsertTriInHash(ids[1], ids[2], ids[3], cellId);
      }
    else if (cellType == VTK_PIXEL || cellType == VTK_QUAD || 
             cellType == VTK_TRIANGLE || cellType == VTK_POLYGON || 
             cellType == VTK_TRIANGLE_STRIP)
      { // save 2D cells for second pass
      flag2D = 1;
      }
    else
      // Default way of getting faces. Differentiates between linear
      // and higher order cells.
      {
      input->GetCell(cellId,cell);
      if ( cell->IsLinear() )
        {
        if (cell->GetCellDimension() == 3)
          {
          int numFaces = cell->GetNumberOfFaces();
          for (j=0; j < numFaces; j++)
            {
            face = cell->GetFace(j);
            numFacePts = face->GetNumberOfPoints();
            if (numFacePts == 4)
              {
              this->InsertQuadInHash(face->PointIds->GetId(0),
              face->PointIds->GetId(1),
              face->PointIds->GetId(2),
              face->PointIds->GetId(3), cellId);
              }
            else if (numFacePts == 3)
              {
              this->InsertTriInHash(face->PointIds->GetId(0),
                                    face->PointIds->GetId(1),
                                    face->PointIds->GetId(2), cellId);
              }
            else
              {
              vtkWarningMacro(<< "I cannot deal with faces with " << numPts
                              << " points.");
              }
            } // for all cell faces
          } // if 3D
        else
          {
          vtkErrorMacro("Missing cell type.");
          }
        } // a linear cell type

      else //process nonlinear cells via triangulation
        {
        if ( cell->GetCellDimension() == 1 )
          {
          cell->Triangulate(0,pts,coords);
          for (i=0; i < pts->GetNumberOfIds(); i+=2)
            {
            newLines->InsertNextCell(2);
            inPtId = pts->GetId(i);
            outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
            newLines->InsertCellPoint(outPtId);
            inPtId = pts->GetId(i+1);
            outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
            newLines->InsertCellPoint(outPtId);
            }
          }
        else if ( cell->GetCellDimension() == 2 )
          {
          cell->Triangulate(0,pts,coords);
          for (i=0; i < pts->GetNumberOfIds(); i+=3)
            {
            newPolys->InsertNextCell(3);
            inPtId = pts->GetId(i);
            outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
            newPolys->InsertCellPoint(outPtId);
            inPtId = pts->GetId(i+1);
            outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
            newPolys->InsertCellPoint(outPtId);
            inPtId = pts->GetId(i+2);
            outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
            newPolys->InsertCellPoint(outPtId);
            }
          } 
        else //3D nonlinear cell
          {
          vtkIdList *cellIds = vtkIdList::New();
          int numFaces = cell->GetNumberOfFaces();
          for (j=0; j < numFaces; j++)
            {
            face = cell->GetFace(j);
            input->GetCellNeighbors(cellId, face->PointIds, cellIds);
            if ( cellIds->GetNumberOfIds() <= 0)
              {
              face->Triangulate(0,pts,coords);
              for (i=0; i < pts->GetNumberOfIds(); i+=3)
                {
                newPolys->InsertNextCell(3);
                inPtId = pts->GetId(i);
                outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
                newPolys->InsertCellPoint(outPtId);
                inPtId = pts->GetId(i+1);
                outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
                newPolys->InsertCellPoint(outPtId);
                inPtId = pts->GetId(i+2);
                outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
                newPolys->InsertCellPoint(outPtId);
                }
              }
            }
          cellIds->Delete();
          } //3d cell
        } //nonlinear cell
      } // Cell type else.
    } // for all cells.
  
  
  // Now insert 2DCells.  Because of poly datas (cell data) ordering,
  // the 2D cells have to come after points and lines.
  // initialize the pointer to the cells for fast traversal.
  cellPointer = input->GetCells()->GetPointer();
  for(cellId=0; cellId < numCells && !abort && flag2D; cellId++)
    {  
    // Direct acces to cells.
    cellType = input->GetCellType(cellId);
    numCellPts = cellPointer[0];
    ids = cellPointer+1;
    // Move to the next cell.
    cellPointer += (1 + *cellPointer);

    // A couple of common cases to see if things go faster.
    if (cellType == VTK_PIXEL)
      { // Do we really want to insert the 2D cells into a hash?
      pts->Reset();
      pts->InsertId(0, this->GetOutputPointId(ids[0], input, newPts, outputPD));
      pts->InsertId(1, this->GetOutputPointId(ids[1], input, newPts, outputPD));
      pts->InsertId(2, this->GetOutputPointId(ids[3], input, newPts, outputPD));
      pts->InsertId(3, this->GetOutputPointId(ids[2], input, newPts, outputPD));
      newPolys->InsertNextCell(pts);
//      outputCD->CopyData(cd,cellId,newCellId);
      outputCD->CopyData(cd, cellId, this->NumberOfNewCells++);
      }
    else if (cellType == VTK_POLYGON || cellType == VTK_TRIANGLE || cellType == VTK_QUAD)
      {
      pts->Reset();
      for ( i=0; i < numCellPts; i++)
        {
        inPtId = ids[i];
        outPtId = this->GetOutputPointId(inPtId, input, newPts, outputPD); 
        pts->InsertId(i, outPtId);
        }
      newPolys->InsertNextCell(pts);
//      outputCD->CopyData(cd,cellId,newCellId);
      outputCD->CopyData(cd, cellId, this->NumberOfNewCells++);
      }
    else if (cellType == VTK_TRIANGLE_STRIP)
      {
      // Change strips to triangles so we do not have to worry about order.
      int toggle = 0;
      vtkIdType ptIds[3];
      // This check is not really necessary.  It was put here because of another (now fixed) bug.
      if (numCellPts > 1)
        {
        ptIds[0] = this->GetOutputPointId(ids[0], input, newPts, outputPD); 
        ptIds[1] = this->GetOutputPointId(ids[1], input, newPts, outputPD); 
        for (i = 2; i < numCellPts; ++i)
          {
          ptIds[2] = this->GetOutputPointId(ids[i], input, newPts, outputPD); 
          newPolys->InsertNextCell(3, ptIds);
//          outputCD->CopyData(cd,cellId,newCellId);
          outputCD->CopyData(cd, cellId, this->NumberOfNewCells++);
          ptIds[toggle] = ptIds[2];
          toggle = !toggle;
          }
        }
      }
    } // for all cells.


  // Now transfer geometry from hash to output (only triangles and quads).
  this->InitQuadHashTraversal();
  while ( (q = this->GetNextVisibleQuadFromHash()) )
    {
    if ( q->p0 == q->p3)
      {
      outPts[0] = this->GetOutputPointId(q->p0, input, newPts, outputPD);
      outPts[1] = this->GetOutputPointId(q->p1, input, newPts, outputPD);
      outPts[2] = this->GetOutputPointId(q->p2, input, newPts, outputPD);
      newPolys->InsertNextCell(3, outPts);
//      outputCD->CopyData(inputCD, q->SourceId, cellId);
      outputCD->CopyData(inputCD, q->SourceId, this->NumberOfNewCells++);
      }
    else
      {
      //cerr << " Creating quad:  " << q->p0 << " " << q->p1 << " " 
      //     << q->p2 << " " << q->p3 << endl;
      outPts[0] = this->GetOutputPointId(q->p0, input, newPts, outputPD);
      outPts[1] = this->GetOutputPointId(q->p1, input, newPts, outputPD);
      outPts[2] = this->GetOutputPointId(q->p2, input, newPts, outputPD);
      outPts[3] = this->GetOutputPointId(q->p3, input, newPts, outputPD);
      newPolys->InsertNextCell(4, outPts);
//      outputCD->CopyData(inputCD, q->SourceId, cellId);
      outputCD->CopyData(inputCD, q->SourceId, this->NumberOfNewCells++);
      }
    }
  
  // Update ourselves and release memory
  //
  cell->Delete();
  coords->Delete();
  pts->Delete();

  output->SetPoints(newPts);
  newPts->Delete();
  output->SetPolys(newPolys);
  newPolys->Delete();
  if (newVerts->GetNumberOfCells() > 0)
    {
    output->SetVerts(newVerts);
    }
  newVerts->Delete();
  newVerts = NULL;
  if (newLines->GetNumberOfCells() > 0)
    {
    output->SetLines(newLines);
    }
  newLines->Delete();
  newLines = NULL;



  //free storage
  output->Squeeze();

  this->DeleteQuadHash();
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::InitializeQuadHash(vtkIdType numPoints)
{
  vtkIdType i;

  if (this->QuadHash)
    {
    this->DeleteQuadHash();
    }

  // Prepare our special quad allocator (for efficiency).
  this->InitFastGeomQuadAllocation(numPoints);

  this->QuadHash = new vtkFastGeomQuad*[numPoints];
  this->QuadHashLength = numPoints;
  this->PointMap = new vtkIdType[numPoints];
  for (i = 0; i < numPoints; ++i)
    {
    this->QuadHash[i] = NULL;
    this->PointMap[i] = -1;
    }
}


//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::DeleteQuadHash()
{
  vtkIdType i;

  this->DeleteAllFastGeomQuads();

  for (i = 0; i < this->QuadHashLength; ++i)
    {
    this->QuadHash[i] = NULL;
    }

  delete [] this->QuadHash;
  this->QuadHash = NULL;
  this->QuadHashLength = 0;
  delete [] this->PointMap;
  this->PointMap = NULL;
}



//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::InsertQuadInHash(vtkIdType a, vtkIdType b,
                                               vtkIdType c, vtkIdType d, 
                                               vtkIdType sourceId)
{
  int tmp;
  vtkFastGeomQuad *quad, **end;


  // Reorder to get smallest id in a.
  if (b < a && b < c && b < d)
    {
    tmp = a;
    a = b;
    b = c;
    c = d;
    d = tmp;
    }
  else if (c < a && c < b && c < d)
    {
    tmp = a;
    a = c;
    c = tmp;
    tmp = b;
    b = d;
    d = tmp;
    }
  else if (d < a && d < b && d < c)
    {
    tmp = a;
    a = d;
    d = c;
    c = b;
    b = tmp;
    }

  // cerr << "Insert:  " << a << " " << b << " " << c << " " << d << endl;
  
  // Look for existing quad in the hash;
  end = this->QuadHash + a;
  quad = *end;
  while (quad)
    {
    end = &(quad->Next);
    // a has to match in this bin.
    // c should be independant of point order.
    if (c == quad->p2)
      { 
      // Check boh orders for b and d.
      if ((b == quad->p1 && d == quad->p3) || (b == quad->p3 && d == quad->p1))
        {
        // We have a match.
        quad->SourceId = -1;
        // That is all we need to do.  Hide any quad shared by two or more cells.
        //cerr << "   Duplicate\n";
        return;
        }
      }
    quad = *end;
    }
  
  //cerr << "  New\n";
  // Create a new quad and add it to the hash.
  quad = this->NewFastGeomQuad();
  quad->Next = NULL;
  quad->SourceId = sourceId;
  quad->p0 = a;
  quad->p1 = b;
  quad->p2 = c;
  quad->p3 = d;
  *end = quad;
}


//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::InsertTriInHash(vtkIdType a, vtkIdType b,
                                              vtkIdType c, vtkIdType sourceId)
{
  int tmp;
  vtkFastGeomQuad *quad, **end;

  // Reorder to get smallest id in a.
  if (b < a && b < c)
    {
    tmp = a;
    a = b;
    b = c;
    c = tmp;
    }
  else if (c < a && c < b)
    {
    tmp = a;
    a = c;
    c = b;
    b = tmp;
    }

  // Look for existing tri in the hash;
  end = this->QuadHash + a;
  quad = *end;
  while (quad)
    {
    end = &(quad->Next);
    // a has to match in this bin.
    // Tris have p0 == p3 
    if (quad->p0 == quad->p3)
      { 
      if ((b == quad->p1 && c == quad->p2) || (b == quad->p2 && c == quad->p1))
        {
        // We have a match.
        quad->SourceId = -1;
        // That is all we need to do. Hide any tri shared by two or more cells.
        return;
        }
      }
    quad = *end;
    }
  
  // Create a new quad and add it to the hash.
  quad = this->NewFastGeomQuad();
  quad->Next = NULL;
  quad->SourceId = sourceId;
  quad->p0 = a;
  quad->p1 = b;
  quad->p2 = c;
  quad->p3 = a;
  *end = quad;
}


//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::InitFastGeomQuadAllocation(int numberOfCells)
{
  int idx;

  this->DeleteAllFastGeomQuads();
  // Allocate 100 pointers to arrays.
  // This should be plenty (unless we have riangle strips) ...
  this->NumberOfFastGeomQuadArrays = 100;
  this->FastGeomQuadArrays = new vtkFastGeomQuad*[100];
  // Initalize all to NULL;
  for (idx = 0; idx < 100; ++idx)
    {
    this->FastGeomQuadArrays[idx] = NULL;
    }
  // Set pointer to the begining.
  this->NextArrayIndex = 0;
  this->NextQuadIndex = 0;

  // Lets keep the chunk size relatively small.
  if (numberOfCells < 100)
    {
    this->FastGeomQuadArrayLength = 50;
    }
  else
    {
    this->FastGeomQuadArrayLength = numberOfCells / 2;
    }
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::DeleteAllFastGeomQuads()
{
  int idx;

  for (idx = 0; idx < this->NumberOfFastGeomQuadArrays; ++idx)
    {
    if (this->FastGeomQuadArrays[idx])
      {
      delete [] this->FastGeomQuadArrays[idx];
      this->FastGeomQuadArrays[idx] = NULL;
      }
    }
  if (this->FastGeomQuadArrays)
    {
    delete [] this->FastGeomQuadArrays;
    this->FastGeomQuadArrays = NULL;
    }
  this->FastGeomQuadArrayLength = 0;
  this->NumberOfFastGeomQuadArrays = 0;
  this->NextArrayIndex = 0;
  this->NextQuadIndex = 0;
}

//----------------------------------------------------------------------------
vtkFastGeomQuad* vtkDataSetSurfaceFilter::NewFastGeomQuad()
{
  if (this->FastGeomQuadArrayLength == 0)
    {
    vtkErrorMacro("Face hash allocation has not been initialized.");
    return NULL;
    }

  // Although this should not happen often, check first.
  if (this->NextArrayIndex >= this->NumberOfFastGeomQuadArrays)
    {
    int idx, num;
    vtkFastGeomQuad** newArrays;
    num = this->FastGeomQuadArrayLength * 2;
    newArrays = new vtkFastGeomQuad*[num];
    for (idx = 0; idx < num; ++idx)
      {
      newArrays[idx] = NULL;
      if (idx < this->NumberOfFastGeomQuadArrays)
        {
        newArrays[idx] = this->FastGeomQuadArrays[idx];
        }
      }
    delete [] this->FastGeomQuadArrays;
    this->FastGeomQuadArrays = newArrays;
    this->FastGeomQuadArrayLength = num;
    }

  // Next: allocate a new array if necessary.
  if (this->FastGeomQuadArrays[this->NextArrayIndex] == NULL)
    {
    // We are allocating a whole bunch at a time to avoid 
    // allocation of many small pieces of memory.
    this->FastGeomQuadArrays[this->NextArrayIndex] 
      = new vtkFastGeomQuad[this->FastGeomQuadArrayLength];
    }

  vtkFastGeomQuad* q = (this->FastGeomQuadArrays[this->NextArrayIndex]) + this->NextQuadIndex;

  // Move to the next quad.
  ++(this->NextQuadIndex);
  if (this->NextQuadIndex >= this->FastGeomQuadArrayLength)
    {
    ++(this->NextArrayIndex);
    this->NextQuadIndex = 0;
    }

  return q;
}

//----------------------------------------------------------------------------
void vtkDataSetSurfaceFilter::InitQuadHashTraversal()
{
  this->QuadHashTraversalIndex = 0;
  this->QuadHashTraversal = this->QuadHash[0];
}

//----------------------------------------------------------------------------
vtkFastGeomQuad *vtkDataSetSurfaceFilter::GetNextVisibleQuadFromHash()
{
  vtkFastGeomQuad *quad;

  quad = this->QuadHashTraversal;

  // Move till traversal until we have a quad to return.
  // Note: the current traversal has not been returned yet.
  while (quad == NULL || quad->SourceId == -1)
    {
    if (quad)
      { // The quad must be hidden.  Move to the next.
      quad = quad->Next;
      }
    else
      { // must be the end of the linked list.  Move to the next bin.
      this->QuadHashTraversalIndex += 1;
      if ( this->QuadHashTraversalIndex >= this->QuadHashLength)
        { // There are no more bins.
        this->QuadHashTraversal = NULL;
        return NULL;
        }
      quad = this->QuadHash[this->QuadHashTraversalIndex];
      }
    }
  
  // Now we have a quad to return.  Set the traversal to the next entry.
  this->QuadHashTraversal = quad->Next;

  return quad;
}

//----------------------------------------------------------------------------
vtkIdType vtkDataSetSurfaceFilter::GetOutputPointId(vtkIdType inPtId,
                                                    vtkDataSet *input,
                                                    vtkPoints *outPts,
                                                    vtkPointData *outPD)
{
  vtkIdType outPtId;
  
  outPtId = this->PointMap[inPtId];
  if (outPtId == -1)
    {
    outPtId = outPts->InsertNextPoint(input->GetPoint(inPtId));
    outPD->CopyData(input->GetPointData(), inPtId, outPtId);
    this->PointMap[inPtId] = outPtId;
    }
  
  return outPtId;
}
