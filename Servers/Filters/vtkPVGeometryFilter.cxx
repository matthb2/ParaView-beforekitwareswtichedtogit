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
#include "vtkPVGeometryFilter.h"

#include "vtkFloatArray.h"
#include "vtkCellArray.h"
#include "vtkPolygon.h"
#include "vtkCellData.h"
#include "vtkCommand.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkGeometryFilter.h"
#include "vtkCompositeDataIterator.h"
#include "vtkHierarchicalBoxDataSet.h"
#include "vtkHierarchicalBoxOutlineFilter.h"
#include "vtkImageData.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineSource.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkRectilinearGrid.h"
#include "vtkRectilinearGridOutlineFilter.h"
#include "vtkStripper.h"
#include "vtkStructuredGrid.h"
#include "vtkStructuredGridOutlineFilter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkCallbackCommand.h"

vtkCxxRevisionMacro(vtkPVGeometryFilter, "$Revision$");
vtkStandardNewMacro(vtkPVGeometryFilter);

vtkCxxSetObjectMacro(vtkPVGeometryFilter, Controller, vtkMultiProcessController);

//----------------------------------------------------------------------------
vtkPVGeometryFilter::vtkPVGeometryFilter ()
{
  this->OutlineFlag = 0;
  this->UseOutline = 1;
  this->UseStrips = 0;
  this->GenerateCellNormals = 1;
  this->NumberOfRequiredInputs = 0;
  this->DataSetSurfaceFilter = vtkDataSetSurfaceFilter::New();
  this->HierarchicalBoxOutline = vtkHierarchicalBoxOutlineFilter::New();

  // Setup a callback for the internal readers to report progress.
  this->InternalProgressObserver = vtkCallbackCommand::New();
  this->InternalProgressObserver->SetCallback(
    &vtkPVGeometryFilter::InternalProgressCallbackFunction);
  this->InternalProgressObserver->SetClientData(this);

  this->Controller = 0;
  this->SetController(vtkMultiProcessController::GetGlobalController());

  this->OutlineSource = vtkOutlineSource::New();
}

