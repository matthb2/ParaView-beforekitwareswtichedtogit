/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkPickFilter - Find nearest point and cell.
// .SECTION Description
// This filter is for picking points and cell in paraview.
// It executes in parallel on distributed data sets.  
// It assumes MPI we have an MPI controller.  The data remains distributed.
// The user sets a point, and the filter finds the nearest
// input point or cell.  

#ifndef __vtkPickFilter_h
#define __vtkPickFilter_h

#include "vtkUnstructuredGridSource.h"

class vtkMultiProcessController;
class vtkIdList;
class vtkIntArray;
class vtkPoints;
class vtkDataSet;
class vtkAppendFilter;

class VTK_EXPORT vtkPickFilter : public vtkUnstructuredGridSource
{
public:
  static vtkPickFilter *New();
  vtkTypeRevisionMacro(vtkPickFilter,vtkUnstructuredGridSource);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Multiple inputs for multiblock.
  void AddInput(vtkDataSet* input);
  void AddInput(vtkDataObject*){vtkErrorMacro("NotDefined");}
  vtkDataSet* GetInput(int idx);
  void RemoveInput(vtkDataSet* input);
  void RemoveInput(vtkDataObject*){vtkErrorMacro("NotDefined");}
  void RemoveAllInputs();

  // Description:
  // Set your picking point here.
  vtkSetVector3Macro(WorldPoint,double);
  vtkGetVector3Macro(WorldPoint,double);

  // Description:
  // Select whether you are using a world point to pick, or
  // a cell / point id.
  vtkSetMacro(PickCell,int);
  vtkGetMacro(PickCell,int);
  vtkBooleanMacro(PickCell,int);

  // Description:
  // Select whether you are picking point or cells.
  // The default value of this flag is off (use world point).
  vtkSetMacro(UseIdToPick,int);
  vtkGetMacro(UseIdToPick,int);
  vtkBooleanMacro(UseIdToPick,int);
  
  // Description:
  // If using an Id to pick, set the ID with this method.
  vtkSetMacro(Id,vtkIdType);
  vtkGetMacro(Id,vtkIdType);
  
  // Descrption:
  // If the input point/cell attributes has an array with this name,
  // then it is used to find the point.  Defaults to GlobalId.
  vtkSetStringMacro(GlobalPointIdArrayName);
  vtkGetStringMacro(GlobalPointIdArrayName);
  vtkSetStringMacro(GlobalCellIdArrayName);
  vtkGetStringMacro(GlobalCellIdArrayName);
  
  // Description:
  // This is set by default (if compiled with MPI).
  // User can override this default.
  void SetController(vtkMultiProcessController* controller);
  
protected:
  vtkPickFilter();
  ~vtkPickFilter();

  virtual void Execute();
  void PointExecute();
  void CellExecute();
  int CompareProcesses(double bestDist2);
  void CreateOutput(vtkIdList* regionCellIds);

  virtual void IdExecute();
  int CellIdExecute(vtkDataSet* input, int inputIdx,  vtkAppendFilter* append);
  int PointIdExecute(vtkDataSet* input, int inputIdx, vtkAppendFilter* append);

  // Flag that toggles between picking cells or picking points.
  int PickCell;

  // Input pick point.
  double WorldPoint[3];

  int UseIdToPick;
  vtkIdType Id;
  char* GlobalPointIdArrayName;
  char* GlobalCellIdArrayName;

  vtkMultiProcessController* Controller;

  // Index is the input id, value is the output id.
  vtkIdList* PointMap;
  // Index is the output id, value is the input id.
  vtkIdList* RegionPointIds;

  // I need this because I am converting this filter
  // to have multiple inputs, and removing the layer feature
  // at the same time.  Maps can only be from one input.
  int BestInputIndex;

  // Returns outputId.
  vtkIdType InsertIdInPointMap(vtkIdType inId);
  void InitializePointMap(vtkIdType numerOfInputPoints);
  void DeletePointMap();
  int ListContainsId(vtkIdList* ids, vtkIdType id);

  // Locator did no do what I wanted.
  vtkIdType FindPointId(double pt[3], vtkDataSet* input);

private:
  vtkPickFilter(const vtkPickFilter&);  // Not implemented.
  void operator=(const vtkPickFilter&);  // Not implemented.
};

#endif


