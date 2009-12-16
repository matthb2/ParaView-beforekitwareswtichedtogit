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
#include "vtkTestingObjectFactory.h"

vtkCxxRevisionMacro(vtkTestingInteractor, "$Revision$");
vtkCxxRevisionMacro(vtkTestingObjectFactory, "$Revision$");

VTK_CREATE_CREATE_FUNCTION(vtkTestingInteractor);

vtkTestingObjectFactory::vtkTestingObjectFactory()
{
  this->RegisterOverride("vtkRenderWindowInteractor",
                         "vtkTestingInteractor",
                         "Overrides for testing",
                         1,
                         vtkObjectFactoryCreatevtkTestingInteractor);
}
