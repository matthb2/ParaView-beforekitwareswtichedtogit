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

#include "vtkPlotGrid.h"

#include "vtkContext2D.h"
#include "vtkPoints2D.h"
#include "vtkPen.h"
#include "vtkAxis.h"
#include "vtkFloatArray.h"

#include "vtkObjectFactory.h"

//-----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkPlotGrid, "$Revision$");
vtkCxxSetObjectMacro(vtkPlotGrid, XAxis, vtkAxis);
vtkCxxSetObjectMacro(vtkPlotGrid, YAxis, vtkAxis);
//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPlotGrid);

//-----------------------------------------------------------------------------
vtkPlotGrid::vtkPlotGrid()
{
  this->XAxis = NULL;
  this->YAxis = NULL;
  this->Point1[0] = 0.0;
  this->Point1[1] = 0.0;
  this->Point2[0] = 0.0;
  this->Point2[1] = 0.0;
}

//-----------------------------------------------------------------------------
vtkPlotGrid::~vtkPlotGrid()
{
  this->SetXAxis(NULL);
  this->SetYAxis(NULL);
}

//-----------------------------------------------------------------------------
bool vtkPlotGrid::Paint(vtkContext2D *painter)
{
  if (!this->XAxis || !this->YAxis)
    {
    // Need axes to define where our grid lines should be drawn
    vtkDebugMacro(<<"No axes set and so grid lines cannot be drawn.");
    return false;
    }
  float ignored; // Values I want to ignore when getting others
  this->XAxis->GetPoint1(&this->Point1[0]);
  this->XAxis->GetPoint2(this->Point2[0], ignored);
  this->YAxis->GetPoint2(ignored, this->Point2[1]);

  // Now do some grid drawing...
  painter->GetPen()->SetWidth(1.0);

  // in x
  if (this->XAxis->GetGridVisible())
    {
    vtkFloatArray *xLines = this->XAxis->GetTickPositions();
    float *xPositions = xLines->GetPointer(0);
    for (int i = 0; i < xLines->GetNumberOfTuples(); ++i)
      {
      painter->DrawLine(xPositions[i], this->Point1[1],
                        xPositions[i], this->Point2[1]);
      }
    }

  // in y
  if (this->YAxis->GetGridVisible())
    {
    vtkFloatArray *yLines = this->YAxis->GetTickPositions();
    float *yPositions = yLines->GetPointer(0);
    for (int i = 0; i < yLines->GetNumberOfTuples(); ++i)
      {
      painter->DrawLine(this->Point1[0], yPositions[i],
                        this->Point2[0], yPositions[i]);
      }
    }

  return true;
}

//-----------------------------------------------------------------------------
void vtkPlotGrid::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

}
