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
#include "vtkThreshold.h"

#include "vtkCell.h"
#include "vtkCellData.h"
#include "vtkIdList.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"

vtkCxxRevisionMacro(vtkThreshold, "$Revision$");
vtkStandardNewMacro(vtkThreshold);

// Construct with lower threshold=0, upper threshold=1, and threshold 
// function=upper AllScalars=1.
vtkThreshold::vtkThreshold()
{
  this->LowerThreshold         = 0.0;
  this->UpperThreshold         = 1.0;
  this->AllScalars             = 1;
  this->AttributeMode          = VTK_ATTRIBUTE_MODE_USE_POINT_DATA;
  this->ThresholdFunction      = &vtkThreshold::Upper;
  this->InputScalarsSelection  = NULL;
  this->ComponentMode          = VTK_COMPONENT_MODE_USE_SELECTED;
  this->SelectedComponent      = 0;
}

vtkThreshold::~vtkThreshold()
{
  // This frees the string
  this->SetInputScalarsSelection(NULL);
}

// Criterion is cells whose scalars are less or equal to lower threshold.
void vtkThreshold::ThresholdByLower(double lower) 
{
  if ( this->LowerThreshold != lower || 
       this->ThresholdFunction != &vtkThreshold::Lower)
    {
    this->LowerThreshold = lower; 
    this->ThresholdFunction = &vtkThreshold::Lower;
    this->Modified();
    }
}
                           
// Criterion is cells whose scalars are greater or equal to upper threshold.
void vtkThreshold::ThresholdByUpper(double upper)
{
  if ( this->UpperThreshold != upper ||
       this->ThresholdFunction != &vtkThreshold::Upper)
    {
    this->UpperThreshold = upper; 
    this->ThresholdFunction = &vtkThreshold::Upper;
    this->Modified();
    }
}
                           
// Criterion is cells whose scalars are between lower and upper thresholds.
void vtkThreshold::ThresholdBetween(double lower, double upper)
{
  if ( this->LowerThreshold != lower || this->UpperThreshold != upper ||
       this->ThresholdFunction != &vtkThreshold::Between)
    {
    this->LowerThreshold = lower; 
    this->UpperThreshold = upper;
    this->ThresholdFunction = &vtkThreshold::Between;
    this->Modified();
    }
}
  
void vtkThreshold::Execute()
{
  vtkIdType cellId, newCellId;
  vtkIdList *cellPts, *pointMap;
  vtkIdList *newCellPts;
  vtkCell *cell;
  vtkPoints *newPoints;
  int i, ptId, newId, numPts;
  int numCellPts;
  double x[3];
  vtkDataSet *input = this->GetInput();
  
  if (!input)
    {
    vtkErrorMacro(<<"No input, Can't Execute");
    }
  vtkUnstructuredGrid *output = this->GetOutput();
  vtkPointData *pd=input->GetPointData(), *outPD=output->GetPointData();
  vtkCellData *cd=input->GetCellData(), *outCD=output->GetCellData();
  vtkDataArray *pointScalars;
  vtkDataArray *cellScalars;
  int keepCell, usePointScalars;

  vtkDebugMacro(<< "Executing threshold filter");
  
  pointScalars=pd->GetScalars(this->InputScalarsSelection);
  cellScalars=cd->GetScalars(this->InputScalarsSelection);

  if ( !(pointScalars || cellScalars) )
    {
    vtkDebugMacro(<<"No scalar data to threshold");
    return;
    }

  outPD->CopyAllocate(pd);
  outCD->CopyAllocate(cd);

  numPts = input->GetNumberOfPoints();
  output->Allocate(input->GetNumberOfCells());
  newPoints = vtkPoints::New();
  newPoints->Allocate(numPts);

  pointMap = vtkIdList::New(); //maps old point ids into new
  pointMap->SetNumberOfIds(numPts);
  for (i=0; i < numPts; i++)
    {
    pointMap->SetId(i,-1);
    }

  // Determine which scalar data to use for thresholding
  if ( this->AttributeMode == VTK_ATTRIBUTE_MODE_DEFAULT )
    {
    if ( pointScalars != NULL)
      {
      usePointScalars = 1;
      }
    else
      {
      usePointScalars = 0;
      }
    }
  else if ( this->AttributeMode == VTK_ATTRIBUTE_MODE_USE_POINT_DATA )
    {
    usePointScalars = 1;
    }
  else
    {
    usePointScalars = 0;
    }

  // Check on scalar consistency
  if ( usePointScalars && !pointScalars )
    {
    vtkErrorMacro(<<"Can't use point scalars because there are none");
    return;
    }
  else if ( !usePointScalars && !cellScalars )
    {
    vtkErrorMacro(<<"Can't use cell scalars because there are none");
    return;
    }

  newCellPts = vtkIdList::New();     

  // Check that the scalars of each cell satisfy the threshold criterion
  for (cellId=0; cellId < input->GetNumberOfCells(); cellId++)
    {
    cell = input->GetCell(cellId);
    cellPts = cell->GetPointIds();
    numCellPts = cell->GetNumberOfPoints();
    
    if ( usePointScalars )
      {
      if (this->AllScalars)
        {
        keepCell = 1;
        for ( i=0; keepCell && (i < numCellPts); i++)
          {
          ptId = cellPts->GetId(i);
          keepCell = this->EvaluateComponents( pointScalars, ptId );
          }
        }
      else
        {
        keepCell = 0;
        for ( i=0; (!keepCell) && (i < numCellPts); i++)
          {
          ptId = cellPts->GetId(i);
          keepCell = this->EvaluateComponents( pointScalars, ptId );
          }
        }
      }
    else //use cell scalars
      {
      keepCell = this->EvaluateComponents( cellScalars, cellId );
      }
    
    if (  numCellPts > 0 && keepCell )
      {
      // satisfied thresholding (also non-empty cell, i.e. not VTK_EMPTY_CELL)
      for (i=0; i < numCellPts; i++)
        {
        ptId = cellPts->GetId(i);
        if ( (newId = pointMap->GetId(ptId)) < 0 )
          {
          input->GetPoint(ptId, x);
          newId = newPoints->InsertNextPoint(x);
          pointMap->SetId(ptId,newId);
          outPD->CopyData(pd,ptId,newId);
          }
        newCellPts->InsertId(i,newId);
        }
      newCellId = output->InsertNextCell(cell->GetCellType(),newCellPts);
      outCD->CopyData(cd,cellId,newCellId);
      newCellPts->Reset();
      } // satisfied thresholding
    } // for all cells

  vtkDebugMacro(<< "Extracted " << output->GetNumberOfCells() 
                << " number of cells.");

  // now clean up / update ourselves
  pointMap->Delete();
  newCellPts->Delete();
  
  output->SetPoints(newPoints);
  newPoints->Delete();

  output->Squeeze();
}

