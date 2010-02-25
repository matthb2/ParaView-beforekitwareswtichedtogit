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

#include "vtkPlotBar.h"

#include "vtkContext2D.h"
#include "vtkPen.h"
#include "vtkBrush.h"
#include "vtkContextDevice2D.h"
#include "vtkContextMapper2D.h"
#include "vtkPoints2D.h"
#include "vtkTable.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"
#include "vtkExecutive.h"
#include "vtkTimeStamp.h"
#include "vtkInformation.h"

#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkPlotBar, "$Revision$");

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPlotBar);

//-----------------------------------------------------------------------------
vtkPlotBar::vtkPlotBar()
{
  this->Points = 0;
  this->Label = 0;
  this->Width = 1.0;
  this->Pen->SetWidth(1.0);
  this->Offset = 1.0;
}

//-----------------------------------------------------------------------------
vtkPlotBar::~vtkPlotBar()
{
  if (this->Points)
    {
    this->Points->Delete();
    this->Points = NULL;
    }
}

//-----------------------------------------------------------------------------
bool vtkPlotBar::Paint(vtkContext2D *painter)
{
  // This is where everything should be drawn, or dispatched to other methods.
  vtkDebugMacro(<< "Paint event called in vtkPlotBar.");

  if (!this->Visible)
    {
    return false;
    }

  // First check if we have an input
  vtkTable *table = this->Data->GetInput();
  if (!table)
    {
    vtkDebugMacro(<< "Paint event called with no input table set.");
    return false;
    }
  else if(this->Data->GetMTime() > this->BuildTime ||
          table->GetMTime() > this->BuildTime ||
          this->MTime > this->BuildTime)
    {
    vtkDebugMacro(<< "Paint event called with outdated table cache. Updating.");
    this->UpdateTableCache(table);
    }

  // Now add some decorations for our selected points...
  if (this->Selection)
    {
    vtkDebugMacro(<<"Selection set " << this->Selection->GetNumberOfTuples());
    }
  else
    {
    vtkDebugMacro("No selection set.");
    }

  // Now to plot the points
  if (this->Points)
    {
    painter->ApplyPen(this->Pen);
    painter->ApplyBrush(this->Brush);
    int n = this->Points->GetNumberOfPoints();
    float *f = vtkFloatArray::SafeDownCast(this->Points->GetData())->GetPointer(0);

    for (int i = 0; i < n; ++i)
      {
      painter->DrawRect(f[2*i]-this->Offset, 0.0, this->Width, f[2*i+1]);
      }
    }

  return true;
}

//-----------------------------------------------------------------------------
bool vtkPlotBar::PaintLegend(vtkContext2D *painter, float rect[4])
{
  painter->ApplyPen(this->Pen);
  painter->ApplyBrush(this->Brush);
  painter->DrawRect(rect[0], rect[1], rect[2], rect[3]);
  return true;
}

//-----------------------------------------------------------------------------
void vtkPlotBar::GetBounds(double bounds[4])
{
  // Get the x and y arrays (index 0 and 1 respectively)
  vtkTable *table = this->Data->GetInput();
  vtkDataArray *x = this->Data->GetInputArrayToProcess(0, table);
  vtkDataArray *y = this->Data->GetInputArrayToProcess(1, table);

  if (this->UseIndexForXSeries && y)
    {
    bounds[0] = 0;
    bounds[1] = y->GetSize();
    y->GetRange(&bounds[2]);
    }
  else if (x && y)
    {
    x->GetRange(&bounds[0]);
    y->GetRange(&bounds[2]);
    }
  // Bar plots always have one of the y bounds at the orgin
  if (bounds[2] < bounds[3])
    {
    bounds[2] = 0.0;
    }
  else
    {
    bounds[3] = 0.0;
    }
  vtkDebugMacro(<< "Bounds: " << bounds[0] << "\t" << bounds[1] << "\t"
                << bounds[2] << "\t" << bounds[3]);
}

//-----------------------------------------------------------------------------
void vtkPlotBar::SetWidth(float width)
{
  this->Width = width;
}

//-----------------------------------------------------------------------------
float vtkPlotBar::GetWidth()
{
  return this->Width;
}

//-----------------------------------------------------------------------------
void vtkPlotBar::SetColor(unsigned char r, unsigned char g, unsigned char b,
                       unsigned char a)
{
  this->Brush->SetColor(r, g, b, a);
}

//-----------------------------------------------------------------------------
void vtkPlotBar::SetColor(double r, double g, double b)
{
  this->Brush->SetColorF(r, g, b);
}

//-----------------------------------------------------------------------------
void vtkPlotBar::GetColor(double rgb[3])
{
  this->Brush->GetColorF(rgb);
}

//-----------------------------------------------------------------------------
namespace {

// Copy the two arrays into the points array
template<class A>
void CopyToPointsSwitch(vtkPoints2D *points, A *a, vtkDataArray *b, int n)
{
  switch(b->GetDataType())
    {
    vtkTemplateMacro(
        CopyToPoints(points, a, static_cast<VTK_TT*>(b->GetVoidPointer(0)), n));
    }
}

// Copy the two arrays into the points array
template<class A, class B>
void CopyToPoints(vtkPoints2D *points, A *a, B *b, int n)
{
  points->SetNumberOfPoints(n);
  for (int i = 0; i < n; ++i)
    {
    points->SetPoint(i, a[i], b[i]);
    }
}

// Copy one array into the points array, use the index of that array as x
template<class A>
void CopyToPoints(vtkPoints2D *points, A *a, int n)
{
  points->SetNumberOfPoints(n);
  for (int i = 0; i < n; ++i)
    {
    points->SetPoint(i, i, a[i]);
    }
}

}

//-----------------------------------------------------------------------------
bool vtkPlotBar::UpdateTableCache(vtkTable *table)
{
  // Get the x and y arrays (index 0 and 1 respectively)
  vtkDataArray* x = this->Data->GetInputArrayToProcess(0, table);
  vtkDataArray* y = this->Data->GetInputArrayToProcess(1, table);
  if (!x && !this->UseIndexForXSeries)
    {
    vtkErrorMacro(<< "No X column is set (index 0).");
    return false;
    }
  else if (!y)
    {
    vtkErrorMacro(<< "No Y column is set (index 1).");
    return false;
    }
  else if (x->GetSize() != y->GetSize() && !this->UseIndexForXSeries)
    {
    vtkErrorMacro("The x and y columns must have the same number of elements.");
    return false;
    }

  if (!this->Points)
    {
    this->Points = vtkPoints2D::New();
    }

  // Now copy the components into their new columns
  if (this->UseIndexForXSeries)
    {
    switch(y->GetDataType())
      {
        vtkTemplateMacro(
            CopyToPoints(this->Points,
                         static_cast<VTK_TT*>(y->GetVoidPointer(0)),
                         y->GetSize()));
      }
    }
  else
    {
    switch(x->GetDataType())
      {
      vtkTemplateMacro(
          CopyToPointsSwitch(this->Points,
                             static_cast<VTK_TT*>(x->GetVoidPointer(0)),
                             y, x->GetSize()));
      }
    }
  this->BuildTime.Modified();
  return true;
}

//-----------------------------------------------------------------------------
void vtkPlotBar::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
