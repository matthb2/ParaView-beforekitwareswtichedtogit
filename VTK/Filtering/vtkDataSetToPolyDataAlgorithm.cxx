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
#include "vtkDataSetToPolyDataAlgorithm.h"

#include "vtkInformation.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkDataSetToPolyDataAlgorithm, "$Revision$");
vtkStandardNewMacro(vtkDataSetToPolyDataAlgorithm);

//----------------------------------------------------------------------------
int vtkDataSetToPolyDataAlgorithm::FillInputPortInformation(
  int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}

//----------------------------------------------------------------------------
void vtkDataSetToPolyDataAlgorithm::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