int vtkThreshold::EvaluateComponents( vtkDataArray *scalars, vtkIdType id )
{
  int keepCell = 0;
  int numComp = scalars->GetNumberOfComponents();
  int c;
  
  switch ( this->ComponentMode )
    {
    case VTK_COMPONENT_MODE_USE_SELECTED:
      c = (this->SelectedComponent < numComp)?(this->SelectedComponent):(0);
      keepCell = 
        (this->*(this->ThresholdFunction))(scalars->GetComponent(id,c));
      break;
    case VTK_COMPONENT_MODE_USE_ANY:
      keepCell = 0;
      for ( c = 0; (!keepCell) && (c < numComp); c++ )
        {
        keepCell = 
          (this->*(this->ThresholdFunction))(scalars->GetComponent(id,c));
        }
      break;
    case VTK_COMPONENT_MODE_USE_ALL:
      keepCell = 1;
      for ( c = 0; keepCell && (c < numComp); c++ )
        {
        keepCell = 
          (this->*(this->ThresholdFunction))(scalars->GetComponent(id,c));
        }
      break;
    }
  return keepCell;
}

// Return the method for manipulating scalar data as a string.
const char *vtkThreshold::GetAttributeModeAsString(void)
{
  if ( this->AttributeMode == VTK_ATTRIBUTE_MODE_DEFAULT )
    {
    return "Default";
    }
  else if ( this->AttributeMode == VTK_ATTRIBUTE_MODE_USE_POINT_DATA )
    {
    return "UsePointData";
    }
  else 
    {
    return "UseCellData";
    }
}

// Return a string representation of the component mode
const char *vtkThreshold::GetComponentModeAsString(void)
{
  if ( this->ComponentMode == VTK_COMPONENT_MODE_USE_SELECTED )
    {
    return "UseSelected";
    }
  else if ( this->ComponentMode == VTK_COMPONENT_MODE_USE_ANY )
    {
    return "UseAny";
    }
  else 
    {
    return "UseAll";
    }
}

void vtkThreshold::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Attribute Mode: " << this->GetAttributeModeAsString() << endl;
  os << indent << "Component Mode: " << this->GetComponentModeAsString() << endl;
  os << indent << "Selected Component: " << this->SelectedComponent << endl;
  
  if (this->InputScalarsSelection)
    {
    os << indent << "InputScalarsSelection: " << this->InputScalarsSelection;
    } 

  os << indent << "All Scalars: " << this->AllScalars << "\n";
  if ( this->ThresholdFunction == &vtkThreshold::Upper )
    {
    os << indent << "Threshold By Upper\n";
    }

  else if ( this->ThresholdFunction == &vtkThreshold::Lower )
    {
    os << indent << "Threshold By Lower\n";
    }

  else if ( this->ThresholdFunction == &vtkThreshold::Between )
    {
    os << indent << "Threshold Between\n";
    }

  os << indent << "Lower Threshold: " << this->LowerThreshold << "\n";
  os << indent << "Upper Threshold: " << this->UpperThreshold << "\n";
}
