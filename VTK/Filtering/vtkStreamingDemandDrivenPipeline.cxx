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
#include "vtkStreamingDemandDrivenPipeline.h"

#include "vtkAlgorithm.h"
#include "vtkAlgorithmOutput.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkExtentTranslator.h"
#include "vtkInformation.h"
#include "vtkInformationDoubleVectorKey.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationIntegerVectorKey.h"
#include "vtkInformationObjectBaseKey.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"

vtkCxxRevisionMacro(vtkStreamingDemandDrivenPipeline, "$Revision$");
vtkStandardNewMacro(vtkStreamingDemandDrivenPipeline);

vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, CONTINUE_EXECUTING, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, EXACT_EXTENT, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, REQUEST_UPDATE_EXTENT, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, MAXIMUM_NUMBER_OF_PIECES, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, UPDATE_EXTENT_INITIALIZED, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, UPDATE_PIECE_NUMBER, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, UPDATE_NUMBER_OF_PIECES, Integer);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, UPDATE_NUMBER_OF_GHOST_LEVELS, Integer);
vtkInformationKeyRestrictedMacro(vtkStreamingDemandDrivenPipeline, WHOLE_EXTENT, IntegerVector, 6);
vtkInformationKeyRestrictedMacro(vtkStreamingDemandDrivenPipeline, UPDATE_EXTENT, IntegerVector, 6);
vtkInformationKeyRestrictedMacro(vtkStreamingDemandDrivenPipeline,
                                 EXTENT_TRANSLATOR, ObjectBase,
                                 "vtkExtentTranslator");
vtkInformationKeyRestrictedMacro(vtkStreamingDemandDrivenPipeline, WHOLE_BOUNDING_BOX, DoubleVector, 6);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, TIME_STEPS, DoubleVector);
vtkInformationKeyMacro(vtkStreamingDemandDrivenPipeline, UPDATE_TIME_INDEX, Integer);

//----------------------------------------------------------------------------
class vtkStreamingDemandDrivenPipelineToDataObjectFriendship
{
public:
  static void Crop(vtkDataObject* obj)
    {
    obj->Crop();
    }
};

//----------------------------------------------------------------------------
vtkStreamingDemandDrivenPipeline::vtkStreamingDemandDrivenPipeline()
{
  this->ContinueExecuting = 0;
}

//----------------------------------------------------------------------------
vtkStreamingDemandDrivenPipeline::~vtkStreamingDemandDrivenPipeline()
{
}

