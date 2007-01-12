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
#include "vtkMinMax.h"
#include "vtkObjectFactory.h"

#include "vtkDataSet.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkFieldData.h"
#include "vtkCellData.h"
#include "vtkPointData.h"
#include "vtkAbstractArray.h"
#include "vtkDataArray.h"
#include "vtkPoints.h"
#include "vtkCellArray.h"

#include "vtkMultiProcessController.h"

vtkStandardNewMacro(vtkMinMax);
vtkCxxRevisionMacro(vtkMinMax, "$Revision$");

template <class T>
void vtkMinMaxExecute(
  vtkMinMax *self,int numComp,int compIdx,T* idata,T* odata
  );

//-----------------------------------------------------------------------------
vtkMinMax::vtkMinMax()
{
  this->Operation = vtkMinMax::MIN;
  this->CFirstPass = NULL;
  this->PFirstPass = NULL;
  this->FirstPasses = NULL;
  this->MismatchOccurred = 0;
}

//-----------------------------------------------------------------------------
vtkMinMax::~vtkMinMax()
{
  if (this->CFirstPass)
    {
    delete[] this->CFirstPass;
    }
  if (this->PFirstPass)
    {
    delete[] this->PFirstPass;
    }
}

//----------------------------------------------------------------------------
int vtkMinMax::FillInputPortInformation(int port, vtkInformation *info)
{
  if(!this->Superclass::FillInputPortInformation(port, info))
    {
    return 0;
    }
  if(port==0)
    {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(),1);
    }
  return 1;
}

//-----------------------------------------------------------------------------
int vtkMinMax::RequestData(vtkInformation* reqInfo,
                             vtkInformationVector** inputVector,
                             vtkInformationVector* outputVector)
{
  int numInputs;
  int idx, numArrays;

  //get hold of input, output
  vtkPolyData* output = vtkPolyData::SafeDownCast(
    outputVector->GetInformationObject(0)->Get(
      vtkDataObject::DATA_OBJECT()));

  vtkDataSet* input0 = vtkDataSet::SafeDownCast(
    inputVector[0]->GetInformationObject(0)->Get(
        vtkDataObject::DATA_OBJECT()));

  //make output arrays of same type and width as input, but make them just one 
  //element long
  vtkFieldData *icd = input0->GetCellData();
  vtkFieldData *ocd = output->GetCellData();
  ocd->CopyStructure(icd);
  numArrays = icd->GetNumberOfArrays();
  for (idx = 0; idx < numArrays; idx++)
    {
    ocd->GetArray(idx)->SetNumberOfTuples(1);
    }
  vtkFieldData *ipd = input0->GetPointData();
  vtkFieldData *opd = output->GetPointData();
  opd->CopyStructure(ipd);
  numArrays = ipd->GetNumberOfArrays();
  for (idx = 0; idx < numArrays; idx++)
    {
    opd->GetArray(idx)->SetNumberOfTuples(1);
    }

  //initialize first pass flags for the cell data
  int numComp;
  numComp = ocd->GetNumberOfComponents();
  if (this->CFirstPass)
    {
    delete[] this->CFirstPass;
    }
  this->CFirstPass = new char[numComp];
  for (idx = 0; idx < numComp; idx++)
    {
    this->CFirstPass[idx] = 1;
    }
  numComp = ipd->GetNumberOfComponents();
  if (this->PFirstPass)
    {
    delete[] this->PFirstPass;
    }
  this->PFirstPass = new char[numComp];
  for (idx = 0; idx < numComp; idx++)
    {
    this->PFirstPass[idx] = 1;
    }

  
  //make output 1 point and cell in the output as placeholders for the results
  vtkPoints *points = vtkPoints::New();
  points->InsertNextPoint(0.0, 0.0, 0.0);
  output->SetPoints(points);
  points->Delete();

  vtkCellArray *cells = vtkCellArray::New();
  vtkIdType ptId = 0;
  cells->InsertNextCell(1, &ptId);
  output->SetVerts(cells);
  cells->Delete();

  //keep a flag in case someone cares about data not lining up exactly
  this->MismatchOccurred = 0;

  //go through each input and perform the operation on all of its data
  //we accumulate the results into the output arrays, so there is no need for
  //a second pass over the per input results
  numInputs = this->GetNumberOfInputConnections(0);
  vtkInformation *inInfo;
  vtkDataSet *inputN;
  for (idx = 0; idx < numInputs; ++idx)
    {
    inInfo = inputVector[0]->GetInformationObject(idx);
    inputN = vtkDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));

    //set first pass flags to point to cell data 
    this->ComponentIdx = 0;
    this->FlagsForCells();
    //operate on the cell data
    this->OperateOnField(inputN->GetCellData(), ocd);

    //ditto for point data
    this->ComponentIdx = 0;
    this->FlagsForPoints();
    this->OperateOnField(inputN->GetPointData(), opd);      
    }

  return 1;
}

