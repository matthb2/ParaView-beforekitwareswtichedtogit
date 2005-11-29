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
#include "vtkPVProcessModule.h"

#include "vtkCallbackCommand.h"
#include "vtkCharArray.h"
#include "vtkDataSet.h"
#include "vtkDoubleArray.h"
#include "vtkDummyController.h"
#include "vtkFloatArray.h"
#include "vtkLongArray.h"
#include "vtkMapper.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVConfig.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkSource.h"
#include "vtkStringList.h"
#include "vtkToolkits.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkClientServerStream.h"
#include "vtkClientServerInterpreter.h"
#include "vtkTimerLog.h"
#include "vtkProcessModuleGUIHelper.h"
#include "vtkPVServerInformation.h"
#include "vtkInstantiator.h"
#include "vtkPVServerOptions.h"
#include "vtkKWProcessStatistics.h"
#include "vtkPVPaths.h"

#include "vtkStdString.h"

#include <vtksys/SystemTools.hxx>
#include <vtkstd/map>

// initialze the class variables
int vtkPVProcessModule::GlobalLODFlag = 0;
int vtkPVProcessModule::GlobalStreamBlock = 0;

//----------------------------------------------------------------------------
//============================================================================
class vtkPVProcessModuleInternals
{
public:
  typedef vtkstd::map<vtkStdString, vtkStdString> MapStringToString;
  MapStringToString Paths;
};
//============================================================================
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVProcessModule);
vtkCxxRevisionMacro(vtkPVProcessModule, "$Revision$");

//----------------------------------------------------------------------------
vtkPVProcessModule::vtkPVProcessModule()
{
  this->Internals = new vtkPVProcessModuleInternals;
  this->MPIMToNSocketConnectionID.ID = 0;
  this->LogThreshold = 0;
  this->ServerInformation = vtkPVServerInformation::New();
  this->UseTriangleStrips = 0;
  this->UseImmediateMode = 1;
  this->Options = 0;
  this->ApplicationInstallationDirectory = 0;
  this->Timer = vtkTimerLog::New();
}

