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
#include <sys/stat.h>
#include "vtkPVServerInformation.h"
#include "vtkPVCaveRenderModule.h"
#include "vtkToolkits.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkPVProcessModule.h"
#include "vtkSMMultiDisplayPartDisplay.h"
#include "vtkPVLODPartDisplayInformation.h"
#include "vtkCollection.h"
#include "vtkRenderer.h"
#include "vtkTimerLog.h"
#include "vtkClientServerStream.h"
#include "vtkCaveRenderManager.h"
#include "vtkMPIMToNSocketConnection.h"
#include "vtkPVOptions.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVCaveRenderModule);
vtkCxxRevisionMacro(vtkPVCaveRenderModule, "$Revision$");



//***************************************************************************
//===========================================================================

//----------------------------------------------------------------------------
vtkPVCaveRenderModule::vtkPVCaveRenderModule()
{
}

//----------------------------------------------------------------------------
vtkPVCaveRenderModule::~vtkPVCaveRenderModule()
{

}


//----------------------------------------------------------------------------
void vtkPVCaveRenderModule::SetProcessModule(vtkProcessModule *pm)
{
  this->Superclass::SetProcessModule(pm);
  if (this->ProcessModule == NULL)
    {
    return;
    }
  int numDisplays;

  // We had trouble with SGI/aliasing with compositing.
  if (this->RenderWindow->IsA("vtkOpenGLRenderWindow") &&
      (this->ProcessModule->GetNumberOfPartitions() > 1))
    {
    this->ProcessModule->GetStream() << vtkClientServerStream::Invoke
                    << this->RenderWindowID 
                    << "SetMultiSamples" << 0 
                    << vtkClientServerStream::End;
    }

  this->Composite = NULL;
  this->CompositeID = this->ProcessModule->NewStreamObject("vtkCaveRenderManager");
  this->ProcessModule->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::RENDER_SERVER);

  if (this->ProcessModule->GetOptions()->GetClientMode())
    {
    this->ProcessModule->GetStream()
      << vtkClientServerStream::Invoke
      << this->CompositeID << "SetClientFlag" << 1
      << vtkClientServerStream::End;
    this->ProcessModule->SendStream(vtkProcessModule::CLIENT);

    this->ProcessModule->GetStream()
      << vtkClientServerStream::Invoke
      << this->ProcessModule->GetProcessModuleID() << "GetRenderServerSocketController"
      << vtkClientServerStream::End;
    this->ProcessModule->GetStream()
      << vtkClientServerStream::Invoke
      << this->CompositeID << "SetSocketController"
      << vtkClientServerStream::LastResult
      << vtkClientServerStream::End;
    this->ProcessModule->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::RENDER_SERVER);

    // Timing of this set is important for server.
    // It has to be after the SocketController has been set,
    // but before the Desplays are defined.
    this->ProcessModule->GetStream()
      << vtkClientServerStream::Invoke
      << this->CompositeID << "InitializeRMIs"
      << vtkClientServerStream::End;
    this->ProcessModule->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::RENDER_SERVER);

    // Setup the tiles.
    // We need a better way to retreive the number of processes
    numDisplays = this->ProcessModule->GetNumberOfPartitions();
    vtkMPIMToNSocketConnection* m2n = NULL;
    if (this->ProcessModule->GetMPIMToNSocketConnectionID().ID)
      {
      m2n = vtkMPIMToNSocketConnection::SafeDownCast(
        this->ProcessModule->GetObjectFromID(this->ProcessModule->GetMPIMToNSocketConnectionID()));
      }   
   if (m2n)
      {
      numDisplays = m2n->GetNumberOfConnections();
      }    

    this->LoadConfigurationFile(numDisplays);
    }
  else
    { // Timing of this set is important for server.
    this->ProcessModule->GetStream()
      << vtkClientServerStream::Invoke
      << this->CompositeID << "InitializeRMIs"
      << vtkClientServerStream::End;
    this->ProcessModule->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::RENDER_SERVER);
    }

  this->ProcessModule->GetStream() 
    << vtkClientServerStream::Invoke
    <<  this->CompositeID 
    << "SetRenderWindow"
    << this->RenderWindowID
    << vtkClientServerStream::End;
  this->ProcessModule->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::RENDER_SERVER);
}

