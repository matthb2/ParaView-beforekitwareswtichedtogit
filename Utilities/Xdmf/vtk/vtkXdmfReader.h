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
// .NAME vtkXdmfReader - read eXtensible Data Model and Format files
// .SECTION Description
// vtkXdmfReader is a source object that reads XDMF data.
// The output of this reader is a single vtkUnstructuredGrid, vtkStructuredGrid
//  or vtkRectilinearGrid data object.
// The superclass of this class, vtkDataReader, provides many methods for
// controlling the reading of the data file, see vtkDataReader for more
// information.
// .SECTION Caveats
// used the XDMF API
// .SECTION See Also
// vtkDataReader

#ifndef __vtkXdmfReader_h
#define __vtkXdmfReader_h

#include "vtkDataReader.h"

class vtkDoubleArray;
class vtkDataArray;
class vtkRectilinearGrid;
class vtkDataObject;
class vtkRectilinearGrid;
class vtkDataArraySelection;
class vtkCallbackCommand;
class vtkDataSet;

//BTX
class XdmfDOM;
class XdmfFormatMulti;
class XdmfTransform;
class XdmfDataDesc;
class XdmfGrid;
class vtkXdmfDataArray;

class vtkXdmfReaderInternal;

//ETX

class VTK_EXPORT vtkXdmfReader : public vtkDataReader
{
public:
  static vtkXdmfReader* New();
  vtkTypeRevisionMacro(vtkXdmfReader, vtkDataReader);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Get and set the output of this reader.
  vtkDataSet *GetOutput();
  vtkDataSet *GetOutput(int idx);
  void SetOutput(vtkDataSet *output);
  void SetOutput(vtkDataObject *output);

  // Description:
  // Get the data array selection tables used to configure which data
  // arrays are loaded by the reader.
  vtkGetObjectMacro(PointDataArraySelection, vtkDataArraySelection);
  vtkGetObjectMacro(CellDataArraySelection, vtkDataArraySelection);
  
  // Description:  
  // Get the number of point or cell arrays available in the input.
  int GetNumberOfPointArrays();
  int GetNumberOfCellArrays();
  
  // Description:
  // Get the name of the point or cell array with the given index in
  // the input.
  const char* GetPointArrayName(int index);
  const char* GetCellArrayName(int index);
  
  // Description:
  // Get the number of Parameters
  int GetNumberOfParameters();

  // Description:
  // Get Parameter Type
  int GetParameterType(int index);
  int GetParameterType(const char *Name);
  char *GetParameterTypeAsString(int index);
  char *GetParameterTypeAsString(const char *Name);

  // Description:
  // Get start, stride, count
  int GetParameterRange(int index, int Shape[3]);
  int GetParameterRange(const char *Name, int Shape[3]);
  char *GetParameterRangeAsString(int index);
  char *GetParameterRangeAsString(const char *Name);

  // Description:
  // Get Parameter Name
  const char *GetParameterName(int index);

  // Description:
  // Set/Get Parameter Current Index
  int SetParameterIndex(const char *Name, int CurrentIndex); 
  int SetParameterIndex(int ParameterIndex, int CurrentIndex); 
  int GetParameterIndex(const char *Name);
  int GetParameterIndex(int index);

  // Description:
  // Get Length of Parameter
  int GetParameterLength(const char *Name);
  int GetParameterLength(int index);


  // Description:
  // Get the Current Value of the Parameter
  const char *GetParameterValue(int index);
  const char *GetParameterValue(const char *Name);

  // Description:
  // Get/Set whether the point or cell array with the given name is to
  // be read.
  int GetPointArrayStatus(const char* name);
  int GetCellArrayStatus(const char* name);
  void SetPointArrayStatus(const char* name, int status);  
  void SetCellArrayStatus(const char* name, int status);  

  // Description:
  // Set whether the all point or cell arrays are to
  // be read.
  void EnableAllArrays();
  void DisableAllArrays();

  // Description:
  // Get the Low Level XdmfDOM
  const char *GetXdmfDOMHandle();

  // Description:
  // Get the Low Level XdmfGrid
  const char *GetXdmfGridHandle(int idx);

  // Description:
  // Get/Set the current domain name.
  virtual void SetDomainName(const char*);
  vtkGetStringMacro(DomainName);

  // Description:
  // Get/Set the current grid name.
  void SetGridName(const char*);

  // Description:
  // Get number of domains and grids.
  int GetNumberOfDomains();
  int GetNumberOfGrids();
  
  // Description:
  // Get the name of domain or grid at index.
  const char* GetDomainName(int idx);
  const char* GetGridName(int idx);
  int GetGridIndex(const char* name);

  // Description:
  // Set / get stride
  vtkSetVector3Macro(Stride, int);
  vtkGetVector3Macro(Stride, int);

  // Description:
  // Enable grids.
  void EnableGrid(const char* name);
  void EnableGrid(int idx);
  void EnableAllGrids();

  // Description:
  // Disable grids
  void DisableGrid(const char* name);
  void DisableGrid(int idx);
  void DisableAllGrids();

  // Description:
  // Get current enable/disable of the grid
  int GetGridSetting(const char* name);
  int GetGridSetting(int idx);

protected:
  vtkXdmfReader();
  ~vtkXdmfReader();   

  virtual void Execute();
  virtual void ExecuteInformation();

  // Callback registered with the SelectionObserver.
  static void SelectionModifiedCallback(vtkObject* caller, unsigned long eid,
                                        void* clientdata, void* calldata);
  
  // The array selections.
  vtkDataArraySelection* PointDataArraySelection;
  vtkDataArraySelection* CellDataArraySelection;

  // The observer to modify this object when the array selections are
  // modified.
  vtkCallbackCommand* SelectionObserver;

  XdmfDOM         *DOM;
  XdmfFormatMulti *FormatMulti;
  XdmfTransform   *Transform;

  // For converting arrays from XDMF to VTK format
  vtkXdmfDataArray *ArrayConverter;

  char* DomainName;
  char* GridName;

  vtkXdmfReaderInternal* Internals;

  int Stride[3];
  int OutputsInitialized;

  int GridsModified;

  void UpdateGrids();

private:
  vtkXdmfReader(const vtkXdmfReader&); // Not implemented
  void operator=(const vtkXdmfReader&); // Not implemented  
};

#endif //__vtkXdmfReader_h