//----------------------------------------------------------------------------
vtkPVProcessModule::~vtkPVProcessModule()
{ 
  this->SetGUIHelper(0);
  this->SetApplicationInstallationDirectory(0);
  this->FinalizeInterpreter();
  this->ServerInformation->Delete();
  this->Timer->Delete();
  delete this->Internals;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::Start(int argc, char **argv)
{
  if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for vtkPVProcessModule to be able to run a gui.");
    return -1;
    }
  
  if (this->Controller == NULL)
    {
    this->Controller = vtkDummyController::New();
    vtkMultiProcessController::SetGlobalController(this->Controller);
    }

  return this->GUIHelper->RunGUIStart(argc, argv, 1, 0);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::Exit()
{
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetDirectoryListing(const char* dir,
                                            vtkStringList* dirs,
                                            vtkStringList* files,
                                            int save)
{
  // Get the listing from the server.
  vtkClientServerStream stream;
  vtkClientServerID lid = 
    this->NewStreamObject("vtkPVServerFileListing", stream);
  stream << vtkClientServerStream::Invoke
         << lid << "GetFileListing" << dir << save
         << vtkClientServerStream::End;
  this->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream);
  vtkClientServerStream result;
  if(!this->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0, 0, &result))
    {
    vtkErrorMacro("Error getting file list result from server.");
    this->DeleteStreamObject(lid, stream);
    this->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream);
    return 0;
    }
  this->DeleteStreamObject(lid, stream);
  this->SendStream(vtkProcessModule::DATA_SERVER_ROOT, stream);

  // Parse the listing.
  if ( dirs )
    {
    dirs->RemoveAllItems();
    }
  if ( files )
    {
    files->RemoveAllItems();
    }
  if(result.GetNumberOfMessages() == 2)
    {
    int i;
    // The first message lists directories.
    if ( dirs )
      {
      for(i=0; i < result.GetNumberOfArguments(0); ++i)
        {
        const char* d;
        if(result.GetArgument(0, i, &d))
          {
          dirs->AddString(d);
          }
        else
          {
          vtkErrorMacro("Error getting directory name from listing.");
          }
        }
      }

    // The second message lists files.
    if ( files )
      {
      for(i=0; i < result.GetNumberOfArguments(1); ++i)
        {
        const char* f;
        if(result.GetArgument(1, i, &f))
          {
          files->AddString(f);
          }
        else
          {
          vtkErrorMacro("Error getting file name from listing.");
          }
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
vtkObjectBase* vtkPVProcessModule::GetObjectFromID(vtkClientServerID id)
{
  return this->Interpreter->GetObjectFromID(id);
}

//----------------------------------------------------------------------------
vtkObjectBase* vtkPVProcessModule::GetObjectFromIntID(unsigned int idin)
{
  vtkClientServerID id;
  id.ID = idin;
  return this->GetObjectFromID(id);
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "LogThreshold: " << this->LogThreshold << endl;
  os << indent << "UseTriangleStrips: " << this->UseTriangleStrips << endl;
  os << indent << "UseImmediateMode: " << this->UseImmediateMode << endl;

  os << indent << "Options: ";
  if(this->Options)
    {
    this->Options->PrintSelf(os << endl, indent.GetNextIndent());
    }
  else
    {
    os << "(none)" << endl;
    }

  os << indent << "ServerInformation: ";
  if (this->ServerInformation)
    {
    this->ServerInformation->PrintSelf(os << endl, indent.GetNextIndent());
    }
  else
    {
    os << "(none)" << endl;;
    }
  os << indent << "ApplicationInstallationDirectory: " << (this->ApplicationInstallationDirectory?this->ApplicationInstallationDirectory:"(none)") << endl;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::InitializeInterpreter()
{
  if(this->Interpreter)
    {
    return;
    }

  this->Superclass::InitializeInterpreter();

}

//----------------------------------------------------------------------------
void vtkPVProcessModule::FinalizeInterpreter()
{
  if(!this->Interpreter)
    {
    return;
    }

  this->Superclass::FinalizeInterpreter();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::LoadModule(const char* name, const char* directory)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << this->GetProcessModuleID()
         << "LoadModuleInternal" << name << directory
         << vtkClientServerStream::End;
  this->SendStream(vtkProcessModule::DATA_SERVER, stream);
  int result = 0;
  if(!this->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0, 0, &result))
    {
    vtkErrorMacro("LoadModule could not get result from server.");
    return 0;
    }
  return result;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::LoadModuleInternal(const char* name,
                                           const char* directory)
{
  const char* paths[] = {directory, 0};
  return this->Interpreter->Load(name, paths);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendPrepareProgress()
{ 
  if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SendPrepareProgress.");
    return;
    }
  this->GUIHelper->SendPrepareProgress();
  this->Superclass::SendPrepareProgress();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendCleanupPendingProgress()
{
  this->Superclass::SendCleanupPendingProgress();
  if ( this->ProgressRequests > 0 )
    {
    return;
    }
 if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SendCleanupPendingProgress.");
    return;
    }
  this->GUIHelper->SendCleanupPendingProgress();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetLocalProgress(const char* filter, int progress)
{
 if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SetLocalProgress.  " << filter << " " << progress);
    return;
    }
 this->GUIHelper->SetLocalProgress(filter, progress);
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::LogStartEvent(char* str)
{
  vtkTimerLog::MarkStartEvent(str);
  this->Timer->StartTimer();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::LogEndEvent(char* str)
{
  this->Timer->StopTimer();
  vtkTimerLog::MarkEndEvent(str);
  if (strstr(str, "id:") && this->LogFile)
    {
    *this->LogFile << str << ", " << this->Timer->GetElapsedTime()
                   << " seconds" << endl;
    *this->LogFile << "--- Virtual memory available: "
                   << this->MemoryInformation->GetAvailableVirtualMemory()
                   << " KB" << endl;
    *this->LogFile << "--- Physical memory available: "
                   << this->MemoryInformation->GetAvailablePhysicalMemory()
                   << " KB" << endl;
    }
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetLogBufferLength(int length)
{
  vtkTimerLog::SetMaxEntries(length);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::ResetLog()
{
  vtkTimerLog::ResetLog();
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetEnableLog(int flag)
{
  vtkTimerLog::SetLogging(flag);
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalLODFlag(int val)
{
  if (vtkPVProcessModule::GlobalLODFlag == val)
    {
    return;
    }
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << this->GetProcessModuleID()
         << "SetGlobalLODFlagInternal"
         << val
         << vtkClientServerStream::End;
  this->SendStream(
    vtkProcessModule::CLIENT|vtkProcessModule::DATA_SERVER, stream);
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalLODFlagInternal(int val)
{
  vtkPVProcessModule::GlobalLODFlag = val;
}
//----------------------------------------------------------------------------
int vtkPVProcessModule::GetGlobalLODFlag()
{
  return vtkPVProcessModule::GlobalLODFlag;
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalStreamBlock(int val)
{
  if (vtkPVProcessModule::GlobalStreamBlock == val)
    {
    return;
    }
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
         << this->GetProcessModuleID()
         << "SetGlobalStreamBlockInternal"
         << val
         << vtkClientServerStream::End;
  this->SendStream(
    vtkProcessModule::CLIENT|vtkProcessModule::DATA_SERVER, stream);
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalStreamBlockInternal(int val)
{
  vtkPVProcessModule::GlobalStreamBlock = val;
}
//----------------------------------------------------------------------------
int vtkPVProcessModule::GetGlobalStreamBlock()
{
  return vtkPVProcessModule::GlobalStreamBlock;
}

//============================================================================
// Stuff that is a part of render-process module.
//-----------------------------------------------------------------------------
const char* vtkPVProcessModule::GetPath(const char* tag, const char* relativePath, const char* file)
{
  if ( !tag || !relativePath || !file )
    {
    return 0;
    }
  int found=0;

  if(this->Options)
    {
    vtksys_stl::string selfPath, errorMsg;
    vtksys_stl::string oldSelfPath;
    if (vtksys::SystemTools::FindProgramPath(
        this->Options->GetArgv0(), selfPath, errorMsg))
      {
      oldSelfPath = selfPath;
      selfPath = vtksys::SystemTools::GetFilenamePath(selfPath);
      selfPath += "/../share/paraview-" PARAVIEW_VERSION;
      vtkstd::string str = selfPath;
      str += "/";
      str += relativePath;
      str += "/";
      str += file;
      if(vtksys::SystemTools::FileExists(str.c_str()))
        {
        this->Internals->Paths[tag] = selfPath.c_str();
        found = 1;
        }
      }
    if ( !found )
      {
      selfPath = oldSelfPath;
      selfPath = vtksys::SystemTools::GetFilenamePath(selfPath);
      selfPath += "/../../share/paraview-" PARAVIEW_VERSION;
      vtkstd::string str = selfPath;
      str += "/";
      str += relativePath;
      str += "/";
      str += file;
      if(vtksys::SystemTools::FileExists(str.c_str()))
        {
        this->Internals->Paths[tag] = selfPath.c_str();
        found = 1;
        }
      }
    }

  if (!found)
    {
    // Look in binary and installation directories
    const char** dir;
    for(dir=PARAVIEW_PATHS; !found && *dir; ++dir)
      {
      vtkstd::string fullFile = *dir;
      fullFile += "/";
      fullFile += relativePath;
      fullFile += "/";
      fullFile += file;
      if(vtksys::SystemTools::FileExists(fullFile.c_str()))
        {
        this->Internals->Paths[tag] = *dir;
        found = 1;
        }
      }
    }
  if ( this->Internals->Paths.find(tag) == this->Internals->Paths.end() )
    {
    return 0;
    }

  return this->Internals->Paths[tag].c_str();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetRenderNodePort()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetRenderNodePort();
}

//----------------------------------------------------------------------------
char* vtkPVProcessModule::GetMachinesFileName()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetMachinesFileName();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetClientMode()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetClientMode();
}

//----------------------------------------------------------------------------
unsigned int vtkPVProcessModule::GetNumberOfMachines()
{
  vtkPVServerOptions *opt = vtkPVServerOptions::SafeDownCast(this->Options);
  if (!opt)
    {
    return 0;
    }
  return opt->GetNumberOfMachines();
}

//----------------------------------------------------------------------------
const char* vtkPVProcessModule::GetMachineName(unsigned int idx)
{
  vtkPVServerOptions *opt = vtkPVServerOptions::SafeDownCast(this->Options);
  if (!opt)
    {
    return NULL;
    }
  return opt->GetMachineName(idx);
}

//----------------------------------------------------------------------------
// This method leaks memory.  It is a quick and dirty way to set different 
// DISPLAY environment variables on the render server.  I think the string 
// cannot be deleted until paraview exits.  The var should have the form:
// "DISPLAY=amber1"
void vtkPVProcessModule::SetProcessEnvironmentVariable(int processId,
                                                       const char* var)
{
  (void)processId;
  char* envstr = vtksys::SystemTools::DuplicateString(var);
  putenv(envstr);
}

//-----------------------------------------------------------------------------
void vtkPVProcessModule::SynchronizeServerClientOptions()
{
  if (!this->Options->GetTileDimensions()[0])
    {
    this->Options->SetTileDimensions
      (this->ServerInformation->GetTileDimensions());
    }
  if (!this->Options->GetUseOffscreenRendering())
    {
    this->Options->SetUseOffscreenRendering
      (this->ServerInformation->GetUseOffscreenRendering());
    }
}