//----------------------------------------------------------------------------
void vtkStreamingDemandDrivenPipeline::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::ProcessRequest(vtkInformation* request)
{
  // The algorithm should not invoke anything on the executive.
  if(!this->CheckAlgorithm("ProcessRequest"))
    {
    return 0;
    }

  // Look for specially supported requests.
  if(request->Has(REQUEST_UPDATE_EXTENT()))
    {
    // Get the output port from which the request was made.
    int outputPort = -1;
    if(request->Has(FROM_OUTPUT_PORT()))
      {
      outputPort = request->Get(FROM_OUTPUT_PORT());
      }

    // Make sure the information on the output port is valid.
    if(!this->VerifyOutputInformation(outputPort))
      {
      return 0;
      }

    // If we need to execute, propagate the update extent.
    int result = 1;
    if(this->NeedToExecuteData(outputPort))
      {
      // Make sure input types are valid before algorithm does anything.
      if(!this->InputCountIsValid() || !this->InputTypeIsValid())
        {
        return 0;
        }

      // Invoke the request on the algorithm.
      vtkSmartPointer<vtkInformation> r = vtkSmartPointer<vtkInformation>::New();
      r->Set(REQUEST_UPDATE_EXTENT(), 1);
      r->Set(FROM_OUTPUT_PORT(), outputPort);
      result = this->CallAlgorithm(r, vtkExecutive::RequestUpstream);

      // Propagate the update extent to all inputs.
      if(result)
        {
        result = this->ForwardUpstream(request);
        }
      }
    return result;
    }

  if(request->Has(REQUEST_DATA()))
    {
    // Let the superclass handle the request first.
    if(this->Superclass::ProcessRequest(request))
      {
      // Crop the output if the exact extent flag is set.
      for(int i=0; i < this->GetNumberOfOutputPorts(); ++i)
        {
        vtkInformation* info = this->GetOutputInformation(i);
        vtkDataObject* data = info->Get(vtkDataObject::DATA_OBJECT());
        if(info->Has(EXACT_EXTENT()) && info->Get(EXACT_EXTENT()))
          {
          vtkStreamingDemandDrivenPipelineToDataObjectFriendship::Crop(data);
          }
        }
      return 1;
      }
    return 0;
    }

  // Let the superclass handle other requests.
  return this->Superclass::ProcessRequest(request);
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::Update()
{
  return this->Superclass::Update();
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::Update(int port)
{
  if(!this->UpdateInformation())
    {
    return 0;
    }
  if(port >= -1 && port < this->Algorithm->GetNumberOfOutputPorts())
    {
    int retval = 1;
    // some streaming filters can request that the pipeline execute multiple
    // times for a single update
    do 
      {
      retval =  
        this->PropagateUpdateExtent(port) && this->UpdateData(port) && retval;
      }
    while (this->ContinueExecuting);
    return retval;
    }
  else
    {
    return 1;
    }
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::UpdateWholeExtent()
{
  this->UpdateInformation();
  // if we have an output then set the UE to WE for it
  if (this->Algorithm->GetNumberOfOutputPorts())
    {
    this->SetUpdateExtentToWholeExtent(0);
    }
  // otherwise do it for the inputs
  else
    {
    // Loop over all input ports.
    for(int i=0; i < this->Algorithm->GetNumberOfInputPorts(); ++i)
      {
      // Loop over all connections on this input port.
      int numInConnections = this->Algorithm->GetNumberOfInputConnections(i);
      for (int j=0; j<numInConnections; j++)
        {
        // Get the pipeline information for this input connection.
        vtkInformation* inInfo = this->GetInputInformation(i, j);
        
        // Get the executive and port number producing this input.
        vtkStreamingDemandDrivenPipeline* inExec =
          vtkStreamingDemandDrivenPipeline::SafeDownCast(
            inInfo->GetExecutive(vtkExecutive::PRODUCER()));
        int inPort = inInfo->GetPort(vtkExecutive::PRODUCER());
        if(inExec)
          {
          inExec->SetUpdateExtentToWholeExtent(inPort);
          }
        }
      }
    }
  return this->Update();
}

//----------------------------------------------------------------------------
int
vtkStreamingDemandDrivenPipeline
::ExecuteInformation(vtkInformation* request)
{
  // Let the superclass make the request to the algorithm.
  if(this->Superclass::ExecuteInformation(request))
    {
    for(int i=0; i < this->Algorithm->GetNumberOfOutputPorts(); ++i)
      {
      vtkInformation* info = this->GetOutputInformation(i);
      vtkDataObject* data = info->Get(vtkDataObject::DATA_OBJECT());

      if (!data)
        {
        return 0;
        }
      // Set default maximum request.
      if(data->GetExtentType() == VTK_PIECES_EXTENT)
        {
        if(!info->Has(MAXIMUM_NUMBER_OF_PIECES()))
          {
          info->Set(MAXIMUM_NUMBER_OF_PIECES(), -1);
          }
        }
      else if(data->GetExtentType() == VTK_3D_EXTENT)
        {
        if(!info->Has(WHOLE_EXTENT()))
          {
          int extent[6] = {0,-1,0,-1,0,-1};
          info->Set(WHOLE_EXTENT(), extent, 6);
          }
        }

      // Make sure an update request exists.
      if(!info->Has(UPDATE_EXTENT_INITIALIZED()) ||
         !info->Get(UPDATE_EXTENT_INITIALIZED()))
        {
        // Request all data by default.
        this->SetUpdateExtentToWholeExtent(i);
        }
      }
    return 1;
    }
  else
    {
    return 0;
    }
}

//----------------------------------------------------------------------------
void
vtkStreamingDemandDrivenPipeline
::CopyDefaultInformation(vtkInformation* request, int direction)
{
  // Let the superclass copy first.
  this->Superclass::CopyDefaultInformation(request, direction);

  if(request->Has(REQUEST_INFORMATION()))
    {
    if(this->GetNumberOfInputPorts() > 0)
      {
      if(vtkInformation* inInfo = this->GetInputInformation(0, 0))
        {
        // Copy information from the first input to all outputs.
        for(int i=0; i < this->GetNumberOfOutputPorts(); ++i)
          {
          vtkInformation* outInfo = this->GetOutputInformation(i);
          outInfo->CopyEntry(inInfo, WHOLE_BOUNDING_BOX());
          outInfo->CopyEntry(inInfo, WHOLE_EXTENT());
          outInfo->CopyEntry(inInfo, MAXIMUM_NUMBER_OF_PIECES());
          outInfo->CopyEntry(inInfo, EXTENT_TRANSLATOR());
          outInfo->CopyEntry(inInfo, TIME_STEPS());
          }
        }
      }

    // Setup default information for the outputs.
    for(int i=0; i < this->Algorithm->GetNumberOfOutputPorts(); ++i)
      {
      vtkInformation* outInfo = this->GetOutputInformation(i);

      // The data object will exist because UpdateDataObject has already
      // succeeded. Except when this method is called by a subclass
      // that does not provide this key in certain cases.
      vtkDataObject* dataObject = outInfo->Get(vtkDataObject::DATA_OBJECT());
      if (!dataObject)
        {
        continue;
        }
      vtkInformation* dataInfo = dataObject->GetInformation();
      if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_PIECES_EXTENT)
        {
        if (!outInfo->Has(MAXIMUM_NUMBER_OF_PIECES()))
          {
          // Since most unstructured filters in VTK generate all their
          // data once, set the default maximum number of pieces to 1.
          outInfo->Set(MAXIMUM_NUMBER_OF_PIECES(), 1);
          }
        }
      else if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_3D_EXTENT)
        {
        if(!outInfo->Has(EXTENT_TRANSLATOR()) ||
           !outInfo->Get(EXTENT_TRANSLATOR()))
          {
          // Create a default extent translator.
          vtkExtentTranslator* translator = vtkExtentTranslator::New();
          outInfo->Set(EXTENT_TRANSLATOR(), translator);
          translator->Delete();
          }
        }
      }
    }
  if(request->Has(REQUEST_UPDATE_EXTENT()))
    {
    // Get the output port from which to copy the extent.
    int outputPort = -1;
    if(request->Has(FROM_OUTPUT_PORT()))
      {
      outputPort = request->Get(FROM_OUTPUT_PORT());
      }

    // Setup default information for the inputs.
    if(this->Algorithm->GetNumberOfOutputPorts() > 0)
      {
      // Copy information from the output port that made the request.
      // Since VerifyOutputInformation has already been called we know
      // there is output information with a data object.
      vtkInformation* outInfo =
        this->GetOutputInformation((outputPort >= 0)? outputPort : 0);
      vtkDataObject* outData = outInfo->Get(vtkDataObject::DATA_OBJECT());

      // Loop over all input ports.
      for(int i=0; i < this->Algorithm->GetNumberOfInputPorts(); ++i)
        {
        // Loop over all connections on this input port.
        int numInConnections = this->Algorithm->GetNumberOfInputConnections(i);
        for (int j=0; j<numInConnections; j++)
          {
          // Get the pipeline information for this input connection.
          vtkInformation* inInfo = this->GetInputInformation(i, j);

          // Copy the time request
          if ( outInfo->Has(UPDATE_TIME_INDEX()) )
            {
            inInfo->CopyEntry(outInfo, UPDATE_TIME_INDEX());
            }

          // Get the executive and port number producing this input.
          vtkStreamingDemandDrivenPipeline* inExec =
            vtkStreamingDemandDrivenPipeline::SafeDownCast(
              inInfo->GetExecutive(vtkExecutive::PRODUCER()));
          int inPort = inInfo->GetPort(vtkExecutive::PRODUCER());
          if(!inExec)
            {
            vtkErrorMacro(
              "Cannot copy default update request from output port "
              << outputPort << " on algorithm "
              << this->Algorithm->GetClassName()
              << "(" << this->Algorithm << ") to input connection "
              << j << " on input port " << i
              << " because the input is not managed by a"
              << " vtkStreamingDemandDrivenPipeline.");
            continue;
            }

          // Get the input data object for this connection.  It should
          // have already been created by the UpdateDataObject pass.
          vtkDataObject* inData = inInfo->Get(vtkDataObject::DATA_OBJECT());
          if(!inData)
            {
            vtkErrorMacro("Cannot copy default update request from output port "
                          << outputPort << " on algorithm "
                          << this->Algorithm->GetClassName()
                          << "(" << this->Algorithm << ") to input connection "
                          << j << " on input port " << i
                          << " because there is no data object.");
            continue;
            }

          // Consider all combinations of extent types.
          if(inData->GetExtentType() == VTK_PIECES_EXTENT)
            {
            if(outData->GetExtentType() == VTK_PIECES_EXTENT)
              {
              if (outInfo->Get(UPDATE_PIECE_NUMBER()) < 0)
                {
                return;
                }
              inInfo->CopyEntry(outInfo, UPDATE_PIECE_NUMBER());
              inInfo->CopyEntry(outInfo, UPDATE_NUMBER_OF_PIECES());
              inInfo->CopyEntry(outInfo, UPDATE_NUMBER_OF_GHOST_LEVELS());
              inInfo->CopyEntry(outInfo, UPDATE_EXTENT_INITIALIZED());
              }
            else if(outData->GetExtentType() == VTK_3D_EXTENT)
              {
              // The conversion from structrued requests to
              // unstrcutured requests is always to request the whole
              // extent.
              inExec->SetUpdateExtentToWholeExtent(inPort);
              }
            }
          else if(inData->GetExtentType() == VTK_3D_EXTENT)
            {
            if(outData->GetExtentType() == VTK_PIECES_EXTENT)
              {
              int piece = outInfo->Get(UPDATE_PIECE_NUMBER());
              int numPieces = outInfo->Get(UPDATE_NUMBER_OF_PIECES());
              int ghostLevel = outInfo->Get(UPDATE_NUMBER_OF_GHOST_LEVELS());
              if (piece >= 0)
                {
                inExec->SetUpdateExtent(inPort, piece, numPieces, ghostLevel);
                }
              }
            else if(outData->GetExtentType() == VTK_3D_EXTENT)
              {
              inInfo->CopyEntry(outInfo, UPDATE_EXTENT());
              inInfo->CopyEntry(outInfo, UPDATE_EXTENT_INITIALIZED());
              }
            }
          }
        }
      }
    }
}

//----------------------------------------------------------------------------
void
vtkStreamingDemandDrivenPipeline
::ResetPipelineInformation(int port, vtkInformation* info)
{
  this->Superclass::ResetPipelineInformation(port, info);
  info->Remove(WHOLE_EXTENT());
  info->Remove(MAXIMUM_NUMBER_OF_PIECES());
  info->Remove(EXTENT_TRANSLATOR());
  info->Remove(UPDATE_EXTENT_INITIALIZED());
  info->Remove(UPDATE_EXTENT());
  info->Remove(UPDATE_PIECE_NUMBER());
  info->Remove(UPDATE_NUMBER_OF_PIECES());
  info->Remove(UPDATE_NUMBER_OF_GHOST_LEVELS());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::PropagateUpdateExtent(int outputPort)
{
  // The algorithm should not invoke anything on the executive.
  if(!this->CheckAlgorithm("PropagateUpdateExtent"))
    {
    return 0;
    }

  // Range check.
  if(outputPort < -1 ||
     outputPort >= this->Algorithm->GetNumberOfOutputPorts())
    {
    vtkErrorMacro("PropagateUpdateExtent given output port index "
                  << outputPort << " on an algorithm with "
                  << this->Algorithm->GetNumberOfOutputPorts()
                  << " output ports.");
    return 0;
    }

  // Setup the request for update extent propagation.
  vtkSmartPointer<vtkInformation> r = vtkSmartPointer<vtkInformation>::New();
  r->Set(REQUEST_UPDATE_EXTENT(), 1);
  r->Set(FROM_OUTPUT_PORT(), outputPort);

  // The request is forwarded upstream through the pipeline.
  r->Set(vtkExecutive::FORWARD_DIRECTION(), vtkExecutive::RequestUpstream);

  // Algorithms process this request before it is forwarded.
  r->Set(vtkExecutive::ALGORITHM_BEFORE_FORWARD(), 1);

  // Send the request.
  return this->ProcessRequest(r);
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::VerifyOutputInformation(int outputPort)
{
  // If no port is specified, check all ports.
  if(outputPort < 0)
    {
    for(int i=0; i < this->Algorithm->GetNumberOfOutputPorts(); ++i)
      {
      if(!this->VerifyOutputInformation(i))
        {
        return 0;
        }
      }
    return 1;
    }

  // Get the information object to check.
  vtkInformation* outInfo = this->GetOutputInformation(outputPort);

  // Make sure there is a data object.  It is supposed to be created
  // by the UpdateDataObject step.
  vtkDataObject* dataObject = outInfo->Get(vtkDataObject::DATA_OBJECT());
  if(!dataObject)
    {
    vtkErrorMacro("No data object has been set in the information for "
                  "output port " << outputPort << ".");
    return 0;
    }

  // Check extents.
  vtkInformation* dataInfo = dataObject->GetInformation();
  if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_PIECES_EXTENT)
    {
    // For an unstructured extent, make sure the update request
    // exists.  We do not need to check if it is valid because
    // out-of-range requests produce empty data.
    if(!outInfo->Has(MAXIMUM_NUMBER_OF_PIECES()))
      {
      vtkErrorMacro("No maximum number of pieces has been set in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ").");
      return 0;
      }
    if(!outInfo->Has(UPDATE_PIECE_NUMBER()))
      {
      vtkErrorMacro("No update piece number has been set in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ").");
      return 0;
      }
    if(!outInfo->Has(UPDATE_NUMBER_OF_PIECES()))
      {
      vtkErrorMacro("No update number of pieces has been set in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ").");
      return 0;
      }
    if(!outInfo->Has(UPDATE_NUMBER_OF_GHOST_LEVELS()))
      {
      // Use zero ghost levels by default.
      outInfo->Set(UPDATE_NUMBER_OF_GHOST_LEVELS(), 0);
      }
    }
  else if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_3D_EXTENT)
    {
    // For a structured extent, make sure the update request
    // exists.
    if(!outInfo->Has(WHOLE_EXTENT()))
      {
      vtkErrorMacro("No whole extent has been set in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ").");
      return 0;
      }
    if(!outInfo->Has(UPDATE_EXTENT()))
      {
      vtkErrorMacro("No update extent has been set in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ").");
      return 0;
      }
    // Make sure the update request is inside the whole extent.
    int wholeExtent[6];
    int updateExtent[6];
    outInfo->Get(WHOLE_EXTENT(), wholeExtent);
    outInfo->Get(UPDATE_EXTENT(), updateExtent);
    if((updateExtent[0] < wholeExtent[0] ||
        updateExtent[1] > wholeExtent[1] ||
        updateExtent[2] < wholeExtent[2] ||
        updateExtent[3] > wholeExtent[3] ||
        updateExtent[4] < wholeExtent[4] ||
        updateExtent[5] > wholeExtent[5]) &&
       (updateExtent[0] <= updateExtent[1] &&
        updateExtent[2] <= updateExtent[3] &&
        updateExtent[4] <= updateExtent[5]))
      {
      // Update extent is outside the whole extent and is not empty.
      vtkErrorMacro("The update extent specified in the "
                    "information for output port " << outputPort
                    << " on algorithm " << this->Algorithm->GetClassName()
                    << "(" << this->Algorithm << ") is "
                    << updateExtent[0] << " " << updateExtent[1] << " "
                    << updateExtent[2] << " " << updateExtent[3] << " "
                    << updateExtent[4] << " " << updateExtent[5]
                    << ", which is outside the whole extent "
                    << wholeExtent[0] << " " << wholeExtent[1] << " "
                    << wholeExtent[2] << " " << wholeExtent[3] << " "
                    << wholeExtent[4] << " " << wholeExtent[5] << ".");
      return 0;
      }
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::ExecuteData(vtkInformation* request)
{
  // Preserve the execution continuation flag in the request across
  // iterations of the algorithm.
  if(this->ContinueExecuting)
    {
    request->Set(CONTINUE_EXECUTING(), 1);
    }
  else
    {
    request->Remove(CONTINUE_EXECUTING());
    }

  // Let the superclass execute the filter.
  int result = this->Superclass::ExecuteData(request);

  // Preserve the execution continuation flag in the request across
  // iterations of the algorithm.
  if(request->Get(CONTINUE_EXECUTING()))
    {
    this->ContinueExecuting = 1;
    }
  else
    {
    this->ContinueExecuting = 0;
    }

  return result;
}

//----------------------------------------------------------------------------
void
vtkStreamingDemandDrivenPipeline::ExecuteDataStart(vtkInformation* request)
{
  // Perform start operations only if not in an execute continuation.
  if(!this->ContinueExecuting)
    {
    this->Superclass::ExecuteDataStart(request);
    }
}

//----------------------------------------------------------------------------
void
vtkStreamingDemandDrivenPipeline::ExecuteDataEnd(vtkInformation* request)
{
  // Perform end operations only if not in an execute continuation.
  if(!this->ContinueExecuting)
    {
    this->Superclass::ExecuteDataEnd(request);
    }
}

//----------------------------------------------------------------------------
void
vtkStreamingDemandDrivenPipeline::MarkOutputsGenerated(vtkInformation* request)
{
  // Tell outputs they have been generated.
  this->Superclass::MarkOutputsGenerated(request);

  // Compute ghost level arrays for generated outputs.
  vtkInformationVector* outputs = this->GetOutputInformation();
  for(int i=0; i < outputs->GetNumberOfInformationObjects(); ++i)
    {
    vtkInformation* outInfo = outputs->GetInformationObject(i);
    vtkDataObject* data = outInfo->Get(vtkDataObject::DATA_OBJECT());
    if(data && !outInfo->Get(DATA_NOT_GENERATED()))
      {
      if(vtkDataSet* ds = vtkDataSet::SafeDownCast(data))
        {
        ds->GenerateGhostLevelArray();
        }
      }
    }
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::NeedToExecuteData(int outputPort)
{
  // Has the algorithm asked to be executed again?
  if(this->ContinueExecuting)
    {
    return 1;
    }

  // If no port is specified, check all ports.  This behavior is
  // implemented by the superclass.
  if(outputPort < 0)
    {
    return this->Superclass::NeedToExecuteData(outputPort);
    }

  // Does the superclass want to execute?
  if(this->Superclass::NeedToExecuteData(outputPort))
    {
    return 1;
    }

  // We need to check the requested update extent.  Get the output
  // port information and data information.  We do not need to check
  // existence of values because it has already been verified by
  // VerifyOutputInformation.
  vtkInformation* outInfo = this->GetOutputInformation(outputPort);
  vtkDataObject* dataObject = outInfo->Get(vtkDataObject::DATA_OBJECT());
  vtkInformation* dataInfo = dataObject->GetInformation();
  if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_PIECES_EXTENT)
    {
    // Check the unstructured extent.  If we do not have the requested
    // piece, we need to execute.
    int dataPiece = dataInfo->Get(vtkDataObject::DATA_PIECE_NUMBER());
    int dataNumberOfPieces = dataInfo->Get(vtkDataObject::DATA_NUMBER_OF_PIECES());
    int dataGhostLevel = dataInfo->Get(vtkDataObject::DATA_NUMBER_OF_GHOST_LEVELS());
    int updatePiece = outInfo->Get(UPDATE_PIECE_NUMBER());
    int updateNumberOfPieces = outInfo->Get(UPDATE_NUMBER_OF_PIECES());
    int updateGhostLevel = outInfo->Get(UPDATE_NUMBER_OF_GHOST_LEVELS());
    if(dataPiece != updatePiece ||
       dataNumberOfPieces != updateNumberOfPieces ||
       dataGhostLevel != updateGhostLevel)
      {
      return 1;
      }
    }
  else if(dataInfo->Get(vtkDataObject::DATA_EXTENT_TYPE()) == VTK_3D_EXTENT)
    {
    // Check the structured extent.  If the update extent is outside
    // of the extent and not empty, we need to execute.
    int dataExtent[6];
    int updateExtent[6];
    outInfo->Get(UPDATE_EXTENT(), updateExtent);
    dataInfo->Get(vtkDataObject::DATA_EXTENT(), dataExtent);
    // if the ue is out side the de
    if((updateExtent[0] < dataExtent[0] ||
        updateExtent[1] > dataExtent[1] ||
        updateExtent[2] < dataExtent[2] ||
        updateExtent[3] > dataExtent[3] ||
        updateExtent[4] < dataExtent[4] ||
        updateExtent[5] > dataExtent[5]) &&
       // and the ue is set
       (updateExtent[0] <= updateExtent[1] &&
        updateExtent[2] <= updateExtent[3] &&
        updateExtent[4] <= updateExtent[5]))
      {
      return 1;
      }
    }

  // We do not need to execute.
  return 0;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetMaximumNumberOfPieces(int port, int n)
{
  if(!this->OutputPortIndexInRange(port, "set maximum number of pieces on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(this->GetMaximumNumberOfPieces(port) != n)
    {
    info->Set(MAXIMUM_NUMBER_OF_PIECES(), n);
    return 1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::GetMaximumNumberOfPieces(int port)
{
  if(!this->OutputPortIndexInRange(port, "get maximum number of pieces from"))
    {
    return -1;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(MAXIMUM_NUMBER_OF_PIECES()))
    {
    info->Set(MAXIMUM_NUMBER_OF_PIECES(), -1);
    }
  return info->Get(MAXIMUM_NUMBER_OF_PIECES());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetWholeExtent(int port, int extent[6])
{
  if(!this->OutputPortIndexInRange(port, "set whole extent on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  int oldExtent[6];
  this->GetWholeExtent(port, oldExtent);
  if(oldExtent[0] != extent[0] || oldExtent[1] != extent[1] ||
     oldExtent[2] != extent[2] || oldExtent[3] != extent[3] ||
     oldExtent[4] != extent[4] || oldExtent[5] != extent[5])
    {
    modified = 1;
    info->Set(WHOLE_EXTENT(), extent, 6);
    }
  return modified;
}

//----------------------------------------------------------------------------
void vtkStreamingDemandDrivenPipeline::GetWholeExtent(int port, int extent[6])
{
  static int emptyExtent[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get whole extent from"))
    {
    memcpy(extent, emptyExtent, sizeof(int)*6);
    return;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(WHOLE_EXTENT()))
    {
    info->Set(WHOLE_EXTENT(), emptyExtent, 6);
    }
  info->Get(WHOLE_EXTENT(), extent);
}

//----------------------------------------------------------------------------
int* vtkStreamingDemandDrivenPipeline::GetWholeExtent(int port)
{
  static int emptyExtent[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get whole extent from"))
    {
    return emptyExtent;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(WHOLE_EXTENT()))
    {
    info->Set(WHOLE_EXTENT(), emptyExtent, 6);
    }
  return info->Get(WHOLE_EXTENT());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdateExtentToWholeExtent(int port)
{
  if(!this->OutputPortIndexInRange(port, "set update extent to whole extent on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);

  // Request all data.
  int modified = 0;
  if(vtkDataObject* data = info->Get(vtkDataObject::DATA_OBJECT()))
    {
    if(data->GetExtentType() == VTK_PIECES_EXTENT)
      {
      modified |= this->SetUpdatePiece(port, 0);
      modified |= this->SetUpdateNumberOfPieces(port, 1);
      modified |= this->SetUpdateGhostLevel(port, 0);
      }
    else if(data->GetExtentType() == VTK_3D_EXTENT)
      {
      int extent[6] = {0,-1,0,-1,0,-1};
      info->Get(WHOLE_EXTENT(), extent);
      modified |= this->SetUpdateExtent(port, extent);
      }
    }
  else
    {
    vtkErrorMacro("SetUpdateExtentToWholeExtent called with no data object on port "
                  << port << ".");
    }

  // Make sure the update extent will remain the whole extent until
  // the update extent is explicitly set by the caller.
  info->Set(UPDATE_EXTENT_INITIALIZED(), 0);

  return modified;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdateExtent(int port, int extent[6])
{
  if(!this->OutputPortIndexInRange(port, "set update extent on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  int oldExtent[6];
  this->GetUpdateExtent(port, oldExtent);
  if(oldExtent[0] != extent[0] || oldExtent[1] != extent[1] ||
     oldExtent[2] != extent[2] || oldExtent[3] != extent[3] ||
     oldExtent[4] != extent[4] || oldExtent[5] != extent[5])
    {
    modified = 1;
    info->Set(UPDATE_EXTENT(), extent, 6);
    }
  info->Set(UPDATE_EXTENT_INITIALIZED(), 1);
  return modified;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdateExtent(int port, int piece,
                                                      int numPieces,
                                                      int ghostLevel)
{
  if(!this->OutputPortIndexInRange(port, "set unstructured update extent on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  modified |= this->SetUpdatePiece(port, piece);
  modified |= this->SetUpdateNumberOfPieces(port, numPieces);
  modified |= this->SetUpdateGhostLevel(port, ghostLevel);
  if(vtkDataObject* data = info->Get(vtkDataObject::DATA_OBJECT()))
    {
    if(data->GetExtentType() == VTK_3D_EXTENT)
      {
      if(vtkExtentTranslator* translator = this->GetExtentTranslator(port))
        {
        int wholeExtent[6];
        this->GetWholeExtent(port, wholeExtent);
        translator->SetWholeExtent(wholeExtent);
        translator->SetPiece(piece);
        translator->SetNumberOfPieces(numPieces);
        translator->SetGhostLevel(ghostLevel);
        translator->PieceToExtent();
        modified |= this->SetUpdateExtent(port, translator->GetExtent());
        }
      else
        {
        vtkErrorMacro("Cannot translate unstructured extent to structured "
                      "for output port " << port << " of algorithm "
                      << this->Algorithm->GetClassName() << "("
                      << this->Algorithm << ").");
        }
      }
    }
  return modified;
}

//----------------------------------------------------------------------------
void vtkStreamingDemandDrivenPipeline::GetUpdateExtent(int port, int extent[6])
{
  static int emptyExtent[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get update extent from"))
    {
    memcpy(extent, emptyExtent, sizeof(int)*6);
    return;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(UPDATE_EXTENT()))
    {
    info->Set(UPDATE_EXTENT(), emptyExtent, 6);
    info->Set(UPDATE_EXTENT_INITIALIZED(), 0);
    }
  info->Get(UPDATE_EXTENT(), extent);
}

//----------------------------------------------------------------------------
int* vtkStreamingDemandDrivenPipeline::GetUpdateExtent(int port)
{
  static int emptyExtent[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get update extent from"))
    {
    return emptyExtent;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(UPDATE_EXTENT()))
    {
    info->Set(UPDATE_EXTENT(), emptyExtent, 6);
    info->Set(UPDATE_EXTENT_INITIALIZED(), 0);
    }
  return info->Get(UPDATE_EXTENT());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdatePiece(int port, int piece)
{
  if(!this->OutputPortIndexInRange(port, "set update piece on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  if(this->GetUpdatePiece(port) != piece)
    {
    info->Set(UPDATE_PIECE_NUMBER(), piece);
    modified = 1;
    }
  info->Set(UPDATE_EXTENT_INITIALIZED(), 1);
  return modified;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::GetUpdatePiece(int port)
{
  if(!this->OutputPortIndexInRange(port, "get update piece from"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(UPDATE_PIECE_NUMBER()))
    {
    info->Set(UPDATE_PIECE_NUMBER(), 0);
    }
  return info->Get(UPDATE_PIECE_NUMBER());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdateNumberOfPieces(int port, int n)
{
  if(!this->OutputPortIndexInRange(port, "set update numer of pieces on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  if(this->GetUpdateNumberOfPieces(port) != n)
    {
    info->Set(UPDATE_NUMBER_OF_PIECES(), n);
    modified = 1;
    }
  info->Set(UPDATE_EXTENT_INITIALIZED(), 1);
  return modified;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::GetUpdateNumberOfPieces(int port)
{
  if(!this->OutputPortIndexInRange(port, "get update number of pieces from"))
    {
    return 1;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(UPDATE_NUMBER_OF_PIECES()))
    {
    info->Set(UPDATE_NUMBER_OF_PIECES(), 1);
    }
  return info->Get(UPDATE_NUMBER_OF_PIECES());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetUpdateGhostLevel(int port, int n)
{
  if(!this->OutputPortIndexInRange(port, "set update numer of ghost levels on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(this->GetUpdateGhostLevel(port) != n)
    {
    info->Set(UPDATE_NUMBER_OF_GHOST_LEVELS(), n);
    return 1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::GetUpdateGhostLevel(int port)
{
  if(!this->OutputPortIndexInRange(port, "get update number of ghost levels from"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(UPDATE_NUMBER_OF_GHOST_LEVELS()))
    {
    info->Set(UPDATE_NUMBER_OF_GHOST_LEVELS(), 0);
    }
  return info->Get(UPDATE_NUMBER_OF_GHOST_LEVELS());
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetRequestExactExtent(int port, int flag)
{
  if(!this->OutputPortIndexInRange(port, "set request exact extent flag on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(this->GetRequestExactExtent(port) != flag)
    {
    info->Set(EXACT_EXTENT(), flag);
    return 1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::GetRequestExactExtent(int port)
{
  if(!this->OutputPortIndexInRange(port, "get request exact extent flag from"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(EXACT_EXTENT()))
    {
    info->Set(EXACT_EXTENT(), 0);
    }
  return info->Get(EXACT_EXTENT());
}

//----------------------------------------------------------------------------
int
vtkStreamingDemandDrivenPipeline
::SetExtentTranslator(int port, vtkExtentTranslator* translator)
{
  if(!this->OutputPortIndexInRange(port, "set extent translator on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  vtkExtentTranslator* oldTranslator =
    vtkExtentTranslator::SafeDownCast(info->Get(EXTENT_TRANSLATOR()));
  if(translator != oldTranslator)
    {
    info->Set(EXTENT_TRANSLATOR(), translator);
    return 1;
    }
  return 0;
}

//----------------------------------------------------------------------------
vtkExtentTranslator*
vtkStreamingDemandDrivenPipeline::GetExtentTranslator(int port)
{
  if(!this->OutputPortIndexInRange(port, "get extent translator from"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  vtkExtentTranslator* translator =
    vtkExtentTranslator::SafeDownCast(info->Get(EXTENT_TRANSLATOR()));
  if(!translator)
    {
    translator = vtkExtentTranslator::New();
    info->Set(EXTENT_TRANSLATOR(), translator);
    translator->Delete();
    }
  return translator;
}

//----------------------------------------------------------------------------
int vtkStreamingDemandDrivenPipeline::SetWholeBoundingBox(int port, 
                                                          double extent[6])
{
  if(!this->OutputPortIndexInRange(port, "set whole bounding box on"))
    {
    return 0;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  int modified = 0;
  double oldBoundingBox[6];
  this->GetWholeBoundingBox(port, oldBoundingBox);
  if(oldBoundingBox[0] != extent[0] || oldBoundingBox[1] != extent[1] ||
     oldBoundingBox[2] != extent[2] || oldBoundingBox[3] != extent[3] ||
     oldBoundingBox[4] != extent[4] || oldBoundingBox[5] != extent[5])
    {
    modified = 1;
    info->Set(WHOLE_BOUNDING_BOX(), extent, 6);
    }
  return modified;
}

//----------------------------------------------------------------------------
void vtkStreamingDemandDrivenPipeline::GetWholeBoundingBox(int port, double extent[6])
{
  static double emptyBoundingBox[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get whole bounding box from"))
    {
    memcpy(extent, emptyBoundingBox, sizeof(double)*6);
    return;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(WHOLE_BOUNDING_BOX()))
    {
    info->Set(WHOLE_BOUNDING_BOX(), emptyBoundingBox, 6);
    }
  info->Get(WHOLE_BOUNDING_BOX(), extent);
}

//----------------------------------------------------------------------------
double* vtkStreamingDemandDrivenPipeline::GetWholeBoundingBox(int port)
{
  static double emptyBoundingBox[6] = {0,-1,0,-1,0,-1};
  if(!this->OutputPortIndexInRange(port, "get whole bounding box from"))
    {
    return emptyBoundingBox;
    }
  vtkInformation* info = this->GetOutputInformation(port);
  if(!info->Has(WHOLE_BOUNDING_BOX()))
    {
    info->Set(WHOLE_BOUNDING_BOX(), emptyBoundingBox, 6);
    }
  return info->Get(WHOLE_BOUNDING_BOX());
}
