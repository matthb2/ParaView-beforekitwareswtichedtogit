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
#include "vtkDataSetSubdivisionAlgorithm.h"
#include "vtkStreamingTessellator.h"

int main()
{
  vtkObject *c;
  c = vtkDataSetSubdivisionAlgorithm::New(); c->Print(cout); c->Delete();
  c = vtkStreamingTessellator::New(); c->Print(cout); c->Delete();
  return 0;
}
