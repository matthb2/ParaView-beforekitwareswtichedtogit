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
#include "vtkSMPart.h"

#include "vtkClientServerStream.h"
#include "vtkObjectFactory.h"
#include "vtkPVClassNameInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkProcessModule.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSMPart);
vtkCxxRevisionMacro(vtkSMPart, "$Revision$");


//----------------------------------------------------------------------------
vtkSMPart::vtkSMPart()
{
  this->SetVTKClassName("vtkDataObject");

  this->DataInformation = vtkPVDataInformation::New();
  this->DataInformationValid = 0;

  this->ClassNameInformation = vtkPVClassNameInformation::New();
}

//----------------------------------------------------------------------------
vtkSMPart::~vtkSMPart()
{  
  this->DataInformation->Delete();
  this->ClassNameInformation->Delete();
}

//----------------------------------------------------------------------------
vtkPVDataInformation* vtkSMPart::GetDataInformation()
{
  if (this->DataInformationValid == 0)
    {
    this->GatherDataInformation();
    }
  return this->DataInformation;
}

//----------------------------------------------------------------------------
void vtkSMPart::InvalidateDataInformation()
{
  this->DataInformationValid = 0;
}

//----------------------------------------------------------------------------
void vtkSMPart::GatherDataInformation()
{
  if (this->GetNumberOfIDs() < 1)
    {
    vtkErrorMacro("Part has no associated object, can not gather info.");
    return;
    }

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  pm->GatherInformation(this->DataInformation, this->GetID(0));

  this->DataInformationValid = 1;
}

//----------------------------------------------------------------------------
void vtkSMPart::InsertExtractPiecesIfNecessary()
{
  if (this->GetNumberOfIDs() < 1)
    {
    return;
    }
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  
  // We are going to create the piece filter with a dummy tcl name,
  // setup the pipeline, and remove tcl's reference to the objects.
  // The vtkData object will be moved to the output of the piece filter.
  pm->GatherInformation(this->ClassNameInformation, this->GetID(0));
  char *className = this->ClassNameInformation->GetVTKClassName();
  vtkClientServerStream stream;
  vtkClientServerID tempDataPiece = {0};
  if (className == NULL)
    {
    vtkErrorMacro("Missing data information.");
    return;
    }
  if (!strcmp(className, "vtkPolyData"))
    {
    // Don't add anything if we are only using one processes.
    // Image can still benifit from its cache so this check
    // is specific for unstructured data.
    if (pm->GetNumberOfPartitions() == 1)
      {
      return;
      }  
    stream << vtkClientServerStream::Invoke 
                    << this->GetID(0) << "UpdateInformation"
                    << vtkClientServerStream::End;
    pm->SendStream(vtkProcessModule::DATA_SERVER, stream, 1);
    stream << vtkClientServerStream::Invoke 
                    << this->GetID(0) << "GetMaximumNumberOfPieces"
                    << vtkClientServerStream::End;
    pm->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream, 1);
    int num =0;
    pm->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0,0,&num);
    if (num != 1)
      { // The source can already produce pieces.
      return;
      }
    // Transmit is more efficient, but has the possiblity of hanging.
    // It will hang if all procs do not  call execute.
    if (getenv("PV_LOCK_SAFE") != NULL)
      {
      tempDataPiece = pm->NewStreamObject("vtkExtractPolyDataPiece", stream);
      }
    else
      {
      tempDataPiece = pm->NewStreamObject("vtkTransmitPolyDataPiece", stream);
// TODO: Add observers. Move logging to process object
//       vtkClientServerStream start;
//       start << vtkClientServerStream::Invoke << pm->GetApplicationID() 
//             << "LogStartEvent" << "Execute TransmitPData" 
//             << vtkClientServerStream::End;
//       stream << vtkClientServerStream::Invoke << tempDataPiece 
//                       << "AddObserver"
//                       << "StartEvent"
//                       << start
//                       << vtkClientServerStream::End;
//       vtkClientServerStream end;
//       end << vtkClientServerStream::Invoke << pm->GetApplicationID() 
//           << "LogEndEvent" << "Execute TransmitPData" 
//           << vtkClientServerStream::End;
//       stream << vtkClientServerStream::Invoke << tempDataPiece 
//                       << "AddObserver"
//                       << "EndEvent"
//                       << end
//                       << vtkClientServerStream::End;
      }
    }
  else if (!strcmp(className, "vtkUnstructuredGrid"))
    {
    // Don't add anything if we are only using one processes.
    // Image can still benifit from its cache so this check
    // is specific for unstructured data.
    if (pm->GetNumberOfPartitions() == 1)
      {
      return;
      }
    stream << vtkClientServerStream::Invoke 
                    << this->GetID(0) << "UpdateInformation"
                    << vtkClientServerStream::End;
    pm->SendStream(vtkProcessModule::DATA_SERVER, stream, 1);
    stream << vtkClientServerStream::Invoke 
                    << this->GetID(0) << "GetMaximumNumberOfPieces"
                    << vtkClientServerStream::End;
    pm->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream, 1);
    int num =0;
    pm->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0,0,&num);
    if (num != 1)
      { // The source can already produce pieces.
      return;
      }

    // Transmit is more efficient, but has the possiblity of hanging.
    // It will hang if all procs do not  call execute.
    if (getenv("PV_LOCK_SAFE") != NULL)
      { 
      tempDataPiece = pm->NewStreamObject(
        "vtkExtractUnstructuredGridPiece", stream);
      }
    else
      {
      tempDataPiece = pm->NewStreamObject(
        "vtkTransmitUnstructuredGridPiece", stream);
// TODO: Add observers. Move logging to process object
//       vtkClientServerStream start;
//       start << vtkClientServerStream::Invoke << pm->GetApplicationID() 
//             << "LogStartEvent" << "Execute TransmitPData" 
//             << vtkClientServerStream::End;
//       stream << vtkClientServerStream::Invoke << tempDataPiece 
//                       << "AddObserver"
//                       << "StartEvent"
//                       << start
//                       << vtkClientServerStream::End;
//       vtkClientServerStream end;
//       end << vtkClientServerStream::Invoke << pm->GetApplicationID() 
//           << "LogEndEvent" << "Execute TransmitPData" 
//           << vtkClientServerStream::End;
//       stream << vtkClientServerStream::Invoke << tempDataPiece 
//                       << "AddObserver"
//                       << "EndEvent"
//                       << end
//                       << vtkClientServerStream::End;
      }
    }
  else if (!strcmp(className, "vtkImageData"))
    {
    if (getenv("PV_LOCK_SAFE") == NULL)
      { 
      tempDataPiece = pm->NewStreamObject(
        "vtkStructuredCacheFilter", stream);
      }
    }
  else if (!strcmp(className, "vtkStructuredGrid"))
    {
    if (getenv("PV_LOCK_SAFE") == NULL)
      {
      tempDataPiece = pm->NewStreamObject(
        "vtkStructuredCacheFilter", stream);
      }
    }
  else if (!strcmp(className, "vtkRectilinearGrid"))
    {
    if (getenv("PV_LOCK_SAFE") == NULL)
      {
      tempDataPiece = pm->NewStreamObject(
        "vtkStructuredCacheFilter", stream);
      }
    }

  // If no filter is to be inserted, just return.
  if(tempDataPiece.ID == 0)
    {
    return;
    }

  // Connect the filter to the pipeline.  
  stream << vtkClientServerStream::Invoke << tempDataPiece 
                  << "SetInput"
                  << this->GetID(0)
                  << vtkClientServerStream::End;
  stream << vtkClientServerStream::Invoke << tempDataPiece 
                  << "GetOutput"
                  << vtkClientServerStream::End;
  this->SetID(0, pm->GetUniqueID());
  stream << vtkClientServerStream::Assign << this->GetID(0)
                  << vtkClientServerStream::LastResult
                  << vtkClientServerStream::End;
  pm->DeleteStreamObject(tempDataPiece, stream);
  pm->SendStream(vtkProcessModule::DATA_SERVER, stream, 0);
}

