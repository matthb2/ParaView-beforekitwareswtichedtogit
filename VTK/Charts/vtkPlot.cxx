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

#include "vtkPlot.h"

#include "vtkPen.h"
#include "vtkTable.h"
#include "vtkDataObject.h"
#include "vtkIdTypeArray.h"
#include "vtkContextMapper2D.h"
#include "vtkObjectFactory.h"

#include "vtkStdString.h"

vtkCxxRevisionMacro(vtkPlot, "$Revision$");
vtkCxxSetObjectMacro(vtkPlot, Selection, vtkIdTypeArray);

//-----------------------------------------------------------------------------
vtkPlot::vtkPlot()
{
  this->Pen = vtkPen::New();
  this->Pen->SetWidth(2.0);
  this->Label = NULL;
  this->UseIndexForXSeries = false;
  this->Data = vtkContextMapper2D::New();
  this->Selection = NULL;
}

//-----------------------------------------------------------------------------
vtkPlot::~vtkPlot()
{
  if (this->Pen)
    {
    this->Pen->Delete();
    this->Pen = NULL;
    }
  if (this->Data)
    {
    this->Data->Delete();
    this->Data = NULL;
    }
  this->SetLabel(NULL);
}

//-----------------------------------------------------------------------------
void vtkPlot::SetColor(unsigned char r, unsigned char g, unsigned char b,
                       unsigned char a)
{
  this->Pen->SetColor(r, g, b, a);
}

//-----------------------------------------------------------------------------
void vtkPlot::SetColor(double r, double g, double b)
{
  this->Pen->SetColorF(r, g, b);
}


//-----------------------------------------------------------------------------
void vtkPlot::SetWidth(float width)
{
  this->Pen->SetWidth(width);
}

//-----------------------------------------------------------------------------
float vtkPlot::GetWidth()
{
  return this->Pen->GetWidth();
}

//-----------------------------------------------------------------------------
void vtkPlot::GetColor(double rgb[3])
{
  this->Pen->GetColorF(rgb);
}

//-----------------------------------------------------------------------------
void vtkPlot::SetInput(vtkTable *table)
{
  this->Data->SetInput(table);
}

//-----------------------------------------------------------------------------
void vtkPlot::SetInput(vtkTable *table, const char *xColumn,
                       const char *yColumn)
{
  if (!xColumn || !yColumn)
    {
    vtkErrorMacro(<< "Called with null arguments for X or Y column.")
    }
  vtkDebugMacro(<< "Setting input, X column = \"" << vtkstd::string(xColumn)
                << "\", " << "Y column = \"" << vtkstd::string(yColumn) << "\"");

  this->Data->SetInput(table);
  this->Data->SetInputArrayToProcess(0, 0, 0,
                                     vtkDataObject::FIELD_ASSOCIATION_ROWS,
                                     xColumn);
  this->Data->SetInputArrayToProcess(1, 0, 0,
                                     vtkDataObject::FIELD_ASSOCIATION_ROWS,
                                     yColumn);
}

//-----------------------------------------------------------------------------
void vtkPlot::SetInput(vtkTable *table, vtkIdType xColumn,
                       vtkIdType yColumn)
{
  this->SetInput(table,
                 table->GetColumnName(xColumn),
                 table->GetColumnName(yColumn));
}

//-----------------------------------------------------------------------------
void vtkPlot::SetInputArray(int index, const char *name)
{
  this->Data->SetInputArrayToProcess(index, 0, 0,
                                     vtkDataObject::FIELD_ASSOCIATION_ROWS,
                                     name);
}

//-----------------------------------------------------------------------------
void vtkPlot::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
