/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*----------------------------------------------------------------------------
 Copyright (c) Sandia Corporation
 See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.
----------------------------------------------------------------------------*/
#include "vtkBoxClipDataSet.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkFloatArray.h"
#include "vtkGenericCell.h"
#include "vtkImageData.h"
#include "vtkIntArray.h"
#include "vtkMergePoints.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkIdList.h"

#include <math.h>
#include <stdio.h>

vtkCxxRevisionMacro(vtkBoxClipDataSet, "$Revision$");
vtkStandardNewMacro(vtkBoxClipDataSet);

//----------------------------------------------------------------------------
vtkBoxClipDataSet::vtkBoxClipDataSet()
{
  this->Locator = NULL;
  this->GenerateClipScalars = 0;

  this->GenerateClippedOutput = 0;
  this->MergeTolerance = 0.01;

  this->vtkSource::SetNthOutput(1,vtkUnstructuredGrid::New());
  this->Outputs[1]->Delete();
  this->InputScalarsSelection = NULL;
  this->Orientation = 1;

}

//----------------------------------------------------------------------------
vtkBoxClipDataSet::~vtkBoxClipDataSet()
{
  if ( this->Locator )
    {
    this->Locator->UnRegister(this);
    this->Locator = NULL;
    }
  this->SetInputScalarsSelection(NULL);
}

//----------------------------------------------------------------------------
// Do not say we have two outputs unless we are generating the clipped output. 
int vtkBoxClipDataSet::GetNumberOfOutputs()
{
    if (this->GenerateClippedOutput)
    {
        return 2;
    }
    return 1;
}

//----------------------------------------------------------------------------
// Overload standard modified time function. If Clip functions is modified,
// then this object is modified as well.
unsigned long vtkBoxClipDataSet::GetMTime()
{
  unsigned long mTime=this->vtkDataSetToUnstructuredGridFilter::GetMTime();
  unsigned long time;

  if ( this->Locator != NULL )
    {
    time = this->Locator->GetMTime();
    mTime = ( time > mTime ? time : mTime );
    }

  return mTime;
}

//----------------------------------------------------------------------------
vtkUnstructuredGrid *vtkBoxClipDataSet::GetClippedOutput()
{
  if (this->NumberOfOutputs < 2)
    {
    return NULL;
    }
  
  return (vtkUnstructuredGrid *)(this->Outputs[1]);
}

//----------------------------------------------------------------------------
//
// Clip by box
//
void vtkBoxClipDataSet::Execute()
{
  vtkDataSet          *input = this->GetInput();
  vtkUnstructuredGrid *output = this->GetOutput();
  vtkUnstructuredGrid *clippedOutput = this->GetClippedOutput();
  if (input == NULL)
    {
    return;
    }
  vtkIdType      i;
  vtkIdType      npts;
  vtkIdType     *pts;
  vtkIdType      estimatedSize;
  vtkIdType      newCellId;
  vtkIdType      numPts = input->GetNumberOfPoints();
  vtkIdType      numCells = input->GetNumberOfCells();
  vtkPointData  *inPD=input->GetPointData(), *outPD = output->GetPointData();
  vtkCellData   *inCD=input->GetCellData();
  vtkCellData   *outCD[2];
  vtkPoints     *newPoints;
  vtkPoints     *cellPts;
  vtkIdList     *cellIds;
  vtkIdTypeArray   *locs[2];
  vtkDebugMacro(<< "Clip by Box");
  vtkUnsignedCharArray *types[2];

  int   j;
  int   cellType = 0;
  int   numOutputs = 1;
  int   inputObjectType = input->GetDataObjectType();
  
  // if we have volumes
  if (inputObjectType == VTK_STRUCTURED_POINTS || 
      inputObjectType == VTK_IMAGE_DATA)
    {
    int dimension;
    int *dims = vtkImageData::SafeDownCast(input)->GetDimensions();
    for (dimension=3, i=0; i<3; i++)
      {
      if ( dims[i] <= 1 )
        {
        dimension--;
        }
      }
    }
  
  // Initialize self; create output objects
  //
  if ( numPts < 1 )
    {
    //vtkErrorMacro(<<"No data to clip");
    return;
    }
  
  // allocate the output and associated helper classes
  estimatedSize = numCells;
  estimatedSize = estimatedSize / 1024 * 1024; //multiple of 1024
  if (estimatedSize < 1024)
    {
    estimatedSize = 1024;
    }
  vtkCellArray *conn[2];
  conn[0] = vtkCellArray::New();
  conn[0]->Allocate(estimatedSize,estimatedSize/2);
  conn[0]->InitTraversal();
  types[0] = vtkUnsignedCharArray::New();
  types[0]->Allocate(estimatedSize,estimatedSize/2);
  locs[0] = vtkIdTypeArray::New();
  locs[0]->Allocate(estimatedSize,estimatedSize/2);

  if ( this->GenerateClippedOutput )
    {
    numOutputs = 2;
    conn[1] = vtkCellArray::New();
    conn[1]->Allocate(estimatedSize,estimatedSize/2);
    conn[1]->InitTraversal();
    types[1] = vtkUnsignedCharArray::New();
    types[1]->Allocate(estimatedSize,estimatedSize/2);
    locs[1] = vtkIdTypeArray::New();
    locs[1]->Allocate(estimatedSize,estimatedSize/2);
    }

  newPoints = vtkPoints::New();
  newPoints->Allocate(numPts,numPts/2);
  
  // locator used to merge potentially duplicate points
  if ( this->Locator == NULL )
    {
    this->CreateDefaultLocator();
    }
  this->Locator->InitPointInsertion (newPoints, input->GetBounds());
  
  if ( !this->GenerateClipScalars && 
       !input->GetPointData()->GetScalars(this->InputScalarsSelection))
    {
    outPD->CopyScalarsOff();
    }
  else
    {
    outPD->CopyScalarsOn();
    }
  outPD->InterpolateAllocate(inPD,estimatedSize,estimatedSize/2);
  outCD[0] = output->GetCellData();
  outCD[0]->CopyAllocate(inCD,estimatedSize,estimatedSize/2);
  if ( this->GenerateClippedOutput )
    {
    outCD[1] = clippedOutput->GetCellData();
    outCD[1]->CopyAllocate(inCD,estimatedSize,estimatedSize/2);
    }

  //Process all cells and clip each in turn

  vtkIdType       updateTime = numCells/20 + 1;  // update roughly every 5%
  vtkGenericCell *cell       = vtkGenericCell::New();

  int abort=0;
  int num[2];
  int numNew[2]; 
  
  num[0]    = num[1]      = 0;
  numNew[0] = numNew[1]   = 0;

  unsigned orientation = GetOrientation();   //Test if there is a transformation 

  //clock_t init_tmp = clock();
  for (vtkIdType cellId=0; cellId < numCells && !abort; cellId++)
    {
    if ( !(cellId % updateTime) )
      {
      this->UpdateProgress((float)cellId / numCells);
      abort = this->GetAbortExecute();
      }
    
    input->GetCell(cellId,cell);
    cellPts = cell->GetPoints();
    cellIds = cell->GetPointIds();
    npts = cellPts->GetNumberOfPoints();
                
    if (this->GenerateClippedOutput) 
      {
      if (orientation)
        ClipHexahedronInOut(newPoints,cell,this->Locator, conn,
                    inPD, outPD, inCD, cellId, outCD);
      else
           ClipBoxInOut(newPoints,cell, this->Locator, conn,
                    inPD, outPD, inCD, cellId, outCD);

      numNew[0] = conn[0]->GetNumberOfCells() - num[0];
      numNew[1] = conn[1]->GetNumberOfCells() - num[1];
      num[0]    = conn[0]->GetNumberOfCells();
      num[1]    = conn[1]->GetNumberOfCells();
      } 
    else 
      {
      if (orientation)
        ClipHexahedron(newPoints,cell,this->Locator, conn[0],
                inPD, outPD, inCD, cellId, outCD[0]);
      else
        ClipBox(newPoints,cell, this->Locator, conn[0],
              inPD, outPD, inCD, cellId, outCD[0]);
    
      numNew[0] = conn[0]->GetNumberOfCells() - num[0];
      num[0]    = conn[0]->GetNumberOfCells();
      }  
    for (i=0 ; i<numOutputs; i++)  // for both outputs
      {
      for (j=0; j < numNew[i]; j++) 
        {
        locs[i]->InsertNextValue(conn[i]->GetTraversalLocation());
        conn[i]->GetNextCell(npts,pts);
        
        //For each new cell added, got to set the type of the cell
        switch ( cell->GetCellDimension() )
          {
          case 0: //points are generated-------------------------------
            cellType = (npts > 1 ? VTK_POLY_VERTEX : VTK_VERTEX);
            break;
          
          case 1: //lines are generated----------------------------------
            cellType = (npts > 2 ? VTK_POLY_LINE : VTK_LINE);
            break;
          
          case 2: //polygons are generated------------------------------
            cellType = (npts == 3 ? VTK_TRIANGLE : 
                 (npts == 4 ? VTK_QUAD : VTK_POLYGON));
            break;
          
          case 3: //tetrahedra are generated------------------------------
            cellType = VTK_TETRA;
            break;
           } //switch
        
        newCellId = types[i]->InsertNextValue(cellType);
        outCD[i]->CopyData(inCD, cellId, newCellId);
        } //for each new cell
      } // for both outputs
    } //for each cell
       
//  clock_t end_tmp = clock();
//  double  sec_tmp = ((double)end_tmp - init_tmp)/CLOCKS_PER_SEC;
//  fprintf(stdout," time : %f\n", sec_tmp);
  
  cell->Delete();
  
  output->SetPoints(newPoints);
  output->SetCells(types[0], locs[0], conn[0]);

  conn[0]->Delete();
  types[0]->Delete();
  locs[0]->Delete();
  
  if ( this->GenerateClippedOutput )
    {
    clippedOutput->SetPoints(newPoints);
    clippedOutput->SetCells(types[1], locs[1], conn[1]);
    conn[1]->Delete();
    types[1]->Delete();
    locs[1]->Delete();
    }
  newPoints->Delete();
  this->Locator->Initialize();//release any extra memory
  output->Squeeze();
}

//----------------------------------------------------------------------------
// Specify a spatial locator for merging points. By default, 
// an instance of vtkMergePoints is used.
void vtkBoxClipDataSet::SetLocator(vtkPointLocator *locator)
{
  if ( this->Locator == locator)
    {
    return;
    }
  
  if ( this->Locator )
    {
    this->Locator->UnRegister(this);
    this->Locator = NULL;
    }

  if ( locator )
    {
    locator->Register(this);
    }

  this->Locator = locator;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoxClipDataSet::CreateDefaultLocator()
{
  if ( this->Locator == NULL )
    {
    this->Locator = vtkMergePoints::New();
    this->Locator->Register(this);
    this->Locator->Delete();
    }
}

//----------------------------------------------------------------------------
// Set the box for clipping
// for each plane, specify the normal and one vertex on the plane. 
//
void vtkBoxClipDataSet::SetBoxClip(float *n0,float *o0,float *n1,float *o1,
                                   float *n2,float *o2,float *n3,float *o3,
                                   float *n4,float *o4,float *n5,float *o5)
{
  double N0[3], O0[3];
  double N1[3], O1[3];
  double N2[3], O2[3];
  double N3[3], O3[3];
  double N4[3], O4[3];
  double N5[3], O5[3];

  for (int i=0; i<3; i++)
    {
    N0[i] = (double)n0[i]; O0[i] = (double)o0[i];
    N1[i] = (double)n1[i]; O1[i] = (double)o1[i];
    N2[i] = (double)n2[i]; O2[i] = (double)o2[i];
    N3[i] = (double)n3[i]; O3[i] = (double)o3[i];
    N4[i] = (double)n4[i]; O4[i] = (double)o4[i];
    N5[i] = (double)n5[i]; O5[i] = (double)o5[i];
    }

  this->SetBoxClip(N0, O0, N1, O1, N2, O2, N3, O3, N4, O4, N5, O5);
}
void vtkBoxClipDataSet::SetBoxClip(double *n0,double *o0,double *n1,double *o1,
                                   double *n2,double *o2,double *n3,double *o3,
                                   double *n4,double *o4,double *n5,double *o5)
{
  int i;

  for ( i=0;i<3;i++) 
    {
    n_pl[0][i] = n0[i];
    o_pl[0][i] = o0[i];
    }
  for ( i=0;i<3;i++) 
    {
    n_pl[1][i] = n1[i];
    o_pl[1][i] = o1[i];
    }
  for ( i=0;i<3;i++) 
    {
    n_pl[2][i] = n2[i];
    o_pl[2][i] = o2[i];
    }
  for ( i=0;i<3;i++) 
    {
    n_pl[3][i] = n3[i];
    o_pl[3][i] = o3[i];
    }
  for ( i=0;i<3;i++) 
    {
    n_pl[4][i] = n4[i];
    o_pl[4][i] = o4[i];
    }
  for ( i=0;i<3;i++) 
    {
    n_pl[5][i] = n5[i];
    o_pl[5][i] = o5[i];
    }
}

//----------------------------------------------------------------------------
// Specify the bounding box for clipping

void vtkBoxClipDataSet::SetBoxClip(float xmin,float xmax,
                                   float ymin,float ymax,
                                   float zmin,float zmax)
{
  this->SetBoxClip((double)xmin, (double)xmax,
                   (double)ymin, (double)ymax,
                   (double)zmin, (double)zmax);
}
void vtkBoxClipDataSet::SetBoxClip(double xmin,double xmax,
                                   double ymin,double ymax,
                                   double zmin,double zmax)
{
  SetOrientation(0);
  BoundBoxClip[0][0] = xmin;
  BoundBoxClip[0][1] = xmax;
  BoundBoxClip[1][0] = ymin;
  BoundBoxClip[1][1] = ymax;
  BoundBoxClip[2][0] = zmin;
  BoundBoxClip[2][1] = zmax;
}

//----------------------------------------------------------------------------
void vtkBoxClipDataSet::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Merge Tolerance: " << this->MergeTolerance << "\n";
  
  if ( this->Locator )
    {
    os << indent << "Locator: " << this->Locator << "\n";
    }
  else
    {
    os << indent << "Locator: (none)\n";
    }

  os << indent << "Generate Clip Scalars: " 
     << (this->GenerateClipScalars ? "On\n" : "Off\n");


  if (this->InputScalarsSelection)
    {
    os << indent << "InputScalarsSelection: " 
       << this->InputScalarsSelection << endl;
    }
}

