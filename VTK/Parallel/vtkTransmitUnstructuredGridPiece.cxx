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
#include "vtkTransmitUnstructuredGridPiece.h"

#include "vtkCellData.h"
#include "vtkExtractUnstructuredGridPiece.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"

vtkCxxRevisionMacro(vtkTransmitUnstructuredGridPiece, "$Revision$");
vtkStandardNewMacro(vtkTransmitUnstructuredGridPiece);

vtkCxxSetObjectMacro(vtkTransmitUnstructuredGridPiece,Controller,
                     vtkMultiProcessController);

//----------------------------------------------------------------------------
vtkTransmitUnstructuredGridPiece::vtkTransmitUnstructuredGridPiece()
{
  this->CreateGhostCells = 1;

  // Controller keeps a reference to this object as well.
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());  
}

//----------------------------------------------------------------------------
vtkTransmitUnstructuredGridPiece::~vtkTransmitUnstructuredGridPiece()
{
  this->SetController(NULL);
}

//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::ComputeInputUpdateExtents(vtkDataObject *out)
{
  vtkUnstructuredGrid *input = this->GetInput();
  
  out = out;
  if (this->GetInput() == NULL)
    {
    vtkErrorMacro("Missing input");
    return;
    }

  if (this->Controller == NULL)
    {
    input->SetUpdateNumberOfPieces(1);
    input->SetUpdatePiece(0);
    input->SetUpdateGhostLevel(0);
    return;
    }
  
  if (this->Controller->GetLocalProcessId() == 0)
    { // Request everything.
    input->SetUpdateNumberOfPieces(1);
    input->SetUpdatePiece(0);
    input->SetUpdateGhostLevel(0);
    }
  else
    { // Request nothing.
    input->SetUpdateNumberOfPieces(0);
    input->SetUpdatePiece(0);
    input->SetUpdateGhostLevel(0);
    }
}

//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::ExecuteInformation()
{
  if (this->GetOutput() == NULL)
    {
    vtkErrorMacro("Missing output");
    return;
    }
  if (this->GetInput())
    {
    this->GetOutput()->CopyInformation(this->GetInput());
    }
  this->GetOutput()->SetMaximumNumberOfPieces(-1);
}
  
//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::Execute()
{
  int procId;

  if (this->Controller == NULL)
    {
    vtkErrorMacro("Could not find Controller.");
    return;
    }

  procId = this->Controller->GetLocalProcessId();
  if (procId == 0)
    {
    // cerr << "Root Execute\n";
    this->RootExecute();
    }
  else
    {
    // cerr << "Satellite Execute " << procId << endl;
    this->SatelliteExecute(procId);
    }
}

//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::RootExecute()
{
  vtkUnstructuredGrid *input = this->GetInput();
  vtkUnstructuredGrid *tmp = vtkUnstructuredGrid::New();
  vtkUnstructuredGrid *output = this->GetOutput();
  vtkExtractUnstructuredGridPiece *extract = vtkExtractUnstructuredGridPiece::New();
  int ext[3];
  int numProcs, i;

  if (output->GetUpdatePiece() != 0)
    {
    vtkWarningMacro(<< "Piece " << output->GetUpdatePiece() 
                    << " does not match process 0.  " 
                    << "Altering request to try to avoid a deadlock.");
    }

  // First, set up the pipeline and handle local request.
  tmp->ShallowCopy(input);
  tmp->SetReleaseDataFlag(0);
  extract->SetCreateGhostCells(this->CreateGhostCells);
  extract->SetInput(tmp);
  extract->GetOutput()->SetUpdateNumberOfPieces(
                                output->GetUpdateNumberOfPieces());
  extract->GetOutput()->SetUpdatePiece(output->GetUpdatePiece());
  extract->GetOutput()->SetUpdateGhostLevel(output->GetUpdateGhostLevel());

  extract->Update();

  // Copy geometry without copying information.
  output->CopyStructure(extract->GetOutput());
  output->GetPointData()->PassData(extract->GetOutput()->GetPointData());
  output->GetCellData()->PassData(extract->GetOutput()->GetCellData());
  vtkFieldData*  inFd = extract->GetOutput()->GetFieldData();
  vtkFieldData* outFd = output->GetFieldData();
  if (inFd && outFd)
    {
    outFd->PassData(inFd);
    }

  // Now do each of the satellite requests.
  numProcs = this->Controller->GetNumberOfProcesses();
  for (i = 1; i < numProcs; ++i)
    {
    this->Controller->Receive(ext, 3, i, 22341);
    extract->GetOutput()->SetUpdateNumberOfPieces(ext[1]);
    extract->GetOutput()->SetUpdatePiece(ext[0]);
    extract->GetOutput()->SetUpdateGhostLevel(ext[2]);
    extract->Update();
    this->Controller->Send(extract->GetOutput(), i, 22342);
    }
  tmp->Delete();
  extract->Delete();
}

//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::SatelliteExecute(int)
{
  vtkUnstructuredGrid *tmp = vtkUnstructuredGrid::New();
  vtkUnstructuredGrid *output = this->GetOutput();
  int ext[3];

  ext[0] = output->GetUpdatePiece();
  ext[1] = output->GetUpdateNumberOfPieces();
  ext[2] = output->GetUpdateGhostLevel();

  this->Controller->Send(ext, 3, 0, 22341);
  this->Controller->Receive(tmp, 0, 22342);

  // Copy geometry without copying information.
  output->CopyStructure(tmp);
  output->GetPointData()->PassData(tmp->GetPointData());
  output->GetCellData()->PassData(tmp->GetCellData());

  tmp->Delete();}

//----------------------------------------------------------------------------
void vtkTransmitUnstructuredGridPiece::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  
  os << indent << "Create Ghost Cells: " << (this->CreateGhostCells ? "On\n" : "Off\n");
  
  os << indent << "Controller: (" << this->Controller << ")\n";

}