//----------------------------------------------------------------------------
vtkPVGeometryFilter::~vtkPVGeometryFilter ()
{
  this->DataSetSurfaceFilter->Delete();
  this->HierarchicalBoxOutline->Delete();
  this->OutlineSource->Delete();
  this->InternalProgressObserver->Delete();
  this->SetController(0);
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::InternalProgressCallbackFunction(vtkObject*,
                                                           unsigned long,
                                                           void* clientdata,
                                                           void*)
{
  reinterpret_cast<vtkPVGeometryFilter*>(clientdata)
    ->InternalProgressCallback();
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::InternalProgressCallback()
{
  // This limits progress for only the DataSetSurfaceFilter.
  float progress = this->DataSetSurfaceFilter->GetProgress();
  this->UpdateProgress(progress);
  if (this->AbortExecute)
    {
    this->DataSetSurfaceFilter->SetAbortExecute(1);
    }
}

//----------------------------------------------------------------------------
// Specify the input data or filter.
void vtkPVGeometryFilter::SetInput(vtkDataObject *input)
{
  this->vtkProcessObject::SetNthInput(0, input);
}

//----------------------------------------------------------------------------
// Specify the input data or filter.
vtkDataObject *vtkPVGeometryFilter::GetInput()
{
  if (this->NumberOfInputs < 1)
    {
    return NULL;
    }
  
  return this->Inputs[0];
}



//----------------------------------------------------------------------------
int vtkPVGeometryFilter::CheckAttributes(vtkDataObject* input)
{
  if (input->IsA("vtkDataSet"))
    {
    if (static_cast<vtkDataSet*>(input)->CheckAttributes())
      {
      return 1;
      }
    }
  else if (input->IsA("vtkCompositeDataSet"))
    {
    vtkCompositeDataSet* compInput = 
      static_cast<vtkCompositeDataSet*>(input);
    vtkCompositeDataIterator* iter = compInput->NewIterator();
    iter->GoToFirstItem();
    while (!iter->IsDoneWithTraversal())
      {
      vtkDataObject* curDataSet = iter->GetCurrentDataObject();
      if (curDataSet && this->CheckAttributes(curDataSet))
        {
        return 1;
        }
      iter->GoToNextItem();
      }
    iter->Delete();
    }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::ExecuteInformation()
{
  vtkDataObject *output = this->GetOutput();

  if (output)
    { // Execute synchronizes (communicates among processes), so we need
    // all procs to call Execute.
    output->SetMaximumNumberOfPieces(-1);
    }
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::Execute()
{
  vtkDataObject *input = this->GetInput();

  if (input == NULL)
    {
    return;
    }

  if (this->CheckAttributes(input))
    {
    return;
    }

  if (input->IsA("vtkImageData"))
    {
    this->ImageDataExecute(static_cast<vtkImageData*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }

  if (input->IsA("vtkStructuredGrid"))
    {
    this->StructuredGridExecute(static_cast<vtkStructuredGrid*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }

  if (input->IsA("vtkRectilinearGrid"))
    {
    this->RectilinearGridExecute(static_cast<vtkRectilinearGrid*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }

  if (input->IsA("vtkUnstructuredGrid"))
    {
    this->UnstructuredGridExecute(static_cast<vtkUnstructuredGrid*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }
  if (input->IsA("vtkPolyData"))
    {
    this->PolyDataExecute(static_cast<vtkPolyData*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }
  if (input->IsA("vtkDataSet"))
    {
    this->DataSetExecute(static_cast<vtkDataSet*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }
  if (input->IsA("vtkHierarchicalBoxDataSet"))
    {
    this->HierarchicalBoxExecute(static_cast<vtkHierarchicalBoxDataSet*>(input));
    this->ExecuteCellNormals(this->GetOutput());
    return;
    }
  return;
}

//----------------------------------------------------------------------------
// We need to change the mapper.  Now it always flat shades when cell normals
// are available.
void vtkPVGeometryFilter::ExecuteCellNormals(vtkPolyData *output)
{
  if ( ! this->GenerateCellNormals)
    {
    return;
    }

  if (output->GetVerts() && output->GetVerts()->GetNumberOfCells())
    { // We can deal with these later.
    return;
    }
  if (output->GetLines() && output->GetLines()->GetNumberOfCells())
    { // We can deal with these later.
    return;
    }
  if (output->GetStrips() && output->GetStrips()->GetNumberOfCells())
    { // We can deal with these later.
    return;
    }

  vtkIdType* endCellPtr;
  vtkIdType* cellPtr;
  vtkIdType *pts = 0;
  vtkIdType npts = 0;
  double polyNorm[3];
  vtkFloatArray* cellNormals = vtkFloatArray::New();
  cellNormals->SetName("cellNormals");
  cellNormals->SetNumberOfComponents(3);
  cellNormals->Allocate(3*output->GetNumberOfCells());
  vtkCellArray* aPrim = output->GetPolys();
  vtkPoints* p = output->GetPoints();


  cellPtr = aPrim->GetPointer();
  endCellPtr = cellPtr+aPrim->GetNumberOfConnectivityEntries();

  while (cellPtr < endCellPtr)
    {
    npts = *cellPtr++;
    pts = cellPtr;
    cellPtr += npts;

    vtkPolygon::ComputeNormal(p,npts,pts,polyNorm);
    cellNormals->InsertNextTuple(polyNorm);    
    }

  if (cellNormals->GetNumberOfTuples() != output->GetNumberOfCells())
    {
    vtkErrorMacro("Number of cell normals does not match output.");
    cellNormals->Delete();
    return;
    }

  output->GetCellData()->SetNormals(cellNormals);
  cellNormals->Delete();
  cellNormals = NULL;
}


//----------------------------------------------------------------------------
void vtkPVGeometryFilter::HierarchicalBoxExecute(vtkHierarchicalBoxDataSet *input)
{
  vtkHierarchicalBoxDataSet* ds = input->NewInstance();
  ds->ShallowCopy(input);
  this->HierarchicalBoxOutline->SetInput(ds);
  ds->Delete();
  this->HierarchicalBoxOutline->Update();
  this->GetOutput()->ShallowCopy(this->HierarchicalBoxOutline->GetOutput());
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::DataSetExecute(vtkDataSet *input)
{
  vtkPolyData *output = this->GetOutput();

  double bds[6];
  int procid = 0;
  int numProcs = 1;

  if (this->Controller )
    {
    procid = this->Controller->GetLocalProcessId();
    numProcs = this->Controller->GetNumberOfProcesses();
    }

  input->GetBounds(bds);

  if ( procid )
    {
    // Satellite node
    this->Controller->Send(bds, 6, 0, 792390);
    }
  else
    {
    int idx;
    double tmp[6];

    for (idx = 1; idx < numProcs; ++idx)
      {
      this->Controller->Receive(tmp, 6, idx, 792390);
      if (tmp[0] < bds[0])
        {
        bds[0] = tmp[0];
        }
      if (tmp[1] > bds[1])
        {
        bds[1] = tmp[1];
        }
      if (tmp[2] < bds[2])
        {
        bds[2] = tmp[2];
        }
      if (tmp[3] > bds[3])
        {
        bds[3] = tmp[3];
        }
      if (tmp[4] < bds[4])
        {
        bds[4] = tmp[4];
        }
      if (tmp[5] > bds[5])
        {
        bds[5] = tmp[5];
        }
      }
    // only output in process 0.
    this->OutlineSource->SetBounds(bds);
    this->OutlineSource->Update();
    
    output->SetPoints(this->OutlineSource->GetOutput()->GetPoints());
    output->SetLines(this->OutlineSource->GetOutput()->GetLines());
    }
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::DataSetSurfaceExecute(vtkDataSet *input)
{
  vtkDataSet* ds = input->NewInstance();
  ds->ShallowCopy(input);
  this->DataSetSurfaceFilter->SetInput(ds);
  ds->Delete();


  // Observe the progress of the internal filter.
  this->DataSetSurfaceFilter->AddObserver(vtkCommand::ProgressEvent, 
                                          this->InternalProgressObserver);
  this->DataSetSurfaceFilter->Update();
  // The internal filter is finished.  Remove the observer.
  this->DataSetSurfaceFilter->RemoveObserver(this->InternalProgressObserver);

  this->GetOutput()->ShallowCopy(this->DataSetSurfaceFilter->GetOutput());
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::ImageDataExecute(vtkImageData *input)
{
  double *spacing;
  double *origin;
  int *ext;
  double bounds[6];
  vtkPolyData *output = this->GetOutput();

  ext = input->GetWholeExtent();

  // If 2d then default to superclass behavior.
//  if (ext[0] == ext[1] || ext[2] == ext[3] || ext[4] == ext[5] ||
//      !this->UseOutline)
  if (!this->UseOutline)
    {
    this->DataSetSurfaceExecute(input);
    this->OutlineFlag = 0;
    return;
    }  
  this->OutlineFlag = 1;

  //
  // Otherwise, let OutlineSource do all the work
  //
  
  if (output->GetUpdatePiece() == 0)
    {
    spacing = input->GetSpacing();
    origin = input->GetOrigin();
    
    bounds[0] = spacing[0] * ((float)ext[0]) + origin[0];
    bounds[1] = spacing[0] * ((float)ext[1]) + origin[0];
    bounds[2] = spacing[1] * ((float)ext[2]) + origin[1];
    bounds[3] = spacing[1] * ((float)ext[3]) + origin[1];
    bounds[4] = spacing[2] * ((float)ext[4]) + origin[2];
    bounds[5] = spacing[2] * ((float)ext[5]) + origin[2];

    vtkOutlineSource *outline = vtkOutlineSource::New();
    outline->SetBounds(bounds);
    outline->Update();

    output->SetPoints(outline->GetOutput()->GetPoints());
    output->SetLines(outline->GetOutput()->GetLines());
    outline->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::StructuredGridExecute(vtkStructuredGrid *input)
{
  int *ext;
  vtkPolyData *output = this->GetOutput();

  ext = input->GetWholeExtent();

  // If 2d then default to superclass behavior.
//  if (ext[0] == ext[1] || ext[2] == ext[3] || ext[4] == ext[5] ||
//      !this->UseOutline)
  if (!this->UseOutline)
    {
    this->DataSetSurfaceExecute(input);
    this->OutlineFlag = 0;
    return;
    }  
  this->OutlineFlag = 1;

  //
  // Otherwise, let Outline do all the work
  //
  

  vtkStructuredGridOutlineFilter *outline = vtkStructuredGridOutlineFilter::New();
  // Because of streaming, it is important to set the input and not copy it.
  outline->SetInput(input);
  outline->GetOutput()->SetUpdateNumberOfPieces(output->GetUpdateNumberOfPieces());
  outline->GetOutput()->SetUpdatePiece(output->GetUpdatePiece());
  outline->GetOutput()->SetUpdateGhostLevel(output->GetUpdateGhostLevel());
  outline->GetOutput()->Update();

  output->CopyStructure(outline->GetOutput());
  outline->Delete();
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::RectilinearGridExecute(vtkRectilinearGrid *input)
{
  int *ext;
  vtkPolyData *output = this->GetOutput();

  ext = input->GetWholeExtent();

  // If 2d then default to superclass behavior.
//  if (ext[0] == ext[1] || ext[2] == ext[3] || ext[4] == ext[5] ||
//      !this->UseOutline)
  if (!this->UseOutline)
    {
    this->DataSetSurfaceExecute(input);
    this->OutlineFlag = 0;
    return;
    }  
  this->OutlineFlag = 1;

  //
  // Otherwise, let Outline do all the work
  //

  vtkRectilinearGridOutlineFilter *outline = vtkRectilinearGridOutlineFilter::New();
  // Because of streaming, it is important to set the input and not copy it.
  outline->SetInput(input);
  outline->GetOutput()->SetUpdateNumberOfPieces(output->GetUpdateNumberOfPieces());
  outline->GetOutput()->SetUpdatePiece(output->GetUpdatePiece());
  outline->GetOutput()->SetUpdateGhostLevel(output->GetUpdateGhostLevel());
  outline->GetOutput()->Update();

  output->CopyStructure(outline->GetOutput());
  outline->Delete();
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::UnstructuredGridExecute(vtkUnstructuredGrid* input)
{
  if (!this->UseOutline)
    {
    this->OutlineFlag = 0;
    this->DataSetSurfaceExecute(input);
    return;
    }
  
  this->OutlineFlag = 1;

  this->DataSetExecute(input);
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::PolyDataExecute(vtkPolyData *input)
{
  vtkPolyData *out = this->GetOutput(); 

  if (!this->UseOutline)
    {
    this->OutlineFlag = 0;
    if (this->UseStrips)
      {
      vtkPolyData *inCopy = vtkPolyData::New();
      vtkStripper *stripper = vtkStripper::New();
      inCopy->ShallowCopy(input);
      inCopy->RemoveGhostCells(1);
      stripper->SetInput(inCopy);
      stripper->Update();
      out->CopyStructure(stripper->GetOutput());
      out->GetPointData()->ShallowCopy(stripper->GetOutput()->GetPointData());
      out->GetCellData()->ShallowCopy(stripper->GetOutput()->GetCellData());
      inCopy->Delete();
      stripper->Delete();
      }
    else
      {
      out->ShallowCopy(input);
      out->RemoveGhostCells(1);
      }
    return;
    }
  
  this->OutlineFlag = 1;
  this->DataSetExecute(input);
}

//----------------------------------------------------------------------------
void vtkPVGeometryFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if (this->OutlineFlag)
    {
    os << indent << "OutlineFlag: On\n";
    }
  else
    {
    os << indent << "OutlineFlag: Off\n";
    }
  
  os << indent << "UseOutline: " << (this->UseOutline?"on":"off") << endl;
  os << indent << "UseStrips: " << (this->UseStrips?"on":"off") << endl;
  os << indent << "GenerateCellNormals: " << (this->GenerateCellNormals?"on":"off") << endl;
  os << indent << "Controller: " << this->Controller << endl;
}