//----------------------------------------------------------------------------
void vtkPVCaveRenderModule::LoadConfigurationFile(int numDisplays)
{
  int idx;
  const char* fileName = this->ProcessModule->GetOptions()->GetCaveConfigurationFileName();
  ifstream *File = 0;
  if(!fileName)
    {
    vtkErrorMacro("Missing configuration file.");
    return;
    } 
  
  // Open the new file
  struct stat fs;
  if ( !stat( fileName, &fs) )
    {
#ifdef _WIN32
    File = new ifstream(fileName, ios::in | ios::binary);
#else
    File = new ifstream(fileName, ios::in);
#endif
    }
  if (! File)
    {
    vtkErrorMacro(<< "Initialize: Could not open file " << fileName);
    return;
    }

  if (File->fail())
    {
    File->close();
    delete File;
    vtkErrorMacro(<< "Initialize: Could not open file " << fileName);
    return;
    }

  vtkCaveRenderManager* crm = 
    vtkCaveRenderManager::SafeDownCast(this->ProcessModule->GetObjectFromID(this->CompositeID));

  for (idx = 0; idx < numDisplays; ++idx)
    { // Just a test case.  Configuration file later.
    char line[256];
    double o[3];
    double x[3];
    double y[3];

    File->getline(line,256);
    if (File->fail())
      {
      File->close();
      delete File;
      vtkErrorMacro(<< "Could not read display " << idx);
      return;
      }

    this->ProcessModule->GetStream() 
      << vtkClientServerStream::Invoke
      << this->ProcessModule->GetProcessModuleID()
      << "SetProcessEnvironmentVariable"
      << idx << line
      << vtkClientServerStream::End;
    this->ProcessModule->SendStream(vtkProcessModule::RENDER_SERVER);
    
    //this->ProcessModule->SetProcessEnvironmentVariable(idx, line); 
    cout << "Setting line: " << line << endl;

    File->getline(line,256);
    sscanf(line, "%lf %lf %lf",o , o+1, o+2);

    File->getline(line,256);
    sscanf(line, "%lf %lf %lf",x , x+1, x+2);

    File->getline(line,256);
    sscanf(line, "%lf %lf %lf",y , y+1, y+2);

    if (File->fail())
      {
      File->close();
      delete File;
      vtkErrorMacro("Unexpected end of configuration file.");
      return;
      }

    crm->DefineDisplay(idx, o, x, y);
    }
  File->close();
  delete File;
}

//----------------------------------------------------------------------------
vtkSMPartDisplay* vtkPVCaveRenderModule::CreatePartDisplay()
{
  vtkSMMultiDisplayPartDisplay* pDisp = vtkSMMultiDisplayPartDisplay::New();
  pDisp->SetProcessModule(vtkPVProcessModule::SafeDownCast(this->GetProcessModule()));
  return pDisp;
}


//----------------------------------------------------------------------------
void vtkPVCaveRenderModule::StillRender()
{
  vtkObject* object;
  vtkSMCompositePartDisplay* pDisp;

  // Change the collection flags and update.
  this->Displays->InitTraversal();
  while ( (object = this->Displays->GetNextItemAsObject()) )
    {
    pDisp = vtkSMCompositePartDisplay::SafeDownCast(object);
    if (pDisp && pDisp->GetVisibility())
      {
      pDisp->SetCollectionDecision(1);
      pDisp->SetLODCollectionDecision(1);
      pDisp->Update();
      }
    }

  // Still Render can get called some funky ways.
  // Interactive renders get called through the PVInteractorStyles
  // which cal ResetCameraClippingRange on the Renderer.
  // We could convert them to call a method on the module directly ...
  this->Renderer->ResetCameraClippingRange();

  this->RenderWindow->SetDesiredUpdateRate(0.002);
  // this->GetPVWindow()->GetInteractor()->GetStillUpdateRate());

  this->ProcessModule->SetGlobalLODFlag(0);


  vtkTimerLog::MarkStartEvent("Still Render");
  this->RenderWindow->Render();
  vtkTimerLog::MarkEndEvent("Still Render");
}


//----------------------------------------------------------------------------
void vtkPVCaveRenderModule::InteractiveRender()
{
  vtkObject* object;
  vtkSMCompositePartDisplay* pDisp;
  vtkPVLODPartDisplayInformation* info;
  unsigned long totalGeoMemory = 0;
  unsigned long totalLODMemory = 0;

  // Compute memory totals.
  this->Displays->InitTraversal();
  while ( (object = this->Displays->GetNextItemAsObject()) )
    {
    pDisp = vtkSMCompositePartDisplay::SafeDownCast(object);
    if (pDisp && pDisp->GetVisibility())
      {
      pDisp->SetCollectionDecision(1);
      pDisp->SetLODCollectionDecision(1);
      // This updates if required (collection disabled).
      info = pDisp->GetLODInformation();
      totalGeoMemory += info->GetGeometryMemorySize();
      totalLODMemory += info->GetLODGeometryMemorySize();
      }
    }

  // Make LOD decision.
  if ((float)(totalGeoMemory)/1000.0 < this->GetLODThreshold())
    {
    this->ProcessModule->SetGlobalLODFlag(0);
    }
  else
    {
    this->ProcessModule->SetGlobalLODFlag(1);
    }

  // This might be used for Reduction factor.
  this->RenderWindow->SetDesiredUpdateRate(5.0);
  // this->GetPVWindow()->GetInteractor()->GetStillUpdateRate());

  // Still Render can get called some funky ways.
  // Interactive renders get called through the PVInteractorStyles
  // which cal ResetCameraClippingRange on the Renderer.
  // We could convert them to call a method on the module directly ...
  // Reset camera clipping range has to be called after
  // LOD flag is set, other wise, the wrong bounds could be used.
  this->Renderer->ResetCameraClippingRange();
  
  vtkTimerLog::MarkStartEvent("Interactive Render");
  this->RenderWindow->Render();
  vtkTimerLog::MarkEndEvent("Interactive Render");
}


//----------------------------------------------------------------------------
void vtkPVCaveRenderModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

