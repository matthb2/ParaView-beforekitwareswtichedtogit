/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2000-2001 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name of Kitware nor the names of any contributors may be used
   to endorse or promote products derived from this software without specific 
   prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "vtkClipPlane.h"
#include "vtkObjectFactory.h"

//--------------------------------------------------------------------------
vtkClipPlane* vtkClipPlane::New()
{
  // First try to create the object from the vtkObjectFactory
  vtkObject* ret = vtkObjectFactory::CreateInstance("vtkClipPlane");
  if(ret)
    {
    return (vtkClipPlane*)ret;
    }
  // If the factory was unable to create the object, then create it here.
  return new vtkClipPlane;
}

// Instantiate object with no input and no defined output.
vtkClipPlane::vtkClipPlane()
{
  this->Origin[0] = 0.0;
  this->Origin[1] = 0.0;
  this->Origin[2] = 0.0;
  
  this->Normal[0] = 0.0;
  this->Normal[1] = 0.0;
  this->Normal[2] = 1.0;

  this->Offset = 0.0;
 
  this->PlaneFunction = vtkPlane::New();
  this->SetClipFunction(this->PlaneFunction);
}

vtkClipPlane::~vtkClipPlane()
{
  this->PlaneFunction->Delete();
  this->PlaneFunction = NULL;
}

void vtkClipPlane::Execute()
{
  this->PlaneFunction->SetOrigin(this->Origin);
  this->PlaneFunction->SetNormal(this->Normal);
  
  this->SetValue(this->Offset);
  this->vtkClipDataSet::Execute();
}


void vtkClipPlane::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkClipDataSet::PrintSelf(os,indent);

  os << indent << "Origin: " << this->Origin[0] << ", "
     << this->Origin[1] << ", " << this->Origin[2] << endl;
  os << indent << "Normal: " << this->Normal[0] << ", "
     << this->Normal[1] << ", " << this->Normal[2] << endl;
}
