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
#include "vtkTextWidget.h"
#include "vtkTextRepresentation.h"
#include "vtkObjectFactory.h"
#include "vtkCommand.h"

vtkCxxRevisionMacro(vtkTextWidget, "$Revision$");
vtkStandardNewMacro(vtkTextWidget);

//-------------------------------------------------------------------------
vtkTextWidget::vtkTextWidget()
{
}

//-------------------------------------------------------------------------
vtkTextWidget::~vtkTextWidget()
{
}

//----------------------------------------------------------------------
void vtkTextWidget::CreateDefaultRepresentation()
{
  if ( ! this->WidgetRep )
    {
    this->WidgetRep = vtkTextRepresentation::New();
    }
}

//-------------------------------------------------------------------------
void vtkTextWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  
}
