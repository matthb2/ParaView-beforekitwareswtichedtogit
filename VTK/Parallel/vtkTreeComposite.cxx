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
#include "vtkTreeComposite.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkTreeComposite, "$Revision$");
vtkStandardNewMacro(vtkTreeComposite);

//-------------------------------------------------------------------------
vtkTreeComposite::vtkTreeComposite()
{
  vtkWarningMacro("vtkTreeComposite is a legacy class and is deprecated in VTK 4.2.  "
                  "Please use vtkCompositeManager instead.  "
                  "The new class defaults to using vtkTreeCompositer, but can use any compositer.");
}
  
//-------------------------------------------------------------------------
vtkTreeComposite::~vtkTreeComposite()
{
}

//-------------------------------------------------------------------------
void vtkTreeComposite::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkCompositeManager::PrintSelf(os, indent);
}