//-----------------------------------------------------------------------------
void vtkMinMax::FlagsForPoints()
{
  this->FirstPasses = this->PFirstPass;
}

//-----------------------------------------------------------------------------
void vtkMinMax::FlagsForCells()
{
  this->FirstPasses = this->CFirstPass;
}

//-----------------------------------------------------------------------------
void vtkMinMax::OperateOnField(vtkFieldData *ifd, vtkFieldData *ofd)
{
  this->GhostLevels = vtkUnsignedCharArray::SafeDownCast(
    ifd->GetArray("vtkGhostLevels"));

  int numArrays = ofd->GetNumberOfArrays();
  for (int idx = 0; idx < numArrays; idx++)
    {
    vtkAbstractArray *ia = ifd->GetArray(idx);
    vtkAbstractArray *oa = ofd->GetArray(idx);

    //type check
    //oa will not be null since we are iterating over ifd
    //input and output numtuples don't need to match (out will always be 1)
    if (ia == NULL ||
        ia->GetDataType() != oa->GetDataType() ||
        ia->GetNumberOfComponents() != oa->GetNumberOfComponents() ||
        (strcmp(ia->GetName(), oa->GetName()) != 0)
      )
      {
      //a mismatch between arrays, ignore this input array
      this->MismatchOccurred = 1;
      //if mismatches make and entire field in the output invalid
      //the firstpasses bitsets will show if a entire output value is invalid
      }
    else
      {
      //operate on all of the elements of this array
      this->OperateOnArray(ia, oa);
      }

    //update first pass flag index to move to the next array
    this->ComponentIdx += oa->GetNumberOfComponents();
    }
}

//-----------------------------------------------------------------------------
void vtkMinMax::OperateOnArray(vtkAbstractArray *ia, vtkAbstractArray *oa)
{
  vtkIdType numTuples = ia->GetNumberOfTuples();
  int numComp = ia->GetNumberOfComponents();
  int datatype = ia->GetDataType();

  this->Name = ia->GetName();      

  //go over each tuple
  for (vtkIdType idx = 0; idx < numTuples; idx++)
    {
    this->Idx = idx;

    if (
      (this->GhostLevels != NULL) &&
      (this->GhostLevels->GetValue(idx)>0)
      )
      {
      //skip cell and point attributes that don't belong to me
      continue;
      }

    //get type agnostic access to the tuple
    void *idata = ia->GetVoidPointer(idx*numComp);
    void *odata = oa->GetVoidPointer(0);

    //go over each component in the tuple (jdx)
    // perform odata[jdx] = operation(idata[jdx],odata[jdx])
    switch (datatype)
      {
      vtkTemplateMacro(
        vtkMinMaxExecute(this, numComp, this->ComponentIdx, 
                         static_cast<VTK_TT *>(idata),
                         static_cast<VTK_TT *>(odata)
          ));

      //if you can make an operator for things like strings etc,
      //put the cases for those strings here

      default:
        vtkErrorMacro(<< "Unknown data type refusing to operate on this array" );
        this->MismatchOccurred = 1;
      }
    }
}

//-----------------------------------------------------------------------------
// This templated function performs the operation on any type of data.
template <class T>
void vtkMinMaxExecute(vtkMinMax *self,
                        int numComp,
                        int compIdx,
                        T* idata,
                        T* odata
                        )
{

  //go over each component of the tuple
  for (int jdx = 0; jdx < numComp; jdx++)
    {
      
    T *ivalue = idata + jdx;
    T *ovalue = odata + jdx;

    char *FirstPasses = self->GetFirstPasses();
    if (FirstPasses[compIdx+jdx])
      {
      FirstPasses[compIdx+jdx] = 0;
      *ovalue = *ivalue;
      }
    
    switch (self->GetOperation()) 
      {
      case vtkMinMax::MIN: 
      {
      if (*ivalue < *ovalue) *ovalue = *ivalue;
      break;
      }
      case vtkMinMax::MAX:
      {
      if (*ivalue > *ovalue) *ovalue = *ivalue;
      break;
      }
      default:
      {
      *ovalue = *ivalue;
      break;
      }
      }    

    }
}

//-----------------------------------------------------------------------------
void vtkMinMax::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Operation: " << this->Operation << endl;

}
