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
#include "vtkLinearContourLineInterpolator.h"
#include <vtkObjectFactory.h>

vtkCxxRevisionMacro(vtkLinearContourLineInterpolator, "$Revision$");
vtkStandardNewMacro(vtkLinearContourLineInterpolator);

//----------------------------------------------------------------------
vtkLinearContourLineInterpolator::vtkLinearContourLineInterpolator()
{
}

//----------------------------------------------------------------------
vtkLinearContourLineInterpolator::~vtkLinearContourLineInterpolator()
{
}

//----------------------------------------------------------------------
int vtkLinearContourLineInterpolator::InterpolateLine( vtkRenderer *vtkNotUsed(ren),
                                                       vtkContourRepresentation *rep,
                                                       int idx1, int idx2 )
{
  return 1;
}