//----------------------------------------------------------------------------
// TetraGrid: Subdivide cells in consistent tetrahedra.
// Case : Voxel(11) or Hexahedron(12).
//
// MinEdgF search the smallest vertex index in linear order of a face(4 vertices)
//
void vtkBoxClipDataSet::MinEdgeF(unsigned *id_v, vtkIdType *cellIds,unsigned *edgF)
{
    
  int i;
  unsigned int    id;
  int   ids;
  int min_f; 

  ids   = 0;
  id    = id_v[0];   //Face index     
  min_f = cellIds[id_v[0]];

  for(i=1;i<4;i++)
    {
    if(min_f > cellIds[id_v[i]])
      {
      min_f = cellIds[id_v[i]];
      id = id_v[i];            
      ids= i;
      }  
    }

  switch(ids)
    {
    case 0:
      if(id < id_v[2])
        {
        edgF[0] = id;
        edgF[1] = id_v[2];
        }
      else
        {
        edgF[0] = id_v[2];
        edgF[1] = id;
        }
      break;
    case 1:
      if(id < id_v[3])
        {
        edgF[0] = id;
        edgF[1] = id_v[3];
        }
      else
        {
        edgF[0] = id_v[3];
        edgF[1] = id;
        }
      break;
    case 2:
      if(id < id_v[0])
        {
        edgF[0] = id;
        edgF[1] = id_v[0];
        }
      else
        {
        edgF[0] = id_v[0];
        edgF[1] = id;
        }
      break;
  
    case 3:
      if(id < id_v[1])
        {
        edgF[0] = id;
        edgF[1] = id_v[1];
        }
      else
        {
        edgF[0] = id_v[1];
        edgF[1] = id;
        }
    break;
    }
}

//----------------------------------------------------------------------------
// TetraGrid: Subdivide cells in consistent tetrahedra.
//
// Case : Voxel or Hexahedron: 
//       if ( subdivide voxel  in 6 tetrahedra)
//           voxel : 2 wedges (*)
//       else 
//           voxel : 5 tetrahedra 
// 
// Case : Wedge (*)
// 
// ------------------------------------------------
//
//(*) WedgeToTetra: subdivide one wedge in  3 tetrahedra
//
//        wedge : 1 tetrahedron + 1 pyramid = 3 tetrahedra. 
//
void vtkBoxClipDataSet::WedgeToTetra(vtkIdType *wedgeId, vtkIdType *cellIds,vtkCellArray *newCellArray)
{
  int i;
  int id;
  vtkIdType xmin;
  vtkIdType newCellId;
  vtkIdType tab[4];
  vtkIdType tabpyram[5];

  static  vtkIdType vwedge[6][4]={{0,4,3,5},{1,4,3,5},{2,4,3,5},
                                          {3,0,1,2},{4,0,1,2},{5,0,1,2}};
        
  // the table 'vwedge' set 6 possibilities of the smallest index 
  //
  //             v5       
  //             /\       .
  //         v3 /..\ v4
  //           /   /
  //        v2/\  /
  //       v0/__\/v1
  //   if(v0 index ) is the smallest index:
  //   wedge is subdivided in:
  //         1 tetrahedron-> vwedge[0]: {v0,v4,v3,v5}
  //     and 1 pyramid    -> vert[0]  : {v1,v2,v5,v4,v0}
  //

  id = 0;     
  xmin = cellIds[wedgeId[0]];
  for(i=1;i<6;i++) 
    {
    if(xmin > cellIds[wedgeId[i]])
      {
      xmin = cellIds[wedgeId[i]];// the smallest global index
      id = i;                    // local index 
      }  
    }
  for (i =0;i<4;i++)
    {
    tab[i] = wedgeId[vwedge[id][i]];
    }
  newCellId = newCellArray->InsertNextCell(4,tab);
  
  // Pyramid :create 2 tetrahedra
  
  static vtkIdType vert[6][5]={{1,2,5,4,0},{2,0,3,5,1},{3,0,1,4,2},
               {1,2,5,4,3},{2,0,3,5,4},{3,0,1,4,5}};  
  for(i=0;i<5;i++)
    {
    tabpyram[i] = wedgeId[vert[id][i]];
    }
  PyramidToTetra(tabpyram,cellIds,newCellArray);
}

//----------------------------------------------------------------------------
// TetraGrid: Subdivide cells in consistent tetrahedra.
//
// PyramidToTetra :Subdivide the pyramid in consistent tetrahedra.
//        Pyramid : 2 tetrahedra.
//
void  vtkBoxClipDataSet::PyramidToTetra(vtkIdType *pyramId, vtkIdType *cellIds,vtkCellArray *newCellArray)
{
  vtkIdType xmin;
  vtkIdType newCellId;
  unsigned i,j,idpy;
  vtkIdType tab[4];

  // the table 'vpy' set  3  possibilities of the smallest index 
  // vertices{v0,v1,v2,v3,v4}. {v0,v1,v2,v3} is a square face of pyramid
  //
  //                v4      
  //                ^                
  //                                  
  //                       
  //           v3 _ _ __ _  v2     
  //           /         /     
  //        v0/_ _ _ _ _/v1         
  //                                  
  //   if(v0 index ) is the smallest index:
  //   the pyramid is subdivided in:
  //         2 tetrahedra-> vpy[0]: {v0,v1,v2,v4}
  //                        vpy[1]: {v0,v2,v3,v4}
  //    
  static vtkIdType vpy[8][4] ={{0,1,2,4},{0,2,3,4},{1,2,3,4},{1,3,0,4},
                                       {2,3,0,4},{2,0,1,4},{3,0,1,4},{3,1,2,4}}; 
                   
  xmin    = cellIds[pyramId[0]];
  idpy = 0;
  for(i=1;i<4;i++) 
    {
    if(xmin > cellIds[pyramId[i]])
      {
      xmin = cellIds[pyramId[i]];  // global index 
      idpy = i;                    // local index
      }
    }
  for(j = 0; j < 4 ; j++) 
    {
    tab[j] = pyramId[vpy[2*idpy][j]];
    }
  newCellId = newCellArray->InsertNextCell(4,tab);
  
  for(j = 0; j < 4 ; j++) 
    {
    tab[j] = pyramId[vpy[2*idpy+1][j]];
    }
  newCellId = newCellArray->InsertNextCell(4,tab);  
}


//----------------------------------------------------------------------------
//Tetra Grid : Subdivide cells in  consistent tetrahedra.
//             For each cell, search the smallest global index. 
//
//  Case Tetrahedron(10): Just insert this cell in the newCellArray
//
//  Case Voxel(11) or Hexahedron(12): 
//       - for each face: looking for the diagonal edge with the smallest index
//       - 2 possibilities: subdivide a cell in 5 or 6 tetrahedra
//
//         (I)Case 6 tetrahedra: 
//                  - 6 possibilities: subdivide de cell in 2 wedges: 
//                   (1) diagonal edges (v0,v5),(v2,v7) 
//                       vwedge[0]={0,5,4,2,7,6},
//                       vwedge[1]={0,1,5,2,3,7},
//                   (2) diagonal edges (v4,v7),(v0,v3)
//                       vwedge[2]={4,7,6,0,3,2},
//                       vwedge[3]={4,5,7,0,1,3},
//                   (3) diagonal edges (v0,v6),(v1,v7)
//        vwedge[4]= {1,7,5,0,6,4},
//                       vwedge[5]{1,3,7,0,2,6},
//                       subdivide de cell in 2 wedges:
//                   (4) diagonal edges (v1,v2),(v5,v6)
//                       vwedge[6]={4,5,6,0,1,2},
//                       vwedge[7]={6,5,7,2,1,3},
//                   (5) diagonal edges (v2,v4),(v3,v5)
//                       vwedge[8]={3,7,5,2,6,4},
//                       vwedge[9]={1,3,5,0,2,4},
//                   (6) diagonal edges (v1,v4),(v3,v6)
//                       vwedge[10]={0,1,4,2,3,6}
//                       vwedge[11]={1,5,4,3,7,6}
//         v6 _ _ __ _  v7     
//           /|        /|           VOXEL
//        v4/_|_ _ _ _/ |           opposite vertex of v0 is v7 and vice-versa
//          | |     v5| |           diagonal edges Edg_f[i]
//          |v2 _ _ _ |_|  v3        
//          |/        |/             
//        v0/_ _ _ _ _|/v1            
//
// 
//     (II)Case 5 tetrahedra: 
//       - search the smallest vertex vi
//       - verify if the opposite vertices of vi do not belong to any diagonal edges Edg_f
//       - 2 possibilites: create 5 tetraedra
//           - if vi is ( 0 or 3 or 5 or 6)  
//             vtetra[]={v0,v5,v3,v6},{v0,v4,v5,v6},{v0,v1,v3,v5},{v5,v3,v6,v7},{v0,v3,v2,v6}};      
//           - if vi is ( 1 or 2 or 4 or 7)                       
//             vtetra[]={v1,v2,v4,v7},{v0,v1,v2,v4},{v1,v4,v5,v7},{v1,v3,v2,v7},{v2,v6,v4,v7}};
//  Case Wedge (13): 
//       the table 'vwedge' set 6 possibilities of the smallest index 
//
//                 v5       
//               /\            .
//           v3 /..\ v4
//             /   /
//          v2/\  /
//         v0/__\/v1
//
//        if(v0 index ) is the smallest index:
//         wedge is subdivided in:
//              1 tetrahedron-> vwedge[0]: {v0,v4,v3,v5}
//               and 1 pyramid-> vert[0] : {v1,v2,v5,v4,v0}
//
//  Case Pyramid (14):
//       the table 'vpy' set  3  possibilities of the smallest index 
//       vertices{v0,v1,v2,v3,v4}. {v0,v1,v2,v3} is a square face of pyramid
//
//                v4  (opposite vertex of face with 4 vertices)  
//                ^                
//                                  
//                       
//          v3 _ _ __ _  v2     
//           /         /     
//        v0/_ _ _ _ _/v1         
//                                  
//        if(v0 index ) is the smallest index:
//        the pyramid is subdivided in:
//        2 tetrahedra-> vpyram[0]: {v0,v1,v2,v4}
//                       vpyram[1]: {v0,v2,v3,v4}
//  
//
//----------------------------------------------------------------------------