//----------------------------------------------------------------------------
// Create the extent translator (sources with no inputs only).
// Needs to be before "ExtractPieces" because translator propagates.
void vtkSMPart::CreateTranslatorIfNecessary()
{
  if (this->GetNumberOfIDs() < 1)
    {
    return;
    }
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  
  // We are going to create the piece filter with a dummy tcl name,
  // setup the pipeline, and remove tcl's reference to the objects.
  // The vtkData object will be moved to the output of the piece filter.
  pm->GatherInformation(this->ClassNameInformation, this->GetID(0));
  char *className = this->ClassNameInformation->GetVTKClassName();
  if (className == NULL)
    {
    vtkErrorMacro("Missing data information.");
    return;
    }
  vtkClientServerStream stream;
  if (strcmp(className, "vtkImageData") == 0 ||
      strcmp(className, "vtkStructuredPoints") == 0 ||
      strcmp(className, "vtkStructuredGrid") == 0 ||
      strcmp(className, "vtkRectilinearGrid") == 0 )
    {
    // Do not overwrite custom extent translators.
    // PVExtent translator should really be the default,
    // Then we would not need to do this.
    stream << vtkClientServerStream::Invoke
           << this->GetID(0) << "GetExtentTranslator"
           << vtkClientServerStream::End
           << vtkClientServerStream::Invoke
           << vtkClientServerStream::LastResult
           << "GetClassName"
           << vtkClientServerStream::End;
    pm->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream, 1);
    char* classname = 0;
    if(!pm->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0,0,&classname))
      {
      vtkErrorMacro(<< "Faild to get server result.");
      }
    if(classname && strcmp(classname, "vtkExtentTranslator") == 0)
      {
      vtkClientServerID translatorID =
        pm->NewStreamObject("vtkPVExtentTranslator", stream);
      stream << vtkClientServerStream::Invoke
             << this->GetID(0) << "SetExtentTranslator"
             << translatorID
             << vtkClientServerStream::End;
      // Translator has to be set on source because it is propagated.
      stream << vtkClientServerStream::Invoke
             << translatorID << "SetOriginalSource"
             << this->GetID(0)
             << vtkClientServerStream::End;
      pm->DeleteStreamObject(translatorID, stream);
      pm->SendStream(vtkProcessModule::DATA_SERVER, stream, 0);
      }
   }

}

//----------------------------------------------------------------------------
void vtkSMPart::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
