/*=========================================================================

Program:   Visualization Toolkit
Module:    $RCSfile$
Language:  C++
Date:      $Date$
Version:   $Revision$

=========================================================================*/
#include "vtkBar.h"
#include "vtkObjectFactory.h"

//----------------------------------------------------------------------------

vtkBar* vtkBar::New()
{
  // First try to create the object from the vtkObjectFactory
  vtkObject* ret = vtkObjectFactory::CreateInstance("vtkBar");
  if(ret)
    {
    return (vtkBar*)ret;
    }
  // If the factory was unable to create the object, then create it here.
  return new vtkBar;
}