void vtkBoxClipDataSet::TetraGrid(vtkIdType typeobj, vtkIdType npts, vtkIdType *cellIds,vtkCellArray *newCellArray)
{
  vtkIdType tab[4];
  vtkIdType tabp[5];
  vtkIdType ptstetra = 4;
  vtkIdType newCellId;
  vtkIdType xmin;  
  unsigned i,j;
  unsigned id   =0;
  unsigned idpy =0;
        
  unsigned Edg_f[6][2]; //edge selected of each face
  unsigned idv[4];
    
  unsigned idopos;
  unsigned numbertetra;

  static  vtkIdType tetra[4]={0,1,2,3};
  static unsigned tabopos[8] = {6,7,4,5,2,3,0,1};

  switch(typeobj)
    {
    case VTK_TETRA:
      newCellId = newCellArray->InsertNextCell(ptstetra,tetra);
      break;  
    case VTK_VOXEL:
      // each face: search edge with smallest global index  
      // face 0 (0,1,5,4)
      idv[0]= 0;
      idv[1]= 1;
      idv[2]= 5;
      idv[3]= 4;
      MinEdgeF(idv,cellIds,Edg_f[0]);
      
      // face 1 (0,1,3,2)
      idv[0]= 0;
      idv[1]= 1;
      idv[2]= 3;
      idv[3]= 2;
      MinEdgeF(idv,cellIds,Edg_f[1]);
      // face 2 (0,2,6,4)
      idv[0]= 0;
      idv[1]= 2;
      idv[2]= 6;
      idv[3]= 4;
      MinEdgeF(idv,cellIds,Edg_f[2]);
      // face 3 (4,5,7,6)
      idv[0]= 4;
      idv[1]= 5;
      idv[2]= 7;
      idv[3]= 6;
      MinEdgeF(idv,cellIds,Edg_f[3]);
      // face 4 (2,3,7,6)
      idv[0]= 2;
      idv[1]= 3;
      idv[2]= 7;
      idv[3]= 6;
      MinEdgeF(idv,cellIds,Edg_f[4]);
      // face 5 (1,3,7,5)
      idv[0]= 1;
      idv[1]= 3;
      idv[2]= 7;
      idv[3]= 5;
      MinEdgeF(idv,cellIds,Edg_f[5]);
  
      //search the smallest global index of voxel 
      xmin = cellIds[0];
      id = 0;
      for(i=1;i<8;i++) 
        {
        if(xmin > cellIds[i])
          {
          xmin = cellIds[i];// the smallest global index
          id = i;           // local index 
          }  
        }
      //two cases: 
      idopos      = 7 - id;
      numbertetra = 5;
      for(i=0;i<6;i++) 
        {
        j=0;
        if (idopos == Edg_f[i][j]) 
          {
          numbertetra = 6;
          break;
          }
        j=1;
        if (idopos == Edg_f[i][j]) 
          {
          numbertetra = 6;
          break;
          }
        }
  
      if(numbertetra == 5) 
        {
        // case 1: create  5 tetraedra 
        if((id == 0)||(id == 3)||(id == 5)||(id == 6))
          {
          static  vtkIdType vtetra[5][4]={{0,5,3,6},{0,4,5,6},
                                      {0,1,3,5},{5,3,6,7},{0,3,2,6}}; 
          for(i=0; i<5; i++)
            {
            newCellId = newCellArray->InsertNextCell(4,vtetra[i]);
            }
          }
        else
          {
          static  vtkIdType vtetra[5][4]={{1,2,4,7},{0,1,2,4},
                                      {1,4,5,7}, {1,3,2,7},{2,6,4,7}};   
          
          for(i=0; i<5; i++)
            {
            newCellId = newCellArray->InsertNextCell(4,vtetra[i]);
            }
          }
        }
      else 
        {
        //case 2: create 2 wedges-> 6 tetrahedra
        static vtkIdType vwedge[12][6]={{0,5,4,2,7,6},{0,1,5,2,3,7},
                  {4,7,6,0,3,2},{4,5,7,0,1,3},
                  {1,7,5,0,6,4},{1,3,7,0,2,6},
                  {4,5,6,0,1,2},{6,5,7,2,1,3},
                  {3,7,5,2,6,4},{1,3,5,0,2,4},
                  {0,1,4,2,3,6},{1,5,4,3,7,6}}; 
        unsigned edgeId = 10*Edg_f[i][0]+ Edg_f[i][1];
        switch(edgeId)     
          {
          case 5:   // edge(v0,v5):10*0 + 5
          case 27:  // edge(v2,v7):10*2 + 7
            WedgeToTetra(vwedge[0],cellIds,newCellArray);
            WedgeToTetra(vwedge[1],cellIds,newCellArray);
            break;
          case 3:   // edge(v0,v2)
          case 47:  // edge(v4,v6)
            WedgeToTetra(vwedge[2],cellIds,newCellArray);
            WedgeToTetra(vwedge[3],cellIds,newCellArray);
            break;
          case 6:
          case 17:
            WedgeToTetra(vwedge[4],cellIds,newCellArray);
            WedgeToTetra(vwedge[5],cellIds,newCellArray);
            break;
          case 12:
          case 56:
            WedgeToTetra(vwedge[6],cellIds,newCellArray);
            WedgeToTetra(vwedge[7],cellIds,newCellArray);
            break;
          case 24:
          case 35:
            WedgeToTetra(vwedge[8],cellIds,newCellArray);
            WedgeToTetra(vwedge[9],cellIds,newCellArray);
            break;
          case 14:
          case 36:
            WedgeToTetra(vwedge[10],cellIds,newCellArray);
            WedgeToTetra(vwedge[11],cellIds,newCellArray);
            break;
          }
        }
        break;
    case VTK_HEXAHEDRON:
      // each face: search edge with smallest global index  
      // face 0 (0,1,5,4)
      idv[0]= 0;
      idv[1]= 1;
      idv[2]= 5;
      idv[3]= 4;
      MinEdgeF(idv,cellIds,Edg_f[0]);
      
      // face 1 (0,1,2,3)
      idv[0]= 0;
      idv[1]= 1;
      idv[2]= 2;
      idv[3]= 3;
      MinEdgeF(idv,cellIds,Edg_f[1]);
      // face 2 (0,3,7,4)
      idv[0]= 0;
      idv[1]= 3;
      idv[2]= 7;
      idv[3]= 4;
      MinEdgeF(idv,cellIds,Edg_f[2]);
      // face 3 (4,5,6,7)
      idv[0]= 4;
      idv[1]= 5;
      idv[2]= 6;
      idv[3]= 7;
      MinEdgeF(idv,cellIds,Edg_f[3]);
      // face 4 (3,2,6,7)
      idv[0]= 3;
      idv[1]= 2;
      idv[2]= 6;
      idv[3]= 7;
      MinEdgeF(idv,cellIds,Edg_f[4]);
      // face 5 (1,2,6,5)
      idv[0]= 1;
      idv[1]= 2;
      idv[2]= 6;
      idv[3]= 5;
      MinEdgeF(idv,cellIds,Edg_f[5]);
  
      //search the smallest global index of voxel 
      xmin = cellIds[0];
      id = 0;
      for(i=1;i<8;i++) 
        {
        if(xmin > cellIds[i])
          {
          xmin = cellIds[i];// the smallest global index
          id = i;           // local index 
          }  
        }
  
      //two cases: 
      idopos      = tabopos[id];
      numbertetra = 5;
      for(i=0;i<6;i++) 
        {
        j=0;
        if (idopos == Edg_f[i][j]) 
          {
          numbertetra = 6;
          break;
          }
        j=1;
        if (idopos == Edg_f[i][j]) 
          {
          numbertetra = 6;
          break;
          }
        }
  
      if(numbertetra == 5) 
        {
        // case 1: create  5 tetraedra 
        if((id == 0)||(id == 2)||(id == 5)||(id == 7))
          {
          static  vtkIdType vtetra[5][4]={{0,5,2,7},{0,4,5,7},
                                      {0,1,2,5},{5,2,7,6},{0,2,3,7}}; 
          for(i=0; i<5; i++)
            {
            newCellId = newCellArray->InsertNextCell(4,vtetra[i]);
            }
          }
        else
          {
          static  vtkIdType vtetra[5][4]={{1,3,4,6},{0,1,3,4},
                                           {1,4,5,6},{1,2,3,6},{3,7,4,6}};   
          
          for(i=0; i<5; i++)
            {
            newCellId = newCellArray->InsertNextCell(4,vtetra[i]);
            }
          }
        } 
      else 
        {
        //case 2: create 2 wedges-> 6 tetrahedra
        static vtkIdType vwedge[12][6]={{0,5,4,3,6,7},{0,1,5,3,2,6},
                  {4,6,7,0,2,3},{4,5,6,0,1,2},
                  {1,6,5,0,7,4},{1,2,6,0,3,7},
                  {4,5,7,0,1,3},{7,5,6,3,1,2},
                  {2,6,5,3,7,4},{1,2,5,0,3,4},
                  {0,1,4,3,2,7},{1,5,4,2,6,7}}; 
        unsigned edgeId = 10*Edg_f[i][0]+ Edg_f[i][1];

        switch(edgeId)
          {
          case 5:  // edge(v0,v5):10*0 + 5
          case 36: // edge(v3,v6):10*3 + 6
            WedgeToTetra(vwedge[0],cellIds,newCellArray);
            WedgeToTetra(vwedge[1],cellIds,newCellArray);
            break;
          case 2:  // edge(v0,v2)
          case 46: // edge(v4,v6)
            WedgeToTetra(vwedge[2],cellIds,newCellArray);
            WedgeToTetra(vwedge[3],cellIds,newCellArray);
            break;
          case 7:
          case 16:
            WedgeToTetra(vwedge[4],cellIds,newCellArray);
            WedgeToTetra(vwedge[5],cellIds,newCellArray);
            break;
          case 13:
          case 57:
            WedgeToTetra(vwedge[6],cellIds,newCellArray);
            WedgeToTetra(vwedge[7],cellIds,newCellArray);
            break;
          case 34:
          case 25:
            WedgeToTetra(vwedge[8],cellIds,newCellArray);
            WedgeToTetra(vwedge[9],cellIds,newCellArray);
            break;
          case 14:
          case 27:
            WedgeToTetra(vwedge[10],cellIds,newCellArray);
            WedgeToTetra(vwedge[11],cellIds,newCellArray);
            break;
          }
        }
      break;      
    case VTK_WEDGE:
      if(npts == 6) //create 3 tetrahedra
        {
        //first tetrahedron 
        static  vtkIdType vwedge[6][4]={{0,4,3,5},{1,4,3,5},{2,4,3,5},
                                       {3,0,1,2},{4,0,1,2},{5,0,1,2}};             
        xmin = cellIds[0];
        id = 0;
        for(i=1;i<6;i++) 
          {
          if(xmin > cellIds[i])
            {
            xmin = cellIds[i];// the smallest global index
            id = i;           // local index 
            }  
          }
        newCellId = newCellArray->InsertNextCell(4,vwedge[id]);
        
        //Pyramid :create 2 tetrahedra
           
        static vtkIdType vert[6][5]={{1,2,5,4,0},{2,0,3,5,1},{3,0,1,4,2},
                                   {1,2,5,4,3},{2,0,3,5,4},{3,0,1,4,5}};  
        static vtkIdType vpy[8][4] ={{0,1,2,4},{0,2,3,4},{1,2,3,4},{1,3,0,4},
                                   {2,3,0,4},{2,0,1,4},{3,0,1,4},{3,1,2,4}};                      
        xmin    = cellIds[vert[id][0]];
        tabp[0] = vert[id][0];
        idpy = 0;
        for(i=1;i<4;i++) 
          {
          tabp[i] = vert[id][i];
          if(xmin > cellIds[vert[id][i]])
            {
            xmin = cellIds[vert[id][i]]; // global index 
            idpy = i;                    // local index
            }
          }
        tabp[4] = cellIds[vert[id][4]];
        for(j = 0; j < 4 ; j++) 
          {
          tab[j] = tabp[vpy[2*idpy][j]];
          }
        newCellId = newCellArray->InsertNextCell(4,tab);
        
        for(j = 0; j < 4 ; j++) 
          {
          tab[j] = tabp[vpy[2*idpy+1][j]];
          }
        newCellId = newCellArray->InsertNextCell(4,tab);
        
        }
      else
        {
        fprintf(stdout," ERROR: this cell is not a wedge\n");
        }
      break;
  
    case VTK_PYRAMID: //Create 2 tetrahedra
      if(npts== 5)
        {
        //note: the first element vpyram[][0] is the smallest index of pyramid
        static  vtkIdType vpyram[8][4]={{0,1,2,4},{0,2,3,4},{1,2,3,4},{1,3,0,4},
                  {2,3,0,4},{2,0,1,4},{3,0,1,4},{3,1,2,4}};
        xmin = cellIds[0];
        id   = 0;
        for(i=1;i<4;i++) 
          {
          if(xmin > cellIds[i])
            {
            xmin = cellIds[i]; // the smallest global index of square face 
            id = i;            // local index
            }          
          }
        newCellId = newCellArray->InsertNextCell(4,vpyram[2*id]);
        newCellId = newCellArray->InsertNextCell(4,vpyram[2*id+1]);
        }
      else
        {
        fprintf(stdout," ERROR: this cell is not a pyramid\n");
        exit(0);
        }
      break;

    default:

      cout << " unhandled type, awaiting fix for this " << typeobj << endl;
    }
}

//----------------------------------------------------------------------------
// The new cell  created in  intersection between tetrahedron and plane
// are tetrahedron or wedges or pyramides.
//
// CreateTetra is used to subdivide wedges and pyramides in tetrahedron.
//  
void vtkBoxClipDataSet::CreateTetra(vtkIdType npts,vtkIdType *cellIds,vtkCellArray *newCellArray)
{
  vtkIdType tabp[5];
  vtkIdType tab[3][4];
  
  unsigned i,j;
  unsigned id =0;
  unsigned idpy;

  vtkIdType xmin;
  vtkIdType newCellId;
        
  if (npts == 6) 
    { 
    //VTK_WEDGE: Create 3 tetrahedra
    //first tetrahedron 
    static  vtkIdType vwedge[6][4]={{0,4,3,5},{1,4,3,5},{2,4,3,5},
              {3,0,1,2},{4,0,1,2},{5,0,1,2}};             
    xmin = cellIds[0];
    id = 0;
    for(i=1;i<6;i++) 
      {
      if(xmin > cellIds[i])
        {
        xmin = cellIds[i];// the smallest global index
        id = i;           // local index 
        }  
      }     
    for(j = 0; j < 4 ; j++) 
      {
      tab[0][j] = cellIds[vwedge[id][j]];
      }  
    newCellId = newCellArray->InsertNextCell(4,tab[0]);
      
    //Pyramid: create 2 tetrahedra
  
    static vtkIdType vert[6][5]= {{1,2,5,4,0},{2,0,3,5,1},{3,0,1,4,2},
                  {1,2,5,4,3},{2,0,3,5,4},{3,0,1,4,5}};  
    static vtkIdType vpy[8][4] = {{0,1,2,4},{0,2,3,4},{1,2,3,4},{1,3,0,4},
                  {2,3,0,4},{2,0,1,4},{3,0,1,4},{3,1,2,4}};                      
    xmin    = cellIds[vert[id][0]];
    tabp[0] = vert[id][0];
    idpy = 0;
    for(i=1;i<4;i++) 
      {
      tabp[i] = vert[id][i];
      if(xmin > cellIds[vert[id][i]])
        {
        xmin = cellIds[vert[id][i]]; // global index 
        idpy = i;                    // local index
        }
      }
    tabp[4] = vert[id][4];
    for(j = 0; j < 4 ; j++) 
      {
      tab[1][j] = cellIds[tabp[vpy[2*idpy][j]]];
      }
    newCellId = newCellArray->InsertNextCell(4,tab[1]);
      
    for(j = 0; j < 4 ; j++) 
      {
      tab[2][j] = cellIds[tabp[vpy[2*idpy+1][j]]];    
      }
    newCellId = newCellArray->InsertNextCell(4,tab[2]);
      
    }
  else 
    {  
    //VTK_PYRAMID: Create 2 tetrahedra
    //The first element in each set is the smallest index of pyramid
    static  vtkIdType vpyram[8][4]={{0,1,2,4},{0,2,3,4},{1,2,3,4},{1,3,0,4},
                                      {2,3,0,4},{2,0,1,4},{3,0,1,4},{3,1,2,4}};
    xmin = cellIds[0];
    id   = 0;
    for(i=1;i<4;i++) 
      {
      if(xmin > cellIds[i])
        {
        xmin = cellIds[i]; // the smallest global index of face with 4 vertices 
        id = i;            // local index
        }          
      }
    for(j = 0; j < 4 ; j++) 
      {
      tab[0][j] = cellIds[vpyram[2*id][j]];
      }
    newCellId = newCellArray->InsertNextCell(4,tab[0]);
    
    for(j = 0; j < 4 ; j++) 
      {
      tab[1][j] = cellIds[vpyram[2*id+1][j]];
      }
    newCellId = newCellArray->InsertNextCell(4,tab[1]);
    } 
}

