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
// .NAME vtkDataSetSurfaceFilter - Extracts outer (polygonal) surface.
// .SECTION Description
// vtkDataSetSurfaceFilter is a faster version of vtkGeometry filter, but it 
// does not have an option to select bounds.  It may use more memory than
// vtkGeometryFilter.  It only has one option: whether to use triangle strips 
// when the input type is structured.

// .SECTION See Also
// vtkGeometryFilter vtkStructuredGridGeometryFilter.

#ifndef __vtkDataSetSurfaceFilter_h
#define __vtkDataSetSurfaceFilter_h

#include "vtkDataSetToPolyDataFilter.h"


class vtkPointData;
class vtkPoints;
//BTX
// Helper structure for hashing faces.
struct vtkFastGeomQuadStruct
{
  vtkIdType p0;
  vtkIdType p1;
  vtkIdType p2;
  vtkIdType p3;
  vtkIdType SourceId;
  struct vtkFastGeomQuadStruct *Next;
};
typedef struct vtkFastGeomQuadStruct vtkFastGeomQuad;
//ETX

class VTK_GRAPHICS_EXPORT vtkDataSetSurfaceFilter : public vtkDataSetToPolyDataFilter
{
public:
  static vtkDataSetSurfaceFilter *New();
  vtkTypeRevisionMacro(vtkDataSetSurfaceFilter,vtkDataSetToPolyDataFilter);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // When input is structured data, this flag will generate faces with
  // triangle strips.  This should render faster and use less memory, but no
  // cell data is copied.  By default, UseStrips is Off.
  vtkSetMacro(UseStrips, int);
  vtkGetMacro(UseStrips, int);
  vtkBooleanMacro(UseStrips, int);

protected:
  vtkDataSetSurfaceFilter();
  ~vtkDataSetSurfaceFilter();

  int UseStrips;
  
  void ComputeInputUpdateExtents(vtkDataObject *output);

  void Execute();
  void StructuredExecute(vtkDataSet *input, int *ext);
  void UnstructuredGridExecute();
  void DataSetExecute();
  void ExecuteInformation();

  // Helper methods.
  void ExecuteFaceStrips(vtkDataSet *input, int maxFlag, int *ext,
                         int aAxis, int bAxis, int cAxis);
  void ExecuteFaceQuads(vtkDataSet *input, int maxFlag, int *ext,
                        int aAxis, int bAxis, int cAxis);

  void InitializeQuadHash(vtkIdType numPoints);
  void DeleteQuadHash();
  void InsertQuadInHash(vtkIdType a, vtkIdType b, vtkIdType c, vtkIdType d,
                        vtkIdType sourceId);
  void InsertTriInHash(vtkIdType a, vtkIdType b, vtkIdType c,
                       vtkIdType sourceId);
  void InitQuadHashTraversal();
  vtkFastGeomQuad *GetNextVisibleQuadFromHash();

  vtkFastGeomQuad **QuadHash;
  vtkIdType QuadHashLength;
  vtkFastGeomQuad *QuadHashTraversal;
  vtkIdType QuadHashTraversalIndex;

  vtkIdType *PointMap;
  vtkIdType GetOutputPointId(vtkIdType inPtId, vtkDataSet *input, 
                             vtkPoints *outPts, vtkPointData *outPD);
  
  vtkIdType NumberOfNewCells;
  
  // Better memory allocation for faces (hash)
  void InitFastGeomQuadAllocation(int numberOfCells);
  vtkFastGeomQuad* NewFastGeomQuad();
  void DeleteAllFastGeomQuads();
  // -----
  int FastGeomQuadArrayLength;
  int NumberOfFastGeomQuadArrays;
  vtkFastGeomQuad** FastGeomQuadArrays;
  // These indexes allow us to find the next available face.
  int NextArrayIndex;
  int NextQuadIndex;

private:
  vtkDataSetSurfaceFilter(const vtkDataSetSurfaceFilter&);  // Not implemented.
  void operator=(const vtkDataSetSurfaceFilter&);  // Not implemented.
};

#endif


