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
#include "vtkPointSetSource.h"

#include "vtkObjectFactory.h"
#include "vtkPointSet.h"

vtkCxxRevisionMacro(vtkPointSetSource, "$Revision$");

//----------------------------------------------------------------------------
vtkPointSetSource::vtkPointSetSource()
{

}

//----------------------------------------------------------------------------
vtkPointSet *vtkPointSetSource::GetOutput()
{
  if (this->NumberOfOutputs < 1)
    {
    return NULL;
    }
  
  return (vtkPointSet *)(this->Outputs[0]);
}


//----------------------------------------------------------------------------
vtkPointSet *vtkPointSetSource::GetOutput(int idx)
{
  return (vtkPointSet *) this->vtkSource::GetOutput(idx); 
}

//----------------------------------------------------------------------------
void vtkPointSetSource::SetOutput(vtkPointSet *output)
{
  this->vtkSource::SetNthOutput(0, output);
}

//----------------------------------------------------------------------------
void vtkPointSetSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