//----------------------------------------------------------------------------
// Clip each cell of an unstructured grid.
// 
//----------------------------------------------------------------------------   
//(1) How decide when the cell is NOT outside
//
//    Explaining with an example in 2D. 
//    Look at 9 region in the picture and the triangle represented there.
//    v0,v1,v2 are vertices of triangle T.
//
//              |         |        
//        1     |   2     |     3       
//     _ _ _ _ _ _ _ _ _  _ _ _ _ _ ymax
//              |         | v1        
//        4     |   5     |/\   6      
//              |         /  \             .
//     _ _ _ _ _ _ _ _ _ /| _ \_ _ _ymin
//              |     v2/_|_ _ \v0        
//        7     |   8     |     9  
//            xmin       xmax
//
// set test={1,1,1,1} (one test for each plane: test={xmin,xmax,ymin,ymax} )
//    for each vertex, if the test is true set 0 in the test table:
//    vO > xmin ?, v0 < xmax ?, v0 > ymin ?, v0 < ymax ?
//    In the example: test={0,1,1,0}
//    v1 > xmin ?, v1 < xmax ?, v1 > ymin ?, v1 < ymax ?
//    In the example: test={0,1,0,0}
//    v2 > xmin ?, v2 < xmax ?, v2 > ymin ?, v2 < ymax ?
//    In the Example: test={0,0,0,0} -> triangle is NOT outside.
//
//
//    In general, look at the possibilities of each region
//
//    (1,0,0,1)  | (0,0,0,1)  | (0,1,0,1)
//    - - - - - - - - - - - - - - - - - - 
//    (1,0,0,0)  | (0,0,0,0)  | (0,1,0,0)
//    - - - - - - - - - - - - - - - - - - 
//    (1,0,1,0)  | (0,0,1,0)  | (0,1,1,0)
//
//    In the case above we have:(v0,v1,v2)
//    (1,1,1,1)->(0,1,1,0)->(0,1,0,0)->(0,0,0,0)
//
//    If you have one vertex in region 5, this triangle is NOT outside
//    The triangle IS outside if (v0,v1,v2) are in region like 
//    (1,2,3): (1,0,0,1)->(0,0,0,1)->(0,0,0,1)
//    (1,4,7): (1,0,0,1)->(1,0,0,0)->(1,0,0,0)
//    (7,8,9): (1,0,1,0)->(0,0,1,0)->(0,0,1,0)
//    (9,6,3): (0,1,1,0)->(0,1,0,0)->(0,1,0,0) 
//
//    Note that if the triangle T is on the region 5, T is NOT outside
//    In fact, T is inside 
//
//    You can extend this idea to 3D.
//
//    Note: xmin = BoundBoxClip[0][0], xmax=  BoundBoxClip[0][1],... 
// 
//----------------------------------------------------------------------------  
// (2) Intersection between Tetrahedron and Plane:
//     Description: 
//         vertices of tetrahedron {v0,v1,v2,v3}
//         edge e1 : (v0,v1),   edge e2 : (v1,v2)
//         edge e3 : (v2,v0),   edge e4 : (v0,v3)
//         edge e5 : (v1,v3),   edge e6 : (v2,v3)
//         
//         interseting points p0, pi, ...
//
//         Note: The algorithm search intersection
//               points with these edge order.
//
//          v3                        v3
//       e6/|\ e5                     | 
//        / | \            e2         | 
//     v2/_ |_ \ v1    v2 - - -v1     |e4 
//       \  |  /                      |
//      e3\ | /e1                     |
//         \|/                        |
//          v0                        v0
//
//    (a) Intersecting  4 edges: see tab4[] 
//                    -> create: 2 wedges
//                    -> 3 cases
//                    ------------------------------------------
//        case 1246:   
//                   if (plane intersecting  edges {e1,e2,e4,e6})
//                   then  p0 belongs to e1, p1 belongs to e2,
//                         p2 belongs to e4, p3 belongs to e6.
//
//                                    v3
//                                     |            v3 
//                                     |            /
//                v1    v2 - *- -v1    |           * p3
//              /            p1        * p2       /
//             *p0                     |       v2/
//           /                         |
//          v0                        v0
//          (e1)          (e2)       (e4)        (e6) 
//
//                   So, there are two wedges:
//                   {p0,v1,p1,p2,v3,p3}  {p2,v0,p0,p3,v2,p1}
// 
//                   Note: if e1 and e2 are intersected by plane,
//                         and the plane intersects 4 edges,
//                         the edge e5 could not be intersected
//                         (skew lines, do not create a planar face)
//                         neither the edge e3((e1,e2,e3) is a face)
//
//                    ------------------------------------------
//        case 2345:   
//                   if (plane intersecting  edges {e2,e3,e4,e5})
//                   The two wedges are:
//                   {p3,v3,p2,p0,v2,p1}, {p1,v0,p2,p0,v1,p3}
//                         
//                    ------------------------------------------
//        case 1356:   
//                   if (plane intersecting  edges {e1,e3,e5,e6})
//                   The two wedges are:
//                   {p0,v0,p1,p2,v3,p3}, {p0,v1,p2,p1,v2,p3}
//
//                    -----------------------------------------
//
//    (b) Intersecting  3 edges: tab3[]
//                    ->create: 1 tetrahedron + 1 wedge
//                    -> 4 cases
//                    ------------------------------------------
//    case 134:
//                   if (plane intersecting  edges {e1,e3,e4})
//                   then  p0 belongs to e1, p1 belongs to e3,
//                         p2 belongs to e4.
//
//                           v3
//                           |             
//                           |            
//                     v2   *p2   v1      
//                       \   |   /  
//                      p1*  |  *p0     
//                          \|/
//                          v0
//                 
//
//                   So, there are:
//                       1 tetrahedron {v0,p0,p2,p1)
//                       and 1 wedge:  {p0,p2,p1,v1,v3,v2}: tab3[0] 
//                   
//                   Note:(a)if e1 and e3 are intersected by plane,
//                            and the plane intersects 3 edges,
//                            the edges e5 could not be intersected,
//                            if so, e6 be intersected too and the 
//                            plane intersect 4 edges.
//                        (b) tetrahedron vertices: 
//                            Use the first three indices of tab3:
//                            {v0,p(0),p(2),p(1)}                            
//                            
//                    ------------------------------------------  
//        case 125:
//                   if (plane intersecting  edges {e1,e2,e5})
//         There are :
//                      1 tetrahedron: {v1,p0,p2,p1}
//                      1 wedge      : {p0,p2,p1,v0,v3,v2},
//                    ------------------------------------------  
//    case 236:      
//                   if (plane intersecting  edges {e2,e3,e6})
//         There are :
//                      1 tetrahedron: {v2,p0,p1,p2}
//                      1 wedge      : {p0,p1,p2,v1,v0,v3},
//                    ------------------------------------------    
//        case 456:
//                  if (plane intersecting  edges {e4,e5,e6})
//         There are :
//                      1 tetrahedron: {v3,p0,p1,p2}
//                      1 wedge      : {p0,p1,p2,v0,v1,v2},
//                    ------------------------------------------
//
//    (c) Intersecting  2 edges and 1 vertex: tab2[] 
//                      -> create: 1 tetrahedron + 1 pyramid 
//      -> 12 cases 
//                    ------------------------------------------    
//        case 12:        v3        indexes:{0,0,1,2,3},
//                        *                  0 1 2 3 4      
//                    e6/ | \ e5                    
//                     /  |p1\         tetrahedron: indices(*,4,1,2)            
//                  v2/_ _|_*_\ v1     
//                    \   |   /                     
//                   e3\  |p0*                     
//                      \ | /e1                       
//                        v0                        
//
//                  if (plane intersecting  edges {e1,e2}
//                      and one vertex)
//           There are
//                      1 tetrahedron: {v1,v3,p0,p1}
//                      1 pyramid    : {v0,p0,p1,v2,v3},
//
//                  Note:  if e1 and e2 are intersected by plane,
//                            and the plane intersects 2 edges,
//                            then the vertex is v3.
//                    ------------------------------------------  
//      
//        case 13:                   indexes{2,1,0,1,3}
//                      Intersecting edges {e1,e3}
//                      Intersectinf vertes v3,
//           
//                      1 tetrahedron: {v0,v3,p1,p0}
//                      1 pyramid    : {v2,p1,p0,v1,v3},
//                    ------------------------------------------  
//        all cases see tab2[]
//                    ------------------------------------------
//    (d) Intersecting  1 edges and 2 vertices: tab1[] 
//                      -> create: 2 tetrahedra
//                      -> 6 cases
//
//        - edge e1, vertices {v2,v3}(e6)
//  
//                        v3        
//                        *              tetrahedra: {p0,v2,v3,v1},{p0,v3,v2,v0}
//                    e6/ | \ e5                     
//                     /  |  \                  .
//                 v2 *_ _|_ _\ v1     
//                    \   |   /                     
//                   e3\  |p0*                     
//                      \ | /e1                       
//                        v0                   
//         - other cases see tab1[]
//----------------------------------------------------------------------------  
//
void vtkBoxClipDataSet::ClipBox(vtkPoints *newPoints,
          vtkGenericCell *cell,
          vtkPointLocator *locator, 
          vtkCellArray *tets,
          vtkPointData *inPD, 
          vtkPointData *outPD,
          vtkCellData *inCD,
          vtkIdType cellId,
          vtkCellData *outCD)
{  
  vtkIdType     cellType   = cell->GetCellType();
  vtkIdList    *cellIds    = cell->GetPointIds();
  vtkCellArray *arraytetra = vtkCellArray::New();
  vtkPoints    *cellPts    = cell->GetPoints();
  vtkIdType     npts       = cellPts->GetNumberOfPoints(); 
  vtkIdType     cellptId[VTK_CELL_SIZE];
  vtkIdType     iid[4];
  vtkIdType    *v_id;
  vtkIdType    *verts, v1, v2;
  vtkIdType     ptId;
  vtkIdType     tab_id[6];
  vtkIdType     ptstetra = 4;

  vtkIdType i,j;
  unsigned allInside;
  unsigned ind[3];

  vtkIdType edges[6][2] = { {0,1}, {1,2}, {2,0},
                            {0,3}, {1,3}, {2,3} };  /* Edges Tetrahedron */
  double value,deltaScalar;
  double t;
  double *p1, *p2;
  double x[3], v[3];
  double v_tetra[4][3];

  for (i=0; i<npts; i++)
    {
    cellptId[i] = cellIds->GetId(i);
    }

  // Convert all volume cells to tetrahedra       
  TetraGrid(cellType,npts,cellptId,arraytetra);   
  unsigned totalnewtetra = arraytetra->GetNumberOfCells();

  for (unsigned idtetranew = 0 ; idtetranew < totalnewtetra; idtetranew++) 
    {
    arraytetra->GetNextCell(ptstetra,v_id);

    // Test Outside: see(1)
    unsigned test[6] = {1,1,1,1,1,1};
    for (i=0; i<4; i++)
      {       
      cellPts->GetPoint(v_id[i],v);

      if (v[0] > BoundBoxClip[0][0])
        test[0] = 0;
      if (v[0] < BoundBoxClip[0][1])
        test[1] = 0;  
      if (v[1] > BoundBoxClip[1][0])
        test[2] = 0;
      if (v[1] < BoundBoxClip[1][1])
        test[3] = 0;
      if (v[2] > BoundBoxClip[2][0])
        test[4] = 0;
      if (v[2] < BoundBoxClip[2][1])
        test[5] = 0;
             
      }//for all points of the cell.

    if ((test[0] == 1)|| (test[1] == 1) ||
    (test[2] == 1)|| (test[3] == 1) ||
    (test[4] == 1)|| (test[5] == 1))
      {
        continue;                         // Tetrahedron is outside.
      }

    float  *pPtr = (float *)cellPts->GetVoidPointer(0);   
    for (allInside=1, i=0; i<4; i++)
      {
        ptId = cellIds->GetId(v_id[i]);
        cellPts->GetPoint(v_id[i],v);

        if (!(((v[0] >= BoundBoxClip[0][0])&&(v[0] <= BoundBoxClip[0][1]) &&
         (v[1] >= BoundBoxClip[1][0])&&(v[1] <= BoundBoxClip[1][1])&& 
         (v[2] >= BoundBoxClip[2][0])&&(v[2] <= BoundBoxClip[2][1]))))
          { 
          //outside,its type might change later (nearby intersection)
          allInside = 0;      
          }
    
        // Currently all points are injected because of the possibility 
        // of intersection point merging.
        if ( locator->InsertUniquePoint(v, iid[i]) )
          {
          outPD->CopyData(inPD,ptId, iid[i]);
          }
        
      }//for all points of the tetrahedron.

    if ( allInside )
      {   
        // Tetrahedron inside.
        vtkIdType newCellId = tets->InsertNextCell(4,iid);
        outCD->CopyData(inCD,cellId,newCellId);
        continue;
      }

    float  pc[3];
    float *pc1  , *pc2;
    double *pedg1,*pedg2;

      // tab4: tetrahedron intersection in 4 edges.see (2)

    unsigned tab4[6][6] = { {0,1,1,2,3,3},   // Tetrahedron Intersection Cases
              {2,0,0,3,2,1},
              {3,3,2,0,2,1},
              {1,0,2,0,1,3},
              {0,0,1,2,3,3},
              {0,1,2,1,2,3}};
    unsigned tab3[4][6] = { {0,2,1,1,3,2},
              {0,2,1,0,3,2},
              {0,1,2,1,0,3},
              {0,1,2,0,1,2}};
    unsigned tab2[12][5] = { {0,0,1,2,3},
               {2,1,0,1,3},
               {1,0,1,0,3},
               {1,0,1,2,0},
               {3,1,0,1,0},
               {2,0,1,3,0},
               {3,1,0,2,1},
               {2,1,0,0,1},
               {0,0,1,3,1},
               {1,0,1,3,2},
               {3,1,0,0,2},
               {0,0,1,1,2}}; 
    unsigned tab1[12][3] = { {2,3,1},
               {3,2,0},
               {3,0,1},
               {0,3,2},
               {1,3,0},
               {3,1,2},
               {2,1,0},
               {1,2,3},
               {0,2,3},
               {2,0,1},
               {0,1,3},
               {1,0,2}}; 
      
    vtkCellArray *cellarray = vtkCellArray::New();
    vtkIdType newCellId = cellarray->InsertNextCell(4,iid);

            // Test Cell intersection with each plane of box
    for (unsigned planes = 0; planes < 6; planes++) 
      {
      switch(planes)
        {
        case 0:
        case 1:
          ind[0] = 0;
          ind[1] = 1;
          ind[2] = 2;
          break;
        case 2:
        case 3:
          ind[0] = 1;
          ind[1] = 0;
          ind[2] = 2;
          break;
        case 4:
        case 5:
          ind[0] = 2;
          ind[1] = 0;
          ind[2] = 1;
          break;
        }
        
      value = BoundBoxClip[ind[0]][planes%2];   // The plane is always parallel to unitary cube. 
        
      unsigned totalnewcells = cellarray->GetNumberOfCells();
      vtkCellArray *newcellArray = vtkCellArray::New();
        
      for (unsigned idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
        {
        unsigned num_inter = 0;
        unsigned edges_inter = 0;
        unsigned i0,i1;
        double p_inter[4][3];   
        vtkIdType p_id[4];
        cellarray->GetNextCell(npts,v_id);
        
        newPoints->GetPoint(v_id[0],v_tetra[0]); //coord (x,y,z) 
        newPoints->GetPoint(v_id[1],v_tetra[1]); 
        newPoints->GetPoint(v_id[2],v_tetra[2]); 
        newPoints->GetPoint(v_id[3],v_tetra[3]); 

        for (int edgeNum=0; edgeNum < 6; edgeNum++)
          {
          verts = edges[edgeNum]; 
          
          p1 = v_tetra[verts[0]];
          p2 = v_tetra[verts[1]];
  
          if ( (p1[ind[0]] <= value && value <= p2[ind[0]]) || 
               (p2[ind[0]] <= value && value <= p1[ind[0]]) )
            {
            deltaScalar = p2[ind[0]] - p1[ind[0]];
      
            if (deltaScalar > 0)
              {
              pedg1 = p1;   pedg2 = p2;
              v1 = verts[0]; v2 = verts[1];
              }
            else
              {
              pedg1 = p2;   pedg2 = p1;
              v1 = verts[1]; v2 = verts[0];
              deltaScalar = -deltaScalar;
              }
      
            // linear interpolation
            t = ( deltaScalar == 0.0 ? 0.0 :
            (value - pedg1[ind[0]]) / deltaScalar );
      
            if ( t == 0.0 )
              {
              continue;
              }
            else if ( t == 1.0 )
              {
              continue;
              }
      
            pc1 = pPtr + 3*v1;
            pc2 = pPtr + 3*v2;
      
            for (j=0; j<3; j++)
              {
              x[j]  = pedg1[j]  + t*(pedg2[j] - pedg1[j]);
              pc[j] = pc1[j] + t*(pc2[j] - pc1[j]);
              p_inter[num_inter][j] = x[j];
              }
      
            // Incorporate point into output and interpolate edge data as necessary
            edges_inter = edges_inter * 10 + (edgeNum+1);
            if ( locator->InsertUniquePoint(x, p_id[num_inter]) )
              {
              outPD->InterpolateEdge(inPD, p_id[num_inter], cellIds->GetId(v1),
                   cellIds->GetId(v2), t);
              }
      
            num_inter++;        
            }//if edge intersects value
          }//for all edges

        if (num_inter == 0) 
          {           
          unsigned outside = 0;
          for(i=0; i<4; i++)
            {
            if (((v_tetra[i][ind[0]] < value) && ((planes % 2) == 0)) || 
                ((v_tetra[i][ind[0]] > value) && ((planes % 2) == 1))) 

              // If only one vertex is ouside, so the tetrahedron is outside 
              // because there is not intersection.
              {      
              outside = 1;
              break;
              }
            }
          if(outside == 0)
            {
             // else it is possible intersection if other plane
                 newCellId = newcellArray->InsertNextCell(4,v_id);       
            }
           continue;
          }
        switch(num_inter) 
          {
          case 4:                 // We have two wedges
            switch(edges_inter) 
              {
              case 1246:
                i0 = 0;
                break;
              case 2345:
                i0 = 2;
                break;
              case 1356:
                i0 = 4;
                break;
              default:
                printf(
                 "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                  num_inter,edges_inter);
              continue;
              }                                                         

            if (((v_tetra[3][ind[0]] < value) && ((planes % 2) == 0)) || 
                ((v_tetra[3][ind[0]] > value) && ((planes % 2) == 1))) 
              { 

              // The v_tetra[3] is outside box, so the first wedge is outside

              tab_id[0] = p_id[tab4[i0+1][0]];                            
              // ps: v_tetra[3] is always in first wedge (see tab)

              tab_id[1] = v_id[tab4[i0+1][1]];
              tab_id[2] = p_id[tab4[i0+1][2]];
              tab_id[3] = p_id[tab4[i0+1][3]];         
              tab_id[4] = v_id[tab4[i0+1][4]];
              tab_id[5] = p_id[tab4[i0+1][5]];
              CreateTetra(6,tab_id,newcellArray);
              }
            else 
              {
              tab_id[0] = p_id[tab4[i0][0]];
              tab_id[1] = v_id[tab4[i0][1]];
              tab_id[2] = p_id[tab4[i0][2]];
              tab_id[3] = p_id[tab4[i0][3]];
              tab_id[4] = v_id[tab4[i0][4]];
              tab_id[5] = p_id[tab4[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
              }
            break;
          case 3:                   // We have one tetrahedron and one wedge
            switch(edges_inter) 
              {
              case 134:
                i0 = 0;
                break;
              case 125:
                i0 = 1;
                break;
              case 236:
                i0 = 2;
                break;
              case 456:
                i0 = 3;
                break;
              default:
                printf(
                "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                num_inter,edges_inter);
                continue;
              }                                                              

            if (((v_tetra[i0][ind[0]] < value) && ((planes % 2) == 0)) ||   
                ((v_tetra[i0][ind[0]] > value) && ((planes % 2) == 1))) 
              {

              // Isolate vertex is outside box, so the tetrahedron is outside
              tab_id[0] = p_id[tab3[i0][0]];
              tab_id[1] = p_id[tab3[i0][1]];
              tab_id[2] = p_id[tab3[i0][2]];
              tab_id[3] = v_id[tab3[i0][3]];
              tab_id[4] = v_id[tab3[i0][4]];
              tab_id[5] = v_id[tab3[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
              }
            else 
              {
              tab_id[0] = v_id[i0];
              tab_id[1] = p_id[tab3[i0][0]];
              tab_id[2] = p_id[tab3[i0][1]];
              tab_id[3] = p_id[tab3[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);                
              }
            break;

          case 2:                    // We have one tetrahedron and one pyramid
            switch(edges_inter)      // i1 = vertex of the tetrahedron
              {
              case 12:
                i0 = 0; i1 = 1;  
                break;
              case 13:
                i0 = 1;  i1 = 0;
                break;
              case 23:
                i0 = 2;  i1 = 2; 
                break;
              case 25:
                i0 = 3;  i1 = 1;
                break;
              case 26:
                i0 = 4;  i1 = 2;
                break;
              case 56:
                i0 = 5;  i1 = 3;
                break;
              case 34:
                i0 = 6;  i1 = 0;
                break;
              case 46:
                i0 = 7;  i1 = 3;
                break;
              case 36:
                i0 = 8;  i1 = 2; 
                break;
              case 14:
                i0 = 9;  i1 = 0; 
                break;
              case 15:
                i0 = 10; i1 = 1;
                break;
              case 45:
                i0 = 11; i1 = 3;
                break;
              default:
                printf(
                    "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                  num_inter,edges_inter);
                continue;
              }                                                          

            if (((v_tetra[i1][ind[0]] < value) && ((planes % 2) == 0)) ||
                ((v_tetra[i1][ind[0]] > value) && ((planes % 2) == 1)))

              // Isolate vertex is outside box, so the tetrahedron is outside
              {
              tab_id[0] = v_id[tab2[i0][0]];
              tab_id[1] = p_id[tab2[i0][1]];
              tab_id[2] = p_id[tab2[i0][2]];
              tab_id[3] = v_id[tab2[i0][3]];
              tab_id[4] = v_id[tab2[i0][4]];
              CreateTetra(5,tab_id,newcellArray);
              }
            else 
              {
              tab_id[0] = v_id[i1];
              tab_id[1] = v_id[tab2[i0][4]];
              tab_id[2] = p_id[tab2[i0][1]];
              tab_id[3] = p_id[tab2[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);    
              }
            break;

          case 1:              // We have two tetrahedron.
            if ((edges_inter > 6) || (edges_inter < 1)) 
              {
              printf(
                "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                num_inter,edges_inter);
              continue;
              }                                                         
            if (((v_tetra[tab1[2*edges_inter-1][2]][ind[0]] < value) && 
                ((planes % 2) == 0)) ||   
                ((v_tetra[tab1[2*edges_inter-1][2]][ind[0]] > value) && 
                ((planes % 2) == 1))) 

              // Isolate vertex is outside box, so the tetrahedron is outside
              {   
              tab_id[0] = p_id[0];
              tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
              }
            else 
              {
              tab_id[0] = p_id[0];
              tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
              }
            break;
          }
        } // for all new cells

      cellarray->Delete();
      cellarray = newcellArray;
      } //for all planes

      unsigned totalnewcells = cellarray->GetNumberOfCells();

      for (unsigned idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
        {
        cellarray->GetNextCell(npts,v_id);
        newCellId = tets->InsertNextCell(npts,v_id);
        outCD->CopyData(inCD,cellId,newCellId);
        }
      cellarray->Delete();
    }
  arraytetra->Delete();
}

//----------------------------------------------------------------------------  
// ClipHexahedron: Box is like hexahedron.
// 
// The difference between ClipBox and ClipHexahedron is the outside test.
// The ClipHexahedron use plane equation to decide who is outside.
//
void vtkBoxClipDataSet::ClipHexahedron(vtkPoints *newPoints,
               vtkGenericCell *cell,
               vtkPointLocator *locator, 
               vtkCellArray *tets,
               vtkPointData *inPD, 
               vtkPointData *outPD,
               vtkCellData *inCD,
               vtkIdType cellId,
               vtkCellData *outCD) 
{  
  vtkIdType idcellnew;
  vtkIdType     cellType   = cell->GetCellType();
  vtkIdList    *cellIds    = cell->GetPointIds();
  vtkCellArray *arraytetra = vtkCellArray::New();
  vtkPoints    *cellPts    = cell->GetPoints();
  vtkIdType     npts       = cellPts->GetNumberOfPoints(); 
  vtkIdType     cellptId[VTK_CELL_SIZE];
  vtkIdType     iid[4];
  vtkIdType    *v_id;
  vtkIdType    *verts, v1, v2;
  vtkIdType     ptId;
  vtkIdType     tab_id[6];
  vtkIdType     ptstetra = 4;

  vtkIdType i,j;
  unsigned allInside;
  unsigned ind[3];

  vtkIdType edges[6][2] = { {0,1}, {1,2}, {2,0}, 
                            {0,3}, {1,3}, {2,3} };  /* Edges Tetrahedron */
  double deltaScalar;
  double t;
  double *p1, *p2;
  double v[3], x[3];
  double p[6];
  double v_tetra[4][3];

  for (i=0; i<npts; i++)
    {
    cellptId[i] = cellIds->GetId(i);
    }
   
  TetraGrid(cellType,npts,cellptId,arraytetra);  // Convert all volume cells to tetrahedra

  unsigned totalnewtetra = arraytetra->GetNumberOfCells();

  for (unsigned idtetranew = 0 ; idtetranew < totalnewtetra; idtetranew++) 
    {
      arraytetra->GetNextCell(ptstetra,v_id);

      // Test Outside
      unsigned test[6] = {1,1,1,1,1,1};
      for (i=0; i<4; i++)
        {
        int k;
        cellPts->GetPoint(v_id[i],v);

        // Use plane equation 
        for(k=0;k<6;k++)
          {
          p[k] = n_pl[k][0]*
                (v[0] - o_pl[k][0])+ n_pl[k][1]*(v[1] - o_pl[k][1]) +  
                n_pl[k][2]*(v[2] - o_pl[k][2]);
          }

       
        for(k=0;k<3;k++)
          {
          if (p[2*k] < 0) test[2*k] = 0;
          if (p[2*k+1] < 0) test[2*k+1] = 0;
          }
             
        }//for all points of the cell.

      if ((test[0] == 1)|| (test[1] == 1) ||
          (test[2] == 1)|| (test[3] == 1) ||
          (test[4] == 1)|| (test[5] == 1))
        {
        continue;                         // Tetrahedron is outside.
        }
     
      float  *pPtr = (float *)cellPts->GetVoidPointer(0);   
      for (allInside=1, i=0; i<4; i++)
        {
        ptId = cellIds->GetId(v_id[i]);
        cellPts->GetPoint(v_id[i],v);
          
        for(unsigned k=0;k<6;k++)
            p[k] = n_pl[k][0]*(v[0] - o_pl[k][0])+ 
                   n_pl[k][1]*(v[1] - o_pl[k][1]) +  
                   n_pl[k][2]*(v[2] - o_pl[k][2]);

        if (!((p[0] <= 0) && (p[1] <= 0) &&
             (p[2] <= 0) && (p[3] <= 0) &&
             (p[4] <= 0) && (p[5] <= 0)))
          {
          allInside = 0;
          }
         
        // Currently all points are injected because of the possibility 
        // of intersection point merging.
        if ( locator->InsertUniquePoint(v, iid[i]) )
          {
          outPD->CopyData(inPD,ptId, iid[i]);
          }
        }//for all points of the tetrahedron.

      if ( allInside )
        {  
        vtkIdType newCellId = tets->InsertNextCell(4,iid);     // Tetrahedron inside.
        outCD->CopyData(inCD,cellId,newCellId);
        continue;
        }

      float  pc[3];
      float *pc1  , *pc2;
      double *pedg1,*pedg2;

      unsigned tab4[6][6] = { {0,1,1,2,3,3},   // Tetrahedron Intersection Cases
              {2,0,0,3,2,1},
              {3,3,2,0,2,1},
              {1,0,2,0,1,3},
              {0,0,1,2,3,3},
              {0,1,2,1,2,3}};
      unsigned tab3[4][6] = { {0,2,1,1,3,2},
              {0,2,1,0,3,2},
              {0,1,2,1,0,3},
              {0,1,2,0,1,2}};
      unsigned tab2[12][5] = { {0,0,1,2,3},
               {2,1,0,1,3},
               {1,0,1,0,3},
               {1,0,1,2,0},
               {3,1,0,1,0},
               {2,0,1,3,0},
               {3,1,0,2,1},
               {2,1,0,0,1},
               {0,0,1,3,1},
               {1,0,1,3,2},
               {3,1,0,0,2},
               {0,0,1,1,2}}; 
      unsigned tab1[12][3] = { {2,3,1},
               {3,2,0},
               {3,0,1},
               {0,3,2},
               {1,3,0},
               {3,1,2},
               {2,1,0},
               {1,2,3},
               {0,2,3},
               {2,0,1},
               {0,1,3},
               {1,0,2}}; 
      
      vtkCellArray *cellarray = vtkCellArray::New();
      vtkIdType newCellId = cellarray->InsertNextCell(4,iid);

      // Test Cell intersection with each plane of box
      for (unsigned planes = 0; planes < 6; planes++) 
        {
        switch(planes) 
          {
          case 0:
          case 1:
            ind[0] = 0;
            ind[1] = 1;
            ind[2] = 2;
            break;
          case 2:
          case 3:
            ind[0] = 1;
            ind[1] = 0;
            ind[2] = 2;
            break;
          case 4:
          case 5:
            ind[0] = 2;
            ind[1] = 0;
            ind[2] = 1;
            break;
          }
        
        vtkIdType totalnewcells = cellarray->GetNumberOfCells();
        vtkCellArray *newcellArray = vtkCellArray::New();
        
        for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
          {
          unsigned i0,i1;
          double p_inter[4][3];   
          unsigned num_inter = 0;
          unsigned edges_inter = 0;
          vtkIdType p_id[4];

          cellarray->GetNextCell(npts,v_id);
          
          newPoints->GetPoint(v_id[0],v_tetra[0]); //coord (x,y,z) 
          newPoints->GetPoint(v_id[1],v_tetra[1]); 
          newPoints->GetPoint(v_id[2],v_tetra[2]); 
          newPoints->GetPoint(v_id[3],v_tetra[3]); 

          p[0] = n_pl[planes][0]*(v_tetra[0][0] - o_pl[planes][0]) +
                 n_pl[planes][1]*(v_tetra[0][1] - o_pl[planes][1]) +  
                 n_pl[planes][2]*(v_tetra[0][2] - o_pl[planes][2]);  
          p[1] = n_pl[planes][0]*(v_tetra[1][0] - o_pl[planes][0]) + 
                 n_pl[planes][1]*(v_tetra[1][1] - o_pl[planes][1]) +  
                 n_pl[planes][2]*(v_tetra[1][2] - o_pl[planes][2]);
          p[2] = n_pl[planes][0]*(v_tetra[2][0] - o_pl[planes][0]) + 
                 n_pl[planes][1]*(v_tetra[2][1] - o_pl[planes][1]) +  
                 n_pl[planes][2]*(v_tetra[2][2] - o_pl[planes][2]);
          p[3] = n_pl[planes][0]*(v_tetra[3][0] - o_pl[planes][0]) + 
                 n_pl[planes][1]*(v_tetra[3][1] - o_pl[planes][1]) +  
                 n_pl[planes][2]*(v_tetra[3][2] - o_pl[planes][2]);
        
          for (int edgeNum=0; edgeNum < 6; edgeNum++)
            {
            verts = edges[edgeNum]; 
        
            p1 = v_tetra[verts[0]];
            p2 = v_tetra[verts[1]];
            double s1 = p[verts[0]];
            double s2 = p[verts[1]];    
            if ( (s1 * s2) <=0)
              {
              deltaScalar = s2 - s1;
            
              if (deltaScalar > 0)
                {
                pedg1 = p1;   pedg2 = p2;
                v1 = verts[0]; v2 = verts[1];
                }
              else
                {
                pedg1 = p2;   pedg2 = p1;
                v1 = verts[1]; v2 = verts[0];
                deltaScalar = -deltaScalar;
                t = s1; s1 = s2; s2 = t;
                }
        
              // linear interpolation
              t = ( deltaScalar == 0.0 ? 0.0 : ( - s1) / deltaScalar );
        
              if ( t == 0.0 )
                {
                continue;
                }
              else if ( t == 1.0 )
                {
                continue;
                }
        
              pc1 = pPtr + 3*v1;
              pc2 = pPtr + 3*v2;
        
              for (j=0; j<3; j++)
                {
                x[j]  = pedg1[j]  + t*(pedg2[j] - pedg1[j]);
                pc[j] = pc1[j] + t*(pc2[j] - pc1[j]);
                p_inter[num_inter][j] = x[j];
                }
        
              // Incorporate point into output and interpolate edge data as necessary
              edges_inter = edges_inter * 10 + (edgeNum+1);

              if ( locator->InsertUniquePoint(x, p_id[num_inter]) )
                {
                outPD->InterpolateEdge(inPD, p_id[num_inter], cellIds->GetId(v1),
                     cellIds->GetId(v2), t);
                }
        
              num_inter++;        
              }//if edge intersects value
            }//for all edges
          if (num_inter == 0) 
            {  
            unsigned outside = 0;
            for(i=0;i<4;i++)
              {
              if (p[i] > 0)
                {
                // If only one vertex is ouside, so the tetrahedron is outside 
                // because there is not intersection.
                // some vertex could be on plane, so you need to test all vertex

                outside = 1;  
                break; 
                }
              }
            if (outside == 0)
              {
              // else it is possible intersection if other plane

              newCellId = newcellArray->InsertNextCell(4,v_id);
              }
            continue;
            }
          switch(num_inter) 
            {
            case 4:                 // We have two wedges
              switch(edges_inter) 
                {
                case 1246:
                  i0 = 0;
                  break;
                case 2345:
                  i0 = 2;
                  break;
                case 1356:
                  i0 = 4;
                  break;
                default:
                  printf(
                    "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                    num_inter,edges_inter);
                  continue;
                }                                                    

              if (((p[3] > 0) && ((planes % 2) == 0)) ||
                  ((p[3] > 0) && ((planes % 2) == 1))) 
                {
                // The v_tetra[3] is outside box, so the first wedge is outside
                // ps: v_tetra[3] is always in first wedge (see tab)

                tab_id[0] = p_id[tab4[i0+1][0]];
                tab_id[1] = v_id[tab4[i0+1][1]];
                tab_id[2] = p_id[tab4[i0+1][2]];
                tab_id[3] = p_id[tab4[i0+1][3]];          
                tab_id[4] = v_id[tab4[i0+1][4]];
                tab_id[5] = p_id[tab4[i0+1][5]];
                CreateTetra(6,tab_id,newcellArray);
                }
              else 
                {
                tab_id[0] = p_id[tab4[i0][0]];
                tab_id[1] = v_id[tab4[i0][1]];
                tab_id[2] = p_id[tab4[i0][2]];
                tab_id[3] = p_id[tab4[i0][3]];
                tab_id[4] = v_id[tab4[i0][4]];
                tab_id[5] = p_id[tab4[i0][5]];
                CreateTetra(6,tab_id,newcellArray);
                }
              break;
            case 3:                   // We have one tetrahedron and one wedge
              switch(edges_inter) 
                {
                case 134:
                  i0 = 0;
                  break;
                case 125:
                  i0 = 1;
                  break;
                case 236:
                  i0 = 2;
                  break;
                case 456:
                  i0 = 3;
                  break;
                default:
                  printf(
                    "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                    num_inter,edges_inter);
                  continue;
                }                                                     

                if (((p[i0] > 0) && ((planes % 2) == 0)) ||   
                    ((p[i0] > 0) && ((planes % 2) == 1))) 
                  {
                  // Isolate vertex is outside box, so the tetrahedron is outside

                  tab_id[0] = p_id[tab3[i0][0]];
                  tab_id[1] = p_id[tab3[i0][1]];
                  tab_id[2] = p_id[tab3[i0][2]];
                  tab_id[3] = v_id[tab3[i0][3]];
                  tab_id[4] = v_id[tab3[i0][4]];
                  tab_id[5] = v_id[tab3[i0][5]];
                  CreateTetra(6,tab_id,newcellArray);
                  }
                else 
                  {
                  tab_id[0] = v_id[i0];
                  tab_id[1] = p_id[tab3[i0][0]];
                  tab_id[2] = p_id[tab3[i0][1]];
                  tab_id[3] = p_id[tab3[i0][2]];
                  newCellId = newcellArray->InsertNextCell(4,tab_id);                
                  }
                break;
            case 2:                    // We have one tetrahedron and one pyramid
              switch(edges_inter)      // i1 = vertex of the tetrahedron
                {
                case 12:
                  i0 = 0; i1 = 1;  
                  break;
                case 13:
                  i0 = 1;  i1 = 0;
                  break;
                case 23:
                  i0 = 2;  i1 = 2; 
                  break;
                case 25:
                  i0 = 3;  i1 = 1;
                  break;
                case 26:
                  i0 = 4;  i1 = 2;
                  break;
                case 56:
                  i0 = 5;  i1 = 3;
                  break;
                case 34:
                  i0 = 6;  i1 = 0;
                  break;
                case 46:
                  i0 = 7;  i1 = 3;
                  break;
                case 36:
                  i0 = 8;  i1 = 2; 
                  break;
                case 14:
                  i0 = 9;  i1 = 0; 
                  break;
                case 15:
                  i0 = 10; i1 = 1;
                  break;
                case 45:
                  i0 = 11; i1 = 3;
                  break;
                default:
                  printf(
                    "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                    num_inter,edges_inter);
                  continue;
                } 
              if (((p[i1] > 0) && ((planes % 2) == 0)) ||
                  ((p[i1] > 0) && ((planes % 2) == 1)))
                {
                // Isolate vertex is outside box, so the tetrahedron is outside

                tab_id[0] = v_id[tab2[i0][0]];
                tab_id[1] = p_id[tab2[i0][1]];
                tab_id[2] = p_id[tab2[i0][2]];
                tab_id[3] = v_id[tab2[i0][3]];
                tab_id[4] = v_id[tab2[i0][4]];
                CreateTetra(5,tab_id,newcellArray);
                }
              else 
                {
                tab_id[0] = v_id[i1];
                tab_id[1] = v_id[tab2[i0][4]];
                tab_id[2] = p_id[tab2[i0][1]];
                tab_id[3] = p_id[tab2[i0][2]];
                newCellId = newcellArray->InsertNextCell(4,tab_id);    
                }
              break;
            case 1:              // We have two tetrahedron.
              if ((edges_inter > 6) || (edges_inter < 1)) 
                {
                printf(
                  "Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",
                  num_inter,edges_inter);
                continue;
                }                                        
              if (((p[tab1[2*edges_inter-1][2]] > 0) && ((planes % 2) == 0)) ||
                  ((p[tab1[2*edges_inter-1][2]] > 0) && ((planes % 2) == 1)))
                {
                // Isolate vertex is outside box, so the tetrahedron is outside
                tab_id[0] = p_id[0];
                tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
                tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
                tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
                newCellId = newcellArray->InsertNextCell(4,tab_id);
                }
              else 
                {
                tab_id[0] = p_id[0];
                tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
                tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
                tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
                newCellId = newcellArray->InsertNextCell(4,tab_id);
                }
              break;
            }
          } // for all new cells
        cellarray->Delete();
        cellarray = newcellArray;
        } //for all planes

      vtkIdType totalnewcells = cellarray->GetNumberOfCells();

      for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
        {
        cellarray->GetNextCell(npts,v_id);
        newCellId = tets->InsertNextCell(npts,v_id);
        outCD->CopyData(inCD,cellId,newCellId);
        }
      cellarray->Delete();
    }           
  arraytetra->Delete();
}               
//----------------------------------------------------------------------------  
// ClipBoxInOut
// 
// The difference between ClipBox and ClipBoxInOut is the outputs.
// The ClipBoxInOut generate both outputs: inside and outside the clip box.
// 
void vtkBoxClipDataSet::ClipBoxInOut(vtkPoints *newPoints,
             vtkGenericCell *cell,
                  vtkPointLocator *locator, 
             vtkCellArray **tets,
             vtkPointData *inPD, 
             vtkPointData *outPD,
             vtkCellData *inCD,
             vtkIdType cellId,
             vtkCellData **outCD)
{  
  vtkIdType     cellType   = cell->GetCellType();
  vtkIdList    *cellIds    = cell->GetPointIds();
  vtkCellArray *arraytetra = vtkCellArray::New();
  vtkPoints    *cellPts    = cell->GetPoints();
  vtkIdType     npts       = cellPts->GetNumberOfPoints(); 
  vtkIdType     cellptId[VTK_CELL_SIZE];
  vtkIdType     iid[4];
  vtkIdType    *v_id;
  vtkIdType    *verts, v1, v2;
  vtkIdType     ptId;
  vtkIdType     ptIdout[4];
  vtkIdType     tab_id[6];
  vtkIdType     ptstetra = 4;

  int i,j;
  int allInside;
  int ind[3];

  vtkIdType edges[6][2] = { {0,1}, {1,2}, {2,0}, 
                      {0,3}, {1,3}, {2,3} };  /* Edges Tetrahedron */
  double value,deltaScalar;
  double t;
  double v[3], x[3];
  double v_tetra[4][3];
  double *p1, *p2;

  for (i=0; i<npts; i++)
    cellptId[i] = cellIds->GetId(i);

  // Convert all volume cells to tetrahedra       
  TetraGrid(cellType,npts,cellptId,arraytetra);   
  unsigned totalnewtetra = arraytetra->GetNumberOfCells();

  for (unsigned idtetranew = 0 ; idtetranew < totalnewtetra; idtetranew++) 
  {
    arraytetra->GetNextCell(ptstetra,v_id);

    float  *pPtr = (float *)cellPts->GetVoidPointer(0);   

    // Test Outside: see(1)
    unsigned test[6] = {1,1,1,1,1,1};
    for (i=0; i<4; i++)
      {       
      ptIdout[i] = cellIds->GetId(v_id[i]);
      //ptId = cellIds->GetId(v_id[i]);
      cellPts->GetPoint(v_id[i],v_tetra[i]);

      if (v_tetra[i][0] > BoundBoxClip[0][0])
        test[0] = 0;
      if (v_tetra[i][0] < BoundBoxClip[0][1])
        test[1] = 0;  
      if (v_tetra[i][1] > BoundBoxClip[1][0])
        test[2] = 0;
      if (v_tetra[i][1] < BoundBoxClip[1][1])
        test[3] = 0;
      if (v_tetra[i][2] > BoundBoxClip[2][0])
        test[4] = 0;
      if (v_tetra[i][2] < BoundBoxClip[2][1])
        test[5] = 0;
           
       //    if ( locator->InsertUniquePoint(v_tetra[i], iid[i]) ) {
       //        outPD->CopyData(inPD,ptId, iid[i]);
       //        }
      }//for all points of the cell.

    if ((test[0] == 1)|| (test[1] == 1) ||
        (test[2] == 1)|| (test[3] == 1) ||
        (test[4] == 1)|| (test[5] == 1)) 
      {
      for (i=0; i<4; i++)
        if ( locator->InsertUniquePoint(v_tetra[i], iid[i]) ) 
          outPD->CopyData(inPD,ptIdout[i], iid[i]);
      int newCellId = tets[1]->InsertNextCell(4,iid);
      outCD[1]->CopyData(inCD,cellId,newCellId);
      continue;                         // Tetrahedron is outside.
      }

    for (allInside=1, i=0; i<4; i++)
      {
      ptId = cellIds->GetId(v_id[i]);
      cellPts->GetPoint(v_id[i],v);

      if (!(((v[0] >= BoundBoxClip[0][0])&&(v[0] <= BoundBoxClip[0][1]) &&
             (v[1] >= BoundBoxClip[1][0])&&(v[1] <= BoundBoxClip[1][1])&& 
             (v[2] >= BoundBoxClip[2][0])&&(v[2] <= BoundBoxClip[2][1]))))
        { 
        //outside,its type might change later (nearby intersection)
        allInside = 0;      
        }
  
      // Currently all points are injected because of the possibility 
      // of intersection point merging.
      if ( locator->InsertUniquePoint(v, iid[i]) )
        {
        outPD->CopyData(inPD,ptId, iid[i]);
        }
      
      }//for all points of the tetrahedron.

    if ( allInside )
      {   
      // Tetrahedron inside.
      int newCellId = tets[0]->InsertNextCell(4,iid);
      outCD[0]->CopyData(inCD,cellId,newCellId);
      continue;
      }

    float  pc[3];
    float *pc1  , *pc2;
    double *pedg1,*pedg2;

    // tab4: tetrahedron intersection in 4 edges.see (2)

    unsigned tab4[6][6] = { {0,1,1,2,3,3},   // Tetrahedron Intersection Cases
          {2,0,0,3,2,1},
          {3,3,2,0,2,1},
          {1,0,2,0,1,3},
          {0,0,1,2,3,3},
          {0,1,2,1,2,3}};
    unsigned tab3[4][6] = { {0,2,1,1,3,2},
          {0,2,1,0,3,2},
          {0,1,2,1,0,3},
          {0,1,2,0,1,2}};
    unsigned tab2[12][5] = { {0,0,1,2,3},
           {2,1,0,1,3},
           {1,0,1,0,3},
           {1,0,1,2,0},
           {3,1,0,1,0},
           {2,0,1,3,0},
           {3,1,0,2,1},
           {2,1,0,0,1},
           {0,0,1,3,1},
           {1,0,1,3,2},
           {3,1,0,0,2},
           {0,0,1,1,2}}; 
    unsigned tab1[12][3] = { {2,3,1},
           {3,2,0},
           {3,0,1},
           {0,3,2},
           {1,3,0},
           {3,1,2},
           {2,1,0},
           {1,2,3},
           {0,2,3},
           {2,0,1},
           {0,1,3},
           {1,0,2}}; 
    
    vtkCellArray *cellarray    = vtkCellArray::New();
    vtkCellArray *cellarrayout = vtkCellArray::New();
    int newCellId = cellarray->InsertNextCell(4,iid);

    // Test Cell intersection with each plane of box
    for (unsigned planes = 0; planes < 6; planes++) 
      {
      switch(planes) 
        {
        case 0:
        case 1:
          ind[0] = 0;
          ind[1] = 1;
          ind[2] = 2;
          break;
        case 2:
        case 3:
          ind[0] = 1;
          ind[1] = 0;
          ind[2] = 2;
          break;
        case 4:
        case 5:
          ind[0] = 2;
          ind[1] = 0;
          ind[2] = 1;
        break;
        }
      
      value = BoundBoxClip[ind[0]][planes%2];   // The plane is always parallel to unitary cube. 
      
      unsigned totalnewcells = cellarray->GetNumberOfCells();
      vtkCellArray *newcellArray = vtkCellArray::New();
      
      for (unsigned idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
        {
        unsigned num_inter = 0;
        unsigned edges_inter = 0;
        unsigned i0,i1;
        double p_inter[4][3];   
        vtkIdType p_id[4];
        cellarray->GetNextCell(npts,v_id);
        
        newPoints->GetPoint(v_id[0],v_tetra[0]); //coord (x,y,z) 
        newPoints->GetPoint(v_id[1],v_tetra[1]); 
        newPoints->GetPoint(v_id[2],v_tetra[2]); 
        newPoints->GetPoint(v_id[3],v_tetra[3]); 
        for (int edgeNum=0; edgeNum < 6; edgeNum++)
          {
          verts = edges[edgeNum]; 
          
          p1 = v_tetra[verts[0]];
          p2 = v_tetra[verts[1]];
  
          if ( (p1[ind[0]] <= value && value <= p2[ind[0]]) || 
               (p2[ind[0]] <= value && value <= p1[ind[0]]) )
            {
            deltaScalar = p2[ind[0]] - p1[ind[0]];
      
            if (deltaScalar > 0)
              {
              pedg1 = p1;   pedg2 = p2;
              v1 = verts[0]; v2 = verts[1];
              }
            else
              {
              pedg1 = p2;   pedg2 = p1;
              v1 = verts[1]; v2 = verts[0];
              deltaScalar = -deltaScalar;
              }
      
            // linear interpolation
            t = ( deltaScalar == 0.0 ? 0.0 :
            (value - pedg1[ind[0]]) / deltaScalar );
      
            if ( t == 0.0 )
              {
              continue;
              }
            else if ( t == 1.0 )
              {
              continue;
              }
      
            pc1 = pPtr + 3*v1;
            pc2 = pPtr + 3*v2;
      
            for (j=0; j<3; j++)
              {
              x[j]  = pedg1[j]  + t*(pedg2[j] - pedg1[j]);
              pc[j] = pc1[j] + t*(pc2[j] - pc1[j]);
              p_inter[num_inter][j] = x[j];
              }
      
            // Incorporate point into output and interpolate edge data as necessary
            edges_inter = edges_inter * 10 + (edgeNum+1);
            if ( locator->InsertUniquePoint(x, p_id[num_inter]) )
              {
              outPD->InterpolateEdge(inPD, p_id[num_inter], cellIds->GetId(v1),
                   cellIds->GetId(v2), t);
              }
      
            num_inter++;        
            }//if edge intersects value
          }//for all edges

        if (num_inter == 0) 
          {           
          unsigned outside = 0;
          for(i=0; i<4; i++)
            {
            if (((v_tetra[i][ind[0]] < value) && ((planes % 2) == 0)) || 
                // If only one vertex is ouside, so the tetrahedron is outside 
                ((v_tetra[i][ind[0]] > value) && ((planes % 2) == 1))) 
                // because there is not intersection.
              {      
              outside = 1;
              break;
              }
            }
          if(outside == 0) 
            {
            // else it is possible intersection if other plane
             newCellId = newcellArray->InsertNextCell(4,v_id);       
            }
          else 
            {
            newCellId = tets[1]->InsertNextCell(4,v_id);
            outCD[1]->CopyData(inCD,cellId,newCellId);
            }
          continue;
          }
        switch(num_inter) 
          {
          case 4:                 // We have two wedges
            switch(edges_inter) 
              {
              case 1246:
                i0 = 0;
                break;
              case 2345:
                i0 = 2;
                break;
              case 1356:
                i0 = 4;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              }                                                        
            if (((v_tetra[3][ind[0]] < value) && ((planes % 2) == 0)) || 
                // The v_tetra[3] is outside box, so
                ((v_tetra[3][ind[0]] > value) && ((planes % 2) == 1))) 
              {
              // the first wedge is outside
              // ps: v_tetra[3] is always in first wedge (see tab)
  
              tab_id[0] = p_id[tab4[i0+1][0]];   // Inside                           
              tab_id[1] = v_id[tab4[i0+1][1]];
              tab_id[2] = p_id[tab4[i0+1][2]];
              tab_id[3] = p_id[tab4[i0+1][3]];         
                                            tab_id[4] = v_id[tab4[i0+1][4]];
              tab_id[5] = p_id[tab4[i0+1][5]];
              CreateTetra(6,tab_id,newcellArray);
  
              tab_id[0] = p_id[tab4[i0][0]];    // Outside
              tab_id[1] = v_id[tab4[i0][1]];
              tab_id[2] = p_id[tab4[i0][2]];
              tab_id[3] = p_id[tab4[i0][3]];
              tab_id[4] = v_id[tab4[i0][4]];
              tab_id[5] = p_id[tab4[i0][5]];
              CreateTetra(6,tab_id,cellarrayout);
              }
            else 
              {
              tab_id[0] = p_id[tab4[i0][0]];   // Inside
              tab_id[1] = v_id[tab4[i0][1]];
              tab_id[2] = p_id[tab4[i0][2]];
              tab_id[3] = p_id[tab4[i0][3]];
              tab_id[4] = v_id[tab4[i0][4]];
              tab_id[5] = p_id[tab4[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
  
              tab_id[0] = p_id[tab4[i0+1][0]];  // Outside                          
              tab_id[1] = v_id[tab4[i0+1][1]];
              tab_id[2] = p_id[tab4[i0+1][2]];
              tab_id[3] = p_id[tab4[i0+1][3]];         
                                            tab_id[4] = v_id[tab4[i0+1][4]];
              tab_id[5] = p_id[tab4[i0+1][5]];
              CreateTetra(6,tab_id,cellarrayout);
            }
            break;
          case 3:                   // We have one tetrahedron and one wedge
            switch(edges_inter) 
              {
              case 134:
                i0 = 0;
                break;
              case 125:
                i0 = 1;
                break;
              case 236:
                i0 = 2;
                break;
              case 456:
                i0 = 3;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              }                                                              
            if (((v_tetra[i0][ind[0]] < value) && ((planes % 2) == 0)) ||   // Isolate vertex is outside box, so
          ((v_tetra[i0][ind[0]] > value) && ((planes % 2) == 1)))     // the tetrahedron is outside
              {
              tab_id[0] = p_id[tab3[i0][0]];  // Inside
              tab_id[1] = p_id[tab3[i0][1]];
              tab_id[2] = p_id[tab3[i0][2]];
              tab_id[3] = v_id[tab3[i0][3]];
              tab_id[4] = v_id[tab3[i0][4]];
              tab_id[5] = v_id[tab3[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
  
              tab_id[0] = v_id[i0];          // Outside
              tab_id[1] = p_id[tab3[i0][0]];
              tab_id[2] = p_id[tab3[i0][1]];
              tab_id[3] = p_id[tab3[i0][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);                
              }
            else 
              {
              tab_id[0] = v_id[i0];          // Inside
              tab_id[1] = p_id[tab3[i0][0]];
              tab_id[2] = p_id[tab3[i0][1]];
              tab_id[3] = p_id[tab3[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);                
  
              tab_id[0] = p_id[tab3[i0][0]]; // Outside
              tab_id[1] = p_id[tab3[i0][1]];
              tab_id[2] = p_id[tab3[i0][2]];
              tab_id[3] = v_id[tab3[i0][3]];
              tab_id[4] = v_id[tab3[i0][4]];
              tab_id[5] = v_id[tab3[i0][5]];
              CreateTetra(6,tab_id,cellarrayout);
              }
            break;
          case 2:                    // We have one tetrahedron and one pyramid
            switch(edges_inter)      // i1 = vertex of the tetrahedron
              {
              case 12:
                i0 = 0; i1 = 1;  
                break;
              case 13:
                i0 = 1;  i1 = 0;
                break;
              case 23:
                i0 = 2;  i1 = 2; 
                break;
              case 25:
                i0 = 3;  i1 = 1;
                break;
              case 26:
                i0 = 4;  i1 = 2;
                break;
              case 56:
                i0 = 5;  i1 = 3;
                break;
              case 34:
                i0 = 6;  i1 = 0;
                break;
              case 46:
                i0 = 7;  i1 = 3;
                break;
              case 36:
                i0 = 8;  i1 = 2; 
                break;
              case 14:
                i0 = 9;  i1 = 0; 
                break;
              case 15:
                i0 = 10; i1 = 1;
                break;
              case 45:
                i0 = 11; i1 = 3;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              }                                                          
            if (((v_tetra[i1][ind[1]] < value) && ((planes % 2) == 0)) ||   // Isolate vertex is outside box, so
          ((v_tetra[i1][ind[1]] > value) && ((planes % 2) == 1)))    // the tetrahedron is outside
              {
              tab_id[0] = v_id[tab2[i0][0]];  // Inside
              tab_id[1] = p_id[tab2[i0][1]];
              tab_id[2] = p_id[tab2[i0][2]];
              tab_id[3] = v_id[tab2[i0][3]];
              tab_id[4] = v_id[tab2[i0][4]];
              CreateTetra(5,tab_id,newcellArray);
  
              tab_id[0] = v_id[i1];          // Outside
              tab_id[1] = v_id[tab2[i0][4]];
              tab_id[2] = p_id[tab2[i0][1]];
              tab_id[3] = p_id[tab2[i0][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);    
              }
            else 
              {
              tab_id[0] = v_id[i1];          // Inside
              tab_id[1] = v_id[tab2[i0][4]];
              tab_id[2] = p_id[tab2[i0][1]];
              tab_id[3] = p_id[tab2[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);    
  
              tab_id[0] = v_id[tab2[i0][0]];  // Outside
              tab_id[1] = p_id[tab2[i0][1]];
              tab_id[2] = p_id[tab2[i0][2]];
              tab_id[3] = v_id[tab2[i0][3]];
              tab_id[4] = v_id[tab2[i0][4]];
              CreateTetra(5,tab_id,cellarrayout);
              }
            break;
          case 1:              // We have two tetrahedron.
            if ((edges_inter > 6) || (edges_inter < 1)) 
              {
              printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
              continue;
              }                                                         
            if (((v_tetra[tab1[2*edges_inter-1][2]][ind[1]] < value) && ((planes % 2) == 0)) ||   
                                         // Isolate vertex is outside box, so
          ((v_tetra[tab1[2*edges_inter-1][2]][ind[1]] > value) && ((planes % 2) == 1))) 
              // the tetrahedron is outside
              {   
                                          
              tab_id[0] = p_id[0];                           // Inside
              tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
  
              tab_id[0] = p_id[0];                           // Outside
              tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);
  
              }
            else 
              {
              tab_id[0] = p_id[0];                           // Inside
              tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
  
              tab_id[0] = p_id[0];                           // Outside
              tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);
              }
            break;
          }
        } // for all new cells
      cellarray->Delete();
      cellarray = newcellArray;
      } //for all planes

    unsigned totalnewcells = cellarray->GetNumberOfCells();   // Inside
    unsigned idcellnew;
    for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
      {
      cellarray->GetNextCell(npts,v_id);
      newCellId = tets[0]->InsertNextCell(npts,v_id);
      outCD[0]->CopyData(inCD,cellId,newCellId);
      }
    cellarray->Delete();

    totalnewcells = cellarrayout->GetNumberOfCells();  // Outside

    for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
      {
      cellarrayout->GetNextCell(npts,v_id);
      newCellId = tets[1]->InsertNextCell(npts,v_id);
      outCD[1]->CopyData(inCD,cellId,newCellId);
      }
    cellarrayout->Delete();
  }
  arraytetra->Delete();
}


//----------------------------------------------------------------------------  
// ClipHexahedronInOut
// 
// The difference between ClipHexahedron and ClipHexahedronInOut is the outputs.
// The ClipHexahedronInOut generate both outputs: inside and outside the clip hexahedron.
//
void vtkBoxClipDataSet::ClipHexahedronInOut(vtkPoints *newPoints,
                    vtkGenericCell *cell,
                    vtkPointLocator *locator, 
                    vtkCellArray **tets,
                    vtkPointData *inPD, 
                    vtkPointData *outPD,
                    vtkCellData *inCD,
                    vtkIdType cellId,
                    vtkCellData **outCD) 
{  
  vtkIdType     cellType   = cell->GetCellType();
  vtkIdList    *cellIds    = cell->GetPointIds();
  vtkCellArray *arraytetra = vtkCellArray::New();
  vtkPoints    *cellPts    = cell->GetPoints();
  vtkIdType     npts = cellPts->GetNumberOfPoints(); 
  vtkIdType     cellptId[VTK_CELL_SIZE];
  vtkIdType     iid[4];
  vtkIdType    *v_id;
  vtkIdType    *verts, v1, v2;
  vtkIdType     ptId;
  vtkIdType     ptIdout[4];
  vtkIdType     tab_id[6];
  vtkIdType     ptstetra = 4;

  int i,j;
  int allInside;
  int ind[3];

  vtkIdType edges[6][2] = { {0,1}, {1,2}, {2,0},
          {0,3}, {1,3}, {2,3} };  /* Edges Tetrahedron */
  double deltaScalar;
  double p[6], t;
  double v_tetra[4][3];
  double v[3], x[3];
  double *p1, *p2;

  for (i=0; i<npts; i++)
    cellptId[i] = cellIds->GetId(i);
   
  TetraGrid(cellType,npts,cellptId,arraytetra);  // Convert all volume cells to tetrahedra

  unsigned totalnewtetra = arraytetra->GetNumberOfCells();
  for (unsigned idtetranew = 0 ; idtetranew < totalnewtetra; idtetranew++) 
    {
    arraytetra->GetNextCell(ptstetra,v_id);

    float  *pPtr = (float *)cellPts->GetVoidPointer(0);   
    // Test Outside
    unsigned test[6] = {1,1,1,1,1,1};
    for (i=0; i<4; i++)
      { 
      ptIdout[i] = cellIds->GetId(v_id[i]);
      cellPts->GetPoint(v_id[i],v_tetra[i]);

      // Use plane equation
      unsigned k;
      for(k=0;k<6;k++)
        p[k] = n_pl[k][0]*(v_tetra[i][0] - o_pl[k][0]) +  
               n_pl[k][1]*(v_tetra[i][1] - o_pl[k][1]) +  
               n_pl[k][2]*(v_tetra[i][2] - o_pl[k][2]);

     
      for(k=0;k<3;k++)
        {
        if (p[2*k] < 0)
            test[2*k] = 0;
        if (p[2*k+1] < 0)
            test[2*k+1] = 0;
        }
     
      }//for all points of the cell.

    if ((test[0] == 1)|| (test[1] == 1) ||
        (test[2] == 1)|| (test[3] == 1) ||
        (test[4] == 1)|| (test[5] == 1)) 
        {
        for (i=0; i<4; i++)
          if ( locator->InsertUniquePoint(v_tetra[i], iid[i]) )
            outPD->CopyData(inPD,ptIdout[i], iid[i]);
        int newCellId = tets[1]->InsertNextCell(4,iid);
        outCD[1]->CopyData(inCD,cellId,newCellId);
        continue;                   // Tetrahedron is outside.
        }

    for (allInside=1, i=0; i<4; i++)
      {
      ptId = cellIds->GetId(v_id[i]);
      cellPts->GetPoint(v_id[i],v);
  
      for(unsigned k=0;k<6;k++)
        p[k] = n_pl[k][0]*(v[0] - o_pl[k][0]) + 
               n_pl[k][1]*(v[1] - o_pl[k][1]) +  
               n_pl[k][2]*(v[2] - o_pl[k][2]);

      if (!((p[0] <= 0) && (p[1] <= 0) &&
           (p[2] <= 0) && (p[3] <= 0) &&
           (p[4] <= 0) && (p[5] <= 0)))
        allInside = 0;
       
      // Currently all points are injected because of the possibility 
      // of intersection point merging.

      if ( locator->InsertUniquePoint(v, iid[i]) )
        {
        outPD->CopyData(inPD,ptId, iid[i]);
        }
      
      }//for all points of the tetrahedron.

    if ( allInside )
      {  
      int newCellId = tets[0]->InsertNextCell(4,iid);     // Tetrahedron inside.
      outCD[0]->CopyData(inCD,cellId,newCellId);
      continue;
      }

    float  pc[3];
    float *pc1  , *pc2;
    double *pedg1,*pedg2;

    unsigned tab4[6][6] = { {0,1,1,2,3,3},   // Tetrahedron Intersection Cases
          {2,0,0,3,2,1},
          {3,3,2,0,2,1},
          {1,0,2,0,1,3},
          {0,0,1,2,3,3},
          {0,1,2,1,2,3}};
    unsigned tab3[4][6] = { {0,2,1,1,3,2},
          {0,2,1,0,3,2},
          {0,1,2,1,0,3},
          {0,1,2,0,1,2}};
    unsigned tab2[12][5] = { {0,0,1,2,3},
           {2,1,0,1,3},
           {1,0,1,0,3},
           {1,0,1,2,0},
           {3,1,0,1,0},
           {2,0,1,3,0},
           {3,1,0,2,1},
           {2,1,0,0,1},
           {0,0,1,3,1},
           {1,0,1,3,2},
           {3,1,0,0,2},
           {0,0,1,1,2}}; 
    unsigned tab1[12][3] = { {2,3,1},
           {3,2,0},
           {3,0,1},
           {0,3,2},
           {1,3,0},
           {3,1,2},
           {2,1,0},
           {1,2,3},
           {0,2,3},
           {2,0,1},
           {0,1,3},
           {1,0,2}}; 
    
    vtkCellArray *cellarray    = vtkCellArray::New();
    vtkCellArray *cellarrayout = vtkCellArray::New();
    int newCellId = cellarray->InsertNextCell(4,iid);

    // Test Cell intersection with each plane of box
    for (unsigned planes = 0; planes < 6; planes++) 
      {
      switch(planes) 
        {
        case 0:
        case 1:
          ind[0] = 0;
          ind[1] = 1;
          ind[2] = 2;
          break;
        case 2:
        case 3:
          ind[0] = 1;
          ind[1] = 0;
          ind[2] = 2;
          break;
        case 4:
        case 5:
          ind[0] = 2;
          ind[1] = 0;
          ind[2] = 1;
        break;
        }
      
      
    unsigned totalnewcells = cellarray->GetNumberOfCells();
    vtkCellArray *newcellArray = vtkCellArray::New();
      
    for (unsigned idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
      {
      unsigned i0,i1;
      double p_inter[4][3];   
      unsigned num_inter = 0;
      unsigned edges_inter = 0;
      vtkIdType p_id[4];

      cellarray->GetNextCell(npts,v_id);
      
      newPoints->GetPoint(v_id[0],v_tetra[0]); //coord (x,y,z) 
      newPoints->GetPoint(v_id[1],v_tetra[1]); 
      newPoints->GetPoint(v_id[2],v_tetra[2]); 
      newPoints->GetPoint(v_id[3],v_tetra[3]); 

      p[0] = n_pl[planes][0]*(v_tetra[0][0] - o_pl[planes][0]) +
       n_pl[planes][1]*(v_tetra[0][1] - o_pl[planes][1]) +  
       n_pl[planes][2]*(v_tetra[0][2] - o_pl[planes][2]);  
      p[1] = n_pl[planes][0]*(v_tetra[1][0] - o_pl[planes][0]) + 
                         n_pl[planes][1]*(v_tetra[1][1] - o_pl[planes][1]) +  
                         n_pl[planes][2]*(v_tetra[1][2] - o_pl[planes][2]);
      p[2] = n_pl[planes][0]*(v_tetra[2][0] - o_pl[planes][0]) + 
                         n_pl[planes][1]*(v_tetra[2][1] - o_pl[planes][1]) +  
                         n_pl[planes][2]*(v_tetra[2][2] - o_pl[planes][2]);
      p[3] = n_pl[planes][0]*(v_tetra[3][0] - o_pl[planes][0]) + 
                         n_pl[planes][1]*(v_tetra[3][1] - o_pl[planes][1]) +  
                         n_pl[planes][2]*(v_tetra[3][2] - o_pl[planes][2]);
    
      for (int edgeNum=0; edgeNum < 6; edgeNum++)
        {
        verts = edges[edgeNum]; 
        
        p1 = v_tetra[verts[0]];
        p2 = v_tetra[verts[1]];
        double s1 = p[verts[0]];
        double s2 = p[verts[1]];    
        if ( (s1 * s2) <=0)
          {
          deltaScalar = s2 - s1;
              
          if (deltaScalar > 0)
            {
              pedg1 = p1;   pedg2 = p2;
              v1 = verts[0]; v2 = verts[1];
            }
          else
            {
              pedg1 = p2;   pedg2 = p1;
              v1 = verts[1]; v2 = verts[0];
              deltaScalar = -deltaScalar;
              t = s1; s1 = s2; s2 = t;
            }
          
          // linear interpolation
          t = ( deltaScalar == 0.0 ? 0.0 :
                ( - s1) / deltaScalar );
          
          if ( t == 0.0 )
            {
              continue;
            }
          else if ( t == 1.0 )
            {
              continue;
            }
          
          pc1 = pPtr + 3*v1;
          pc2 = pPtr + 3*v2;
          
          for (j=0; j<3; j++)
            {
              x[j]  = pedg1[j]  + t*(pedg2[j] - pedg1[j]);
              pc[j] = pc1[j] + t*(pc2[j] - pc1[j]);
              p_inter[num_inter][j] = x[j];
            }
          
          // Incorporate point into output and interpolate edge data as necessary
          edges_inter = edges_inter * 10 + (edgeNum+1);
    
          if ( locator->InsertUniquePoint(x, p_id[num_inter]) )
            {
              outPD->InterpolateEdge(inPD, p_id[num_inter], cellIds->GetId(v1),
                   cellIds->GetId(v2), t);
            }
          
          num_inter++;        
          }//if edge intersects value
        }//for all edges

      if (num_inter == 0) 
        {  
        unsigned outside = 0;
        for(i=0;i<4;i++)
          {
          if (p[i] > 0) 
            {  // If only one vertex is ouside, so the tetrahedron is outside 
            outside = 1;   // because there is not intersection.
            break; // some vertex could be on plane, so you need to test all vertex
            }
          }
          if (outside == 0)
            {
            // else it is possible intersection if other plane
            newCellId = newcellArray->InsertNextCell(4,v_id);
            }
          else 
            {
            newCellId = tets[1]->InsertNextCell(4,v_id);
            outCD[1]->CopyData(inCD,cellId,newCellId);
            }
          continue;
          }

        switch(num_inter) 
          {
          case 4:                 // We have two wedges
            switch(edges_inter) 
              {
              case 1246:
                i0 = 0;
                break;
              case 2345:
                i0 = 2;
                break;
              case 1356:
                i0 = 4;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              }                                              
            if (((p[3] > 0) && ((planes % 2) == 0)) ||   // The v_tetra[3] is outside box, so
                 ((p[3] > 0) && ((planes % 2) == 1))) // the first wedge is outside
                                                     // ps: v_tetra[3] is always in first wedge (see tab)
              {
              tab_id[0] = p_id[tab4[i0+1][0]];     // Inside
              tab_id[1] = v_id[tab4[i0+1][1]];
              tab_id[2] = p_id[tab4[i0+1][2]];
              tab_id[3] = p_id[tab4[i0+1][3]];          
              tab_id[4] = v_id[tab4[i0+1][4]];
              tab_id[5] = p_id[tab4[i0+1][5]];
              CreateTetra(6,tab_id,newcellArray);
    
              tab_id[0] = p_id[tab4[i0][0]];      // Outside
              tab_id[1] = v_id[tab4[i0][1]];
              tab_id[2] = p_id[tab4[i0][2]];
              tab_id[3] = p_id[tab4[i0][3]];
              tab_id[4] = v_id[tab4[i0][4]];
              tab_id[5] = p_id[tab4[i0][5]];
              CreateTetra(6,tab_id,cellarrayout);
              }
            else 
              {
              tab_id[0] = p_id[tab4[i0][0]];      // Inside
              tab_id[1] = v_id[tab4[i0][1]];
              tab_id[2] = p_id[tab4[i0][2]];
              tab_id[3] = p_id[tab4[i0][3]];
              tab_id[4] = v_id[tab4[i0][4]];
              tab_id[5] = p_id[tab4[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
    
              tab_id[0] = p_id[tab4[i0+1][0]];     // Outside
              tab_id[1] = v_id[tab4[i0+1][1]];
              tab_id[2] = p_id[tab4[i0+1][2]];
              tab_id[3] = p_id[tab4[i0+1][3]];          
              tab_id[4] = v_id[tab4[i0+1][4]];
              tab_id[5] = p_id[tab4[i0+1][5]];
              CreateTetra(6,tab_id,cellarrayout);
              }
    
            break;
          case 3:             // We have one tetrahedron and one wedge
            switch(edges_inter) 
              {
              case 134:
                i0 = 0;
                break;
              case 125:
                i0 = 1;
                break;
              case 236:
                i0 = 2;
                break;
              case 456:
                i0 = 3;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              }                                               
            if (((p[i0] > 0) && ((planes % 2) == 0)) ||   // Isolate vertex is outside box, so
                ((p[i0] > 0) && ((planes % 2) == 1)))    // the tetrahedron is outside
              {
              tab_id[0] = p_id[tab3[i0][0]];  // Inside
              tab_id[1] = p_id[tab3[i0][1]];
              tab_id[2] = p_id[tab3[i0][2]];
              tab_id[3] = v_id[tab3[i0][3]];
              tab_id[4] = v_id[tab3[i0][4]];
              tab_id[5] = v_id[tab3[i0][5]];
              CreateTetra(6,tab_id,newcellArray);
        
              tab_id[0] = v_id[i0];           // Outside
              tab_id[1] = p_id[tab3[i0][0]];
              tab_id[2] = p_id[tab3[i0][1]];
              tab_id[3] = p_id[tab3[i0][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);                
              }
            else 
              {
              tab_id[0] = v_id[i0];           // Inside
              tab_id[1] = p_id[tab3[i0][0]];
              tab_id[2] = p_id[tab3[i0][1]];
              tab_id[3] = p_id[tab3[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);                
        
              tab_id[0] = p_id[tab3[i0][0]];  // Outside
              tab_id[1] = p_id[tab3[i0][1]];
              tab_id[2] = p_id[tab3[i0][2]];
              tab_id[3] = v_id[tab3[i0][3]];
              tab_id[4] = v_id[tab3[i0][4]];
              tab_id[5] = v_id[tab3[i0][5]];
              CreateTetra(6,tab_id,cellarrayout);
              }
            break;
          case 2:              // We have one tetrahedron and one pyramid
            switch(edges_inter)     // i1 = vertex of the tetrahedron
              {
              case 12:
                i0 = 0; i1 = 1;  
                break;
              case 13:
                i0 = 1;  i1 = 0;
                break;
              case 23:
                i0 = 2;  i1 = 2; 
                break;
              case 25:
                i0 = 3;  i1 = 1;
                break;
              case 26:
                i0 = 4;  i1 = 2;
                break;
              case 56:
                i0 = 5;  i1 = 3;
                break;
              case 34:
                i0 = 6;  i1 = 0;
                break;
              case 46:
                i0 = 7;  i1 = 3;
                break;
              case 36:
                i0 = 8;  i1 = 2; 
                break;
              case 14:
                i0 = 9;  i1 = 0; 
                break;
              case 15:
                i0 = 10; i1 = 1;
                break;
              case 45:
                i0 = 11; i1 = 3;
                break;
              default:
                printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
                continue;
              } 
    
            if (((p[i1] > 0) && ((planes % 2) == 0)) ||   // Isolate vertex is outside box, so
    
               ((p[i1] > 0) && ((planes % 2) == 1)))    // the tetrahedron is outside
              {
              tab_id[0] = v_id[tab2[i0][0]];  // Inside
              tab_id[1] = p_id[tab2[i0][1]];
              tab_id[2] = p_id[tab2[i0][2]];
              tab_id[3] = v_id[tab2[i0][3]];
              tab_id[4] = v_id[tab2[i0][4]];
              CreateTetra(5,tab_id,newcellArray);
      
              tab_id[0] = v_id[i1];     // Outside
              tab_id[1] = v_id[tab2[i0][4]];
              tab_id[2] = p_id[tab2[i0][1]];
              tab_id[3] = p_id[tab2[i0][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);    
              }
            else 
              {
              tab_id[0] = v_id[i1];     // Inside
              tab_id[1] = v_id[tab2[i0][4]];
              tab_id[2] = p_id[tab2[i0][1]];
              tab_id[3] = p_id[tab2[i0][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);    
      
              tab_id[0] = v_id[tab2[i0][0]];  // Outside
              tab_id[1] = p_id[tab2[i0][1]];
              tab_id[2] = p_id[tab2[i0][2]];
              tab_id[3] = v_id[tab2[i0][3]];
              tab_id[4] = v_id[tab2[i0][4]];
              CreateTetra(5,tab_id,cellarrayout);
              }
            break;
          case 1:              // We have two tetrahedron.
            if ((edges_inter > 6) || (edges_inter < 1)) 
              {
              printf("Intersection not found: Num_inter = %5d  Edges_inter = %8d\n",num_inter,edges_inter);
              continue;
              }                                        
            if (((p[tab1[2*edges_inter-1][2]] > 0) && ((planes % 2) == 0)) ||   // Isolate vertex is outside box, so
                ((p[tab1[2*edges_inter-1][2]] > 0) && ((planes % 2) == 1)))    // the tetrahedron is outside
              {
              tab_id[0] = p_id[0];                    // Inside
              tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
      
              tab_id[0] = p_id[0];                    // Outside
              tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);
              }
            else 
              {
              tab_id[0] = p_id[0];                    // Inside
              tab_id[1] = v_id[tab1[2*edges_inter-1][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-1][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-1][2]];
              newCellId = newcellArray->InsertNextCell(4,tab_id);
      
              tab_id[0] = p_id[0];                    // Outside
              tab_id[1] = v_id[tab1[2*edges_inter-2][0]];
              tab_id[2] = v_id[tab1[2*edges_inter-2][1]];
              tab_id[3] = v_id[tab1[2*edges_inter-2][2]];
              newCellId = cellarrayout->InsertNextCell(4,tab_id);
              }
            break;
          }
        } // for all new cells
      cellarray->Delete();
      cellarray = newcellArray;
      } //for all planes

    unsigned totalnewcells = cellarray->GetNumberOfCells();    // Inside 

    unsigned idcellnew;
    for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
      {
      cellarray->GetNextCell(npts,v_id);
      newCellId = tets[0]->InsertNextCell(npts,v_id);
      outCD[0]->CopyData(inCD,cellId,newCellId);
      }
    cellarray->Delete();

    totalnewcells = cellarrayout->GetNumberOfCells();    // Outside

    for (idcellnew = 0 ; idcellnew < totalnewcells; idcellnew++) 
      {
      cellarrayout->GetNextCell(npts,v_id);
      newCellId = tets[1]->InsertNextCell(npts,v_id);
      outCD[1]->CopyData(inCD,cellId,newCellId);
      }
    cellarrayout->Delete();
    }
  arraytetra->Delete();
}
