/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkCompositeManager.h"

#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCompressCompositer.h"
#include "vtkFloatArray.h"
#include "vtkLight.h"
#include "vtkLightCollection.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPolyDataMapper.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkRendererCollection.h"
#include "vtkTimerLog.h"
#include "vtkToolkits.h"
#include "vtkUnsignedCharArray.h"

#ifdef _WIN32
#include "vtkWin32OpenGLRenderWindow.h"
#endif

#ifdef VTK_USE_MPI
 #include <mpi.h>
#endif

vtkCxxRevisionMacro(vtkCompositeManager, "$Revision$");
vtkStandardNewMacro(vtkCompositeManager);


// Structures to communicate render info.
struct vtkCompositeRenderWindowInfo 
{
  int Size[2];
  int ReductionFactor;
  int NumberOfRenderers;
  float DesiredUpdateRate;
};

struct vtkCompositeRendererInfo 
{
  float CameraPosition[3];
  float CameraFocalPoint[3];
  float CameraViewUp[3];
  float CameraClippingRange[2];
  float LightPosition[3];
  float LightFocalPoint[3];
  float Background[3];
  float ParallelScale;
};

#define vtkInitializeVector3(v) { v[0] = 0; v[1] = 0; v[2] = 0; }
#define vtkInitializeVector2(v) { v[0] = 0; v[1] = 0; }
#define vtkInitializeCompositeRendererInfoMacro(r)      \
  {                                                     \
  vtkInitializeVector3(r.CameraPosition);               \
  vtkInitializeVector3(r.CameraFocalPoint);             \
  vtkInitializeVector3(r.CameraViewUp);                 \
  vtkInitializeVector2(r.CameraClippingRange);          \
  vtkInitializeVector3(r.LightPosition);                \
  vtkInitializeVector3(r.LightFocalPoint);              \
  vtkInitializeVector3(r.Background);                   \
  r.ParallelScale = 0.0;                                \
  }
  


//-------------------------------------------------------------------------
vtkCompositeManager::vtkCompositeManager()
{
  this->Compositer = vtkCompressCompositer::New();
  this->Compositer->Register(this);
  this->Compositer->Delete();

  this->RenderWindow = NULL;
  this->RenderWindowInteractor = NULL;
  this->Controller = vtkMultiProcessController::GetGlobalController();

  this->NumberOfProcesses = 1;

  if (this->Controller)
    {
    this->Controller->Register(this);
    this->NumberOfProcesses = this->Controller->GetNumberOfProcesses();
    }

  this->RendererSize[0] = this->RendererSize[1] = 0;

  this->StartTag = this->EndTag = 0;
  this->StartInteractorTag = 0;
  this->EndInteractorTag = 0;

  this->PData = this->ZData = NULL;
  this->LocalPData = this->LocalZData = NULL;

  this->Lock = 0;
  this->UseChar = 1;
  this->UseRGB = 1;
  this->UseCompositing = 1;
  
  this->ReductionFactor = 1;
  
  this->GetBuffersTime = 0.0;
  this->SetBuffersTime = 0.0;
  this->CompositeTime = 0.0;
  this->MaxRenderTime = 0.0;

  this->Manual = 0;

  this->Timer = vtkTimerLog::New();

  this->FirstRender = 0;

  this->DoMagnifyBuffer = 1;
}

  
//-------------------------------------------------------------------------
vtkCompositeManager::~vtkCompositeManager()
{
  this->SetRenderWindow(NULL);
  
  this->Timer->Delete();
  this->Timer = NULL;

  this->SetRendererSize(0,0);
  
  if (this->Controller)
    {
    this->Controller->UnRegister(this);
    this->Controller = NULL;
    }
  if (this->Compositer)
    {
    this->Compositer->UnRegister(this);
    this->Compositer = NULL;
    }

  if (this->Lock)
    {
    vtkErrorMacro("Destructing while locked!");
    }

  if (this->PData)
    {
    vtkCompositeManager::DeleteArray(this->PData);
    this->PData = NULL;
    }
  
  if (this->ZData)
    {
    vtkCompositeManager::DeleteArray(this->ZData);
    this->ZData = NULL;
    }

  if (this->LocalPData)
    {
    vtkCompositeManager::DeleteArray(this->LocalPData);
    this->LocalPData = NULL;
    }
  
  if (this->LocalZData)
    {
    vtkCompositeManager::DeleteArray(this->LocalZData);
    this->LocalZData = NULL;
    }
}

//-------------------------------------------------------------------------
// We may want to pass the render window as an argument for a sanity check.
void vtkCompositeManagerStartRender(vtkObject *caller,
                                 unsigned long vtkNotUsed(event), 
                                 void *clientData, void *)
{
  vtkCompositeManager *self = (vtkCompositeManager *)clientData;
  
  if (caller != self->GetRenderWindow())
    { // Sanity check.
    vtkGenericWarningMacro("Caller mismatch.");
    return;
    }

  self->StartRender();
}

//-------------------------------------------------------------------------
void vtkCompositeManagerEndRender(vtkObject *caller,
                                  unsigned long vtkNotUsed(event), 
                                  void *clientData, void *)
{
  vtkCompositeManager *self = (vtkCompositeManager *)clientData;
  
  if (caller != self->GetRenderWindow())
    { // Sanity check.
    vtkGenericWarningMacro("Caller mismatch.");
    return;
    }

  self->EndRender();
}


//-------------------------------------------------------------------------
void vtkCompositeManagerExitInteractor(vtkObject *vtkNotUsed(o),
                                       unsigned long vtkNotUsed(event), 
                                       void *clientData, void *)
{
  vtkCompositeManager *self = (vtkCompositeManager *)clientData;

  self->ExitInteractor();
}

//-------------------------------------------------------------------------
void vtkCompositeManagerResetCamera(vtkObject *caller,
                                    unsigned long vtkNotUsed(event), 
                                    void *clientData, void *)
{
  vtkCompositeManager *self = (vtkCompositeManager *)clientData;
  vtkRenderer *ren = (vtkRenderer*)caller;

  self->ResetCamera(ren);
}

//-------------------------------------------------------------------------
void vtkCompositeManagerResetCameraClippingRange(
  vtkObject *caller, unsigned long vtkNotUsed(event),void *clientData, void *)
{
  vtkCompositeManager *self = (vtkCompositeManager *)clientData;
  vtkRenderer *ren = (vtkRenderer*)caller;

  self->ResetCameraClippingRange(ren);
}

//-------------------------------------------------------------------------
void vtkCompositeManagerAbortRenderCheck(vtkObject*, unsigned long, void* arg,
                                         void*)
{
  vtkCompositeManager *self = (vtkCompositeManager*)arg;
  
  self->CheckForAbortRender();
}

//----------------------------------------------------------------------------
void vtkCompositeManagerRenderRMI(void *arg, void *, int, int)
{
  vtkCompositeManager* self = (vtkCompositeManager*) arg;
  
  self->RenderRMI();
}

//----------------------------------------------------------------------------
void vtkCompositeManagerSatelliteStartRender(vtkObject* vtkNotUsed(caller),
                                             unsigned long vtkNotUsed(event), 
                                             void *clientData, void *)
{
  vtkCompositeManager* self = (vtkCompositeManager*) clientData;
  
  self->SatelliteStartRender();
}


//----------------------------------------------------------------------------
void vtkCompositeManagerSatelliteEndRender(vtkObject* vtkNotUsed(caller),
                                           unsigned long vtkNotUsed(event), 
                                           void *clientData, void *)
{
  vtkCompositeManager* self = (vtkCompositeManager*) clientData;
  
  self->SatelliteEndRender();
}

//----------------------------------------------------------------------------
void vtkCompositeManagerComputeVisiblePropBoundsRMI(void *arg, void *, 
                                                    int, int)
{
  vtkCompositeManager* self = (vtkCompositeManager*) arg;
  
  self->ComputeVisiblePropBoundsRMI();
}

void vtkCompositeManager::SetReductionFactor(int factor)
{
  if (factor == this->ReductionFactor)
    {
    return;
    }
  
  this->ReductionFactor = factor;
}

//-------------------------------------------------------------------------
// Only process 0 needs start and end render callbacks.
void vtkCompositeManager::SetRenderWindow(vtkRenderWindow *renWin)
{
  vtkRendererCollection *rens;
  vtkRenderer *ren = 0;

  if (this->RenderWindow == renWin)
    {
    return;
    }
  this->Modified();

  if (this->RenderWindow)
    {
    // Remove all of the observers.
    if (this->Controller && this->Controller->GetLocalProcessId() == 0)
      {
      this->RenderWindow->RemoveObserver(this->StartTag);
      this->RenderWindow->RemoveObserver(this->EndTag);
      
      // Will make do with first renderer. (Assumes renderer does not change.)
      rens = this->RenderWindow->GetRenderers();
      rens->InitTraversal();
      ren = rens->GetNextItem();
      if (ren)
        {
        ren->RemoveObserver(this->ResetCameraTag);
        ren->RemoveObserver(this->ResetCameraClippingRangeTag);
        }
      }
    if ( this->Controller && this->Controller->GetLocalProcessId() != 0 )
      {
      this->RenderWindow->RemoveObserver(this->StartTag);
      this->RenderWindow->RemoveObserver(this->EndTag);
      }
    // Delete the reference.
    this->RenderWindow->UnRegister(this);
    this->RenderWindow =  NULL;
    this->SetRenderWindowInteractor(NULL);
    }
  if (renWin)
    {
    renWin->Register(this);
    this->RenderWindow = renWin;
    this->SetRenderWindowInteractor(renWin->GetInteractor());
    if (this->Controller)
      {
      // In case a subclass wants to check for aborts.
      vtkCallbackCommand* abc = vtkCallbackCommand::New();
      abc->SetCallback(vtkCompositeManagerAbortRenderCheck);
      abc->SetClientData(this);
      this->RenderWindow->AddObserver(vtkCommand::AbortCheckEvent, abc);
      abc->Delete();
      
      if (this->Controller && this->Controller->GetLocalProcessId() == 0)
        {
        vtkCallbackCommand *cbc;
        
        cbc= vtkCallbackCommand::New();
        cbc->SetCallback(vtkCompositeManagerStartRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
        this->StartTag = renWin->AddObserver(vtkCommand::StartEvent,cbc);
        cbc->Delete();
        
        cbc = vtkCallbackCommand::New();
        cbc->SetCallback(vtkCompositeManagerEndRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
        this->EndTag = renWin->AddObserver(vtkCommand::EndEvent,cbc);
        cbc->Delete();
        
        // Will make do with first renderer. (Assumes renderer does
        // not change.)
        rens = this->RenderWindow->GetRenderers();
        rens->InitTraversal();
        ren = rens->GetNextItem();
        if (ren)
          {
          cbc = vtkCallbackCommand::New();
          cbc->SetCallback(vtkCompositeManagerResetCameraClippingRange);
          cbc->SetClientData((void*)this);
          // ren will delete the cbc when the observer is removed.
          this->ResetCameraClippingRangeTag = 
          ren->AddObserver(vtkCommand::ResetCameraClippingRangeEvent,cbc);
          cbc->Delete();
          
          
          cbc = vtkCallbackCommand::New();
          cbc->SetCallback(vtkCompositeManagerResetCamera);
          cbc->SetClientData((void*)this);
          // ren will delete the cbc when the observer is removed.
          this->ResetCameraTag = 
          ren->AddObserver(vtkCommand::ResetCameraEvent,cbc);
          cbc->Delete();
          }
        }
      else if (this->Controller && this->Controller->GetLocalProcessId() != 0)
        {
        vtkCallbackCommand *cbc;
        
        cbc= vtkCallbackCommand::New();
        cbc->SetCallback(vtkCompositeManagerSatelliteStartRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
        this->StartTag = renWin->AddObserver(vtkCommand::StartEvent,cbc);
        cbc->Delete();
        
        cbc = vtkCallbackCommand::New();
        cbc->SetCallback(vtkCompositeManagerSatelliteEndRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
        this->EndTag = renWin->AddObserver(vtkCommand::EndEvent,cbc);
        cbc->Delete();
        }
      else
        {
        // This is here for some reason?
        // ren = ren; 
#ifdef _WIN32
        // I had a problem with some graphics cards getting front and
        // back buffers mixed up, so I made the remote render windows
        // single buffered. One nice feature of this is being able to
        // see the render in these helper windows.
        vtkWin32OpenGLRenderWindow *renWin;
  
        renWin = vtkWin32OpenGLRenderWindow::SafeDownCast(this->RenderWindow);
        if (renWin)
          {
          // Lets keep the render window single buffer
          renWin->DoubleBufferOff();
          // I do not want to replace the original.
          renWin = renWin;
          }
#endif
        }
      }
    }
}


//-------------------------------------------------------------------------
void vtkCompositeManager::SetController(vtkMultiProcessController *mpc)
{
  if (this->Controller == mpc)
    {
    return;
    }
  if (mpc)
    {
    mpc->Register(this);
    this->NumberOfProcesses = mpc->GetNumberOfProcesses();
    }
  if (this->Controller)
    {
    this->Controller->UnRegister(this);
    }
  this->Controller = mpc;

  if (this->Compositer)
    {
    this->Compositer->SetController(mpc);
    }
}
//-------------------------------------------------------------------------
void vtkCompositeManager::SetNumberOfProcesses(int numProcs)
{
  if (this->Controller)
    {
    if (numProcs > this->Controller->GetNumberOfProcesses() )
      {
      numProcs = this->Controller->GetNumberOfProcesses();
      }
    }
  if (this->NumberOfProcesses == numProcs)
    {
    return;
    }

  this->Modified();
  this->NumberOfProcesses = numProcs;
  if (this->Compositer)
    {
    this->Compositer->SetNumberOfProcesses(numProcs);
    }
}
//-------------------------------------------------------------------------
void vtkCompositeManager::SetCompositer(vtkCompositer *c)
{
  if (c == this->Compositer)
    {
    return;
    }
  if (c)
    {
    c->Register(this);
    c->SetController(this->Controller);
    c->SetNumberOfProcesses(this->NumberOfProcesses);
    }
  if (this->Compositer)
    {
    this->Compositer->UnRegister(this);
    this->Compositer = NULL;
    }
  this->Compositer = c;
}



//-------------------------------------------------------------------------
// Only satellite processes process interactor loops specially.
// We only setup callbacks in those processes (not process 0).
void 
vtkCompositeManager::SetRenderWindowInteractor(
  vtkRenderWindowInteractor *iren)
{
  if (this->RenderWindowInteractor == iren)
    {
    return;
    }

  if (this->Controller == NULL)
    {
    return;
    }
  
  if (this->RenderWindowInteractor)
    {
    if (!this->Controller->GetLocalProcessId())
      {
      this->RenderWindowInteractor->RemoveObserver(this->EndInteractorTag);
      }
    this->RenderWindowInteractor->UnRegister(this);
    this->RenderWindowInteractor =  NULL;
    }
  if (iren)
    {
    iren->Register(this);
    this->RenderWindowInteractor = iren;
    
    if (!this->Controller->GetLocalProcessId())
      {
      vtkCallbackCommand *cbc;
      cbc= vtkCallbackCommand::New();
      cbc->SetCallback(vtkCompositeManagerExitInteractor);
      cbc->SetClientData((void*)this);
      // IRen will delete the cbc when the observer is removed.
      this->EndInteractorTag = iren->AddObserver(vtkCommand::ExitEvent,cbc);
      cbc->Delete();
      }
    }
}

//----------------------------------------------------------------------------
void vtkCompositeManager::RenderRMI()
{
  // Start and end methods take care of synchronization and compositing
  vtkRenderWindow* renWin = this->RenderWindow;
  renWin->Render();
}


//-------------------------------------------------------------------------
void vtkCompositeManager::SatelliteStartRender()
{
  int i;
  vtkCompositeRenderWindowInfo winInfo;
  vtkCompositeRendererInfo renInfo;
  vtkRendererCollection *rens;
  vtkRenderer* ren;
  vtkCamera *cam = 0;
  vtkLightCollection *lc;
  vtkLight *light;
  vtkRenderWindow* renWin = this->RenderWindow;
  vtkMultiProcessController *controller = this->Controller;
  
  vtkInitializeCompositeRendererInfoMacro(renInfo);
  
  // Receive the window size.
  controller->Receive((char*)(&winInfo), 
                      sizeof(struct vtkCompositeRenderWindowInfo), 0, 
                      vtkCompositeManager::WIN_INFO_TAG);
  renWin->SetSize(winInfo.Size);
  renWin->SetDesiredUpdateRate(winInfo.DesiredUpdateRate);

  // Synchronize the renderers.
  rens = renWin->GetRenderers();
  rens->InitTraversal();
  for (i = 0; i < winInfo.NumberOfRenderers; ++i)
    {
    // Receive the camera information.

    // We put this before receive because we want the pipeline to be
    // updated the first time if the camera does not exist and we want
    // it to happen before we block in receive
    ren = rens->GetNextItem();
    if (ren)
      {
      cam = ren->GetActiveCamera();
      }

    controller->Receive((char*)(&renInfo), 
                        sizeof(struct vtkCompositeRendererInfo), 
                        0, vtkCompositeManager::REN_INFO_TAG);
    if (ren == NULL)
      {
      vtkErrorMacro("Renderer mismatch.");
      }
    else
      {
      lc = ren->GetLights();
      lc->InitTraversal();
      light = lc->GetNextItem();
  
      cam->SetPosition(renInfo.CameraPosition);
      cam->SetFocalPoint(renInfo.CameraFocalPoint);
      cam->SetViewUp(renInfo.CameraViewUp);
      cam->SetClippingRange(renInfo.CameraClippingRange);
      if (renInfo.ParallelScale != 0.0)
        {
        cam->ParallelProjectionOn();
        cam->SetParallelScale(renInfo.ParallelScale);
        }
      else
        {
        cam->ParallelProjectionOff();   
        }
      if (light)
        {
        light->SetPosition(renInfo.LightPosition);
        light->SetFocalPoint(renInfo.LightFocalPoint);
        }
      ren->SetBackground(renInfo.Background);
      ren->SetViewport(0, 0, 1.0/(float)winInfo.ReductionFactor, 
                       1.0/(float)winInfo.ReductionFactor);
      }
    }
  this->SetRendererSize(winInfo.Size[0]/winInfo.ReductionFactor, 
                        winInfo.Size[1]/winInfo.ReductionFactor);
}

//-------------------------------------------------------------------------
void vtkCompositeManager::SatelliteEndRender()
{
  if (this->CheckForAbortComposite())
    {
    return;
    }
  
  this->Composite();
}

//-------------------------------------------------------------------------
// This is only called in the satellite processes (not 0).
void vtkCompositeManager::InitializeRMIs()
{
  if (this->Controller == NULL)
    {
    vtkErrorMacro("Missing Controller.");
    return;
    }

  this->Controller->AddRMI(vtkCompositeManagerRenderRMI, (void*)this, 
                           vtkCompositeManager::RENDER_RMI_TAG); 

  this->Controller->AddRMI(vtkCompositeManagerComputeVisiblePropBoundsRMI,
     (void*)this, vtkCompositeManager::COMPUTE_VISIBLE_PROP_BOUNDS_RMI_TAG);  
}

//-------------------------------------------------------------------------
// This is only called in the satellite processes (not 0).
void vtkCompositeManager::StartInteractor()
{
  if (this->Controller == NULL)
    {
    vtkErrorMacro("Missing Controller.");
    return;
    }

  this->InitializeRMIs();

  if (!this->Controller->GetLocalProcessId())
    {
    if (!this->RenderWindowInteractor)
      {
      vtkErrorMacro("Missing interactor.");
      this->ExitInteractor();
      return;
      }
    this->RenderWindowInteractor->Initialize();
    this->RenderWindowInteractor->Start();
    }
  else
    {
    this->Controller->ProcessRMIs();
    }
}

//-------------------------------------------------------------------------
// This is only called in process 0.
void vtkCompositeManager::ExitInteractor()
{
  int numProcs, id;
  
  if (this->Controller == NULL)
    {
    vtkErrorMacro("Missing Controller.");
    return;
    }

  numProcs = this->Controller->GetNumberOfProcesses();
  for (id = 1; id < numProcs; ++id)
    {
    this->Controller->TriggerRMI(id, 
                                 vtkMultiProcessController::BREAK_RMI_TAG);
    }
}


//-------------------------------------------------------------------------
// Only called in process 0.
void vtkCompositeManager::StartRender()
{
  struct vtkCompositeRenderWindowInfo winInfo;
  struct vtkCompositeRendererInfo renInfo;
  int id, numProcs;
  int *size;
  vtkRendererCollection *rens;
  vtkRenderer* ren;
  vtkCamera *cam;
  vtkLightCollection *lc;
  vtkLight *light;
  
  vtkDebugMacro("StartRender");
  
  // Used to time the total render (without compositing.)
  this->Timer->StartTimer();

  if (!this->UseCompositing)
    {
    return;
    }  

  vtkRenderWindow* renWin = this->RenderWindow;
  vtkMultiProcessController *controller = this->Controller;

  if (controller == NULL || this->Lock)
    {
    return;
    }
  
  // Lock here, unlock at end render.
  this->Lock = 1;
  
  // Trigger the satellite processes to start their render routine.
  rens = this->RenderWindow->GetRenderers();
  numProcs = this->NumberOfProcesses;
  size = this->RenderWindow->GetSize();
  if (this->ReductionFactor > 0)
    {
    winInfo.Size[0] = size[0];
    winInfo.Size[1] = size[1];
    winInfo.ReductionFactor = this->ReductionFactor;
    vtkRenderer* renderer =
      ((vtkRenderer*)this->RenderWindow->GetRenderers()->GetItemAsObject(0));
    renderer->SetViewport(0, 0, 1.0/this->ReductionFactor, 
                          1.0/this->ReductionFactor);
    }
  else
    {
    winInfo.Size[0] = size[0];
    winInfo.Size[1] = size[1];
    winInfo.ReductionFactor = 1;
    }
  winInfo.NumberOfRenderers = rens->GetNumberOfItems();
  winInfo.DesiredUpdateRate = this->RenderWindow->GetDesiredUpdateRate();
  
  if ( winInfo.Size[0] == 0 || winInfo.Size[1] == 0 )
    {
    this->FirstRender = 1;
    renWin->SwapBuffersOff();
    return;
    }

  this->SetRendererSize(winInfo.Size[0]/this->ReductionFactor, 
                        winInfo.Size[1]/this->ReductionFactor);
  
  for (id = 1; id < numProcs; ++id)
    {
    if (this->Manual == 0)
      {
      controller->TriggerRMI(id, NULL, 0, 
                             vtkCompositeManager::RENDER_RMI_TAG);
      }
    // Synchronize the size of the windows.
    controller->Send((char*)(&winInfo), 
                     sizeof(vtkCompositeRenderWindowInfo), id, 
                     vtkCompositeManager::WIN_INFO_TAG);
    }
  
  // Make sure the satellite renderers have the same camera I do.
  // Note: This will lockup unless every process has the same number
  // of renderers.
  rens->InitTraversal();
  while ( (ren = rens->GetNextItem()) )
    {
    cam = ren->GetActiveCamera();
    lc = ren->GetLights();
    lc->InitTraversal();
    light = lc->GetNextItem();
    cam->GetPosition(renInfo.CameraPosition);
    cam->GetFocalPoint(renInfo.CameraFocalPoint);
    cam->GetViewUp(renInfo.CameraViewUp);
    cam->GetClippingRange(renInfo.CameraClippingRange);
    if (cam->GetParallelProjection())
      {
      renInfo.ParallelScale = cam->GetParallelScale();
      }
    else
      {
      renInfo.ParallelScale = 0.0;
      }
    if (light)
      {
      light->GetPosition(renInfo.LightPosition);
      light->GetFocalPoint(renInfo.LightFocalPoint);
      }
    ren->GetBackground(renInfo.Background);
    
    for (id = 1; id < numProcs; ++id)
      {
      controller->Send((char*)(&renInfo),
                       sizeof(struct vtkCompositeRendererInfo), id, 
                       vtkCompositeManager::REN_INFO_TAG);
      }
    }
  
  // Turn swap buffers off before the render so the end render method
  // has a chance to add to the back buffer.
  renWin->SwapBuffersOff();

  vtkTimerLog::MarkStartEvent("Render Geometry");
}

//-------------------------------------------------------------------------
void vtkCompositeManager::EndRender()
{
  if (!this->UseCompositing)
    {
    return;
    }

  vtkTimerLog::MarkEndEvent("Render Geometry");

  if (this->FirstRender)
    {
    this->FirstRender = 0;
    this->Lock = 0;
    this->StartRender();
    }

  vtkRenderWindow* renWin = this->RenderWindow;
  //vtkMultiProcessController *controller = this->Controller;
  int numProcs;
  
  // EndRender only happens on root.
  if (this->CheckForAbortComposite())
    {
    renWin->SwapBuffersOn();  
    this->Lock = 0;
    return;
    }  

  numProcs = this->NumberOfProcesses;
  if (numProcs > 1)
    {
    this->Composite();
    }
  else
    {
    // Stop the timer that has been timing the render.  Normally done
    // in composite.
    this->Timer->StopTimer();
    this->MaxRenderTime = this->Timer->GetElapsedTime();
    }
  
  // Force swap buffers here.
  renWin->SwapBuffersOn();  
  renWin->Frame();
  
  // Release lock.
  this->Lock = 0;
}


//-------------------------------------------------------------------------
void vtkCompositeManager::ResetCamera(vtkRenderer *ren)
{
  float bounds[6];

  if (this->Controller == NULL || this->Lock)
    {
    return;
    }

  this->Lock = 1;
  
  this->ComputeVisiblePropBounds(ren, bounds);
  // Keep from setting camera from some outrageous value.
  if (bounds[0]>bounds[1] || bounds[2]>bounds[3] || bounds[4]>bounds[5])
    {
    // See if the not pickable values are better.
    ren->ComputeVisiblePropBounds(bounds);
    if (bounds[0]>bounds[1] || bounds[2]>bounds[3] || bounds[4]>bounds[5])
      {
      this->Lock = 0;
      return;
      }
    }
  ren->ResetCamera(bounds);
  
  this->Lock = 0;
}

//-------------------------------------------------------------------------
void vtkCompositeManager::ResetCameraClippingRange(vtkRenderer *ren)
{
  float bounds[6];

  if (this->Controller == NULL || this->Lock)
    {
    return;
    }

  this->Lock = 1;
  
  this->ComputeVisiblePropBounds(ren, bounds);
  ren->ResetCameraClippingRange(bounds);

  this->Lock = 0;
}

//----------------------------------------------------------------------------
void vtkCompositeManager::ComputeVisiblePropBounds(vtkRenderer *ren, 
                                                   float bounds[6])
{
  float tmp[6];
  int id, num;
  
  num = this->NumberOfProcesses;  
  for (id = 1; id < num; ++id)
    {
    this->Controller->TriggerRMI(id,COMPUTE_VISIBLE_PROP_BOUNDS_RMI_TAG);
    }

  ren->ComputeVisiblePropBounds(bounds);

  for (id = 1; id < num; ++id)
    {
    this->Controller->Receive(tmp, 6, id, vtkCompositeManager::BOUNDS_TAG);
    if (tmp[0] < bounds[0]) {bounds[0] = tmp[0];}
    if (tmp[1] > bounds[1]) {bounds[1] = tmp[1];}
    if (tmp[2] < bounds[2]) {bounds[2] = tmp[2];}
    if (tmp[3] > bounds[3]) {bounds[3] = tmp[3];}
    if (tmp[4] < bounds[4]) {bounds[4] = tmp[4];}
    if (tmp[5] > bounds[5]) {bounds[5] = tmp[5];}
    }
}

//----------------------------------------------------------------------------
void vtkCompositeManager::ComputeVisiblePropBoundsRMI()
{
  vtkRendererCollection *rens;
  vtkRenderer* ren;
  float bounds[6];
  
  rens = this->RenderWindow->GetRenderers();
  rens->InitTraversal();
  ren = rens->GetNextItem();

  ren->ComputeVisiblePropBounds(bounds);

  this->Controller->Send(bounds, 6, 0, vtkCompositeManager::BOUNDS_TAG);
}

//-------------------------------------------------------------------------
void vtkCompositeManager::InitializePieces()
{
  vtkRendererCollection *rens;
  vtkRenderer *ren;
  vtkActorCollection *actors;
  vtkActor *actor;
  vtkMapper *mapper;
  vtkPolyDataMapper *pdMapper;
  int piece, numPieces;

  if (this->RenderWindow == NULL || this->Controller == NULL)
    {
    return;
    }
  piece = this->Controller->GetLocalProcessId();
  numPieces = this->Controller->GetNumberOfProcesses();

  rens = this->RenderWindow->GetRenderers();
  rens->InitTraversal();
  while ( (ren = rens->GetNextItem()) )
    {
    actors = ren->GetActors();
    actors->InitTraversal();
    while ( (actor = actors->GetNextItem()) )
      {
      mapper = actor->GetMapper();
      pdMapper = vtkPolyDataMapper::SafeDownCast(mapper);
      if (pdMapper)
        {
        pdMapper->SetPiece(piece);
        pdMapper->SetNumberOfPieces(numPieces);
        }
      }
    }
}

//-------------------------------------------------------------------------
void vtkCompositeManager::InitializeOffScreen()
{
  vtkDebugMacro("InitializeOffScreen");
  if (this->RenderWindow == NULL || this->Controller == NULL)
    {
    vtkDebugMacro("Missing object: Window = " << this->RenderWindow
                  << ", Controller = " << this->Controller);
    return;
    }
  
  // Do not make process 0 off screen.
  if (this->Controller->GetLocalProcessId() == 0)
    {
    vtkDebugMacro("Process 0.  Keep OnScreen.");
    return;
    }  

  this->RenderWindow->SetOffScreenRendering(1);
}

//-------------------------------------------------------------------------

void vtkCompositeManager::ResizeFloatArray(vtkFloatArray* fa, int numComp,
                                           vtkIdType size)
{
  fa->SetNumberOfComponents(numComp);

#ifdef MPIPROALLOC
  vtkIdType fa_size = fa->GetSize();
  if ( fa_size < size*numComp )
    {
    float* ptr = fa->GetPointer(0);
    if (ptr)
      {
      MPI_Free_mem(ptr);
      }
    char* tptr;
    MPI_Alloc_mem(size*numComp*sizeof(float), NULL, &tptr);
    ptr = (float*)tptr;
    fa->SetArray(ptr, size*numComp, 1);
    }
  else
    {
    fa->SetNumberOfTuples(size);
    }
#else
  fa->SetNumberOfTuples(size);
#endif
}

void vtkCompositeManager::ResizeUnsignedCharArray(vtkUnsignedCharArray* uca, 
                                                  int numComp, 
                                                  vtkIdType size)
{
  uca->SetNumberOfComponents(numComp);
#ifdef MPIPROALLOC
  vtkIdType uca_size = uca->GetSize();

  if ( uca_size < size*numComp )
    {
    unsigned char* ptr = uca->GetPointer(0);
    if (ptr)
      {
      MPI_Free_mem(ptr);
      }
    char* tptr;
    MPI_Alloc_mem(size*numComp*sizeof(unsigned char), NULL, &tptr);
    ptr = (unsigned char*)tptr;
    uca->SetArray(ptr, size*numComp, 1);
    }
  else
    {
    uca->SetNumberOfTuples(size);
    }
#else
  uca->SetNumberOfTuples(size);
#endif
}

void vtkCompositeManager::DeleteArray(vtkDataArray* da)
{
#ifdef MPIPROALLOC
  void* ptr = da->GetVoidPointer(0);
  if (ptr)
    {
    MPI_Free_mem(ptr);
    }
#endif
  da->Delete();
}

void vtkCompositeManager::SetUseChar(int useChar)
{
  if (useChar == this->UseChar)
    {
    return;
    }
  this->Modified();
  this->UseChar = useChar;

  // Cannot use float RGB (must be float RGBA).
  if (this->UseChar == 0)
    {
    this->UseRGB = 0;
    }
  
  this->ReallocPDataArrays();
}


void vtkCompositeManager::SetUseRGB(int useRGB)
{
  if (useRGB == this->UseRGB)
    {
    return;
    }
  this->Modified();
  this->UseRGB = useRGB;

  // Cannot use float RGB (must be char RGB).
  if (useRGB)
    {  
    this->UseChar = 1;
    }

  this->ReallocPDataArrays();
}

// Only reallocs arrays if they have been allocated already.
void vtkCompositeManager::ReallocPDataArrays()
{
  int numComps = 4;
  int numTuples = this->RendererSize[0] * this->RendererSize[1];

  if (this->UseRGB)
    {
    numComps = 3;
    }

  if (this->PData)
    {
    vtkCompositeManager::DeleteArray(this->PData);
    this->PData = NULL;
    } 
  if (this->LocalPData)
    {
    vtkCompositeManager::DeleteArray(this->LocalPData);
    this->LocalPData = NULL;
    } 

  if (this->UseChar)
    {
    this->PData = vtkUnsignedCharArray::New();
    vtkCompositeManager::ResizeUnsignedCharArray(
      static_cast<vtkUnsignedCharArray*>(this->PData),
      numComps, numTuples);
    this->LocalPData = vtkUnsignedCharArray::New();
    vtkCompositeManager::ResizeUnsignedCharArray(
      static_cast<vtkUnsignedCharArray*>(this->LocalPData),
      numComps, numTuples);
    }
  else 
    {
    this->PData = vtkFloatArray::New();
    vtkCompositeManager::ResizeFloatArray(
      static_cast<vtkFloatArray*>(this->PData),numComps, numTuples);
    this->LocalPData = vtkFloatArray::New();
    vtkCompositeManager::ResizeFloatArray(
      static_cast<vtkFloatArray*>(this->LocalPData),numComps, numTuples);
    }
}

void vtkCompositeManager::SetRendererSize(int x, int y)
{
  int numComps = 4;  
  
  // 3 for RGB,  4 for RGBA (RGB option only for char).
  if (this->UseRGB)
    {
    numComps = 3;
    }

  if (this->RendererSize[0] == x && this->RendererSize[1] == y)
    {
    return;
    }
  
  int numPixels = x * y;
  if (numPixels > 0)
    {
    if (this->UseChar)
      {
      if (!this->PData)
        {
        this->PData = vtkUnsignedCharArray::New();
        }
      vtkCompositeManager::ResizeUnsignedCharArray(
        static_cast<vtkUnsignedCharArray*>(this->PData), 
        numComps, numPixels);
      if (!this->LocalPData)
        {
        this->LocalPData = vtkUnsignedCharArray::New();
        }
      vtkCompositeManager::ResizeUnsignedCharArray(
        static_cast<vtkUnsignedCharArray*>(this->LocalPData), 
        numComps, numPixels);
      }
    else
      {
      if (!this->PData)
        {
        this->PData = vtkFloatArray::New();
        }
      vtkCompositeManager::ResizeFloatArray(
        static_cast<vtkFloatArray*>(this->PData), 
        numComps, numPixels);
      if (!this->LocalPData)
        {
        this->LocalPData = vtkFloatArray::New();
        }
      vtkCompositeManager::ResizeFloatArray(
        static_cast<vtkFloatArray*>(this->LocalPData), 
        numComps, numPixels);
      }

    if (!this->ZData)
      {
      this->ZData = vtkFloatArray::New();
      }
    vtkCompositeManager::ResizeFloatArray(this->ZData, 1, numPixels);
    if (!this->LocalZData)
      {
      this->LocalZData = vtkFloatArray::New();
      }
    vtkCompositeManager::ResizeFloatArray(this->LocalZData, 1, numPixels);
    }
  else
    {
    if (this->PData)
      {
      vtkCompositeManager::DeleteArray(this->PData);
      this->PData = NULL;
      }
    
    if (this->ZData)
      {
      vtkCompositeManager::DeleteArray(this->ZData);
      this->ZData = NULL;
      }

    if (this->LocalPData)
      {
      vtkCompositeManager::DeleteArray(this->LocalPData);
      this->LocalPData = NULL;
      }
    
    if (this->LocalZData)
      {
      vtkCompositeManager::DeleteArray(this->LocalZData);
      this->LocalZData = NULL;
      }
    }

  this->RendererSize[0] = x;
  this->RendererSize[1] = y;
}


//-------------------------------------------------------------------------
float vtkCompositeManager::GetZ(int x, int y)
{
  int idx;
  
  if (this->Controller == NULL || this->NumberOfProcesses == 1)
    {
    int *size = this->RenderWindow->GetSize();
    
    // Make sure we have default values.
    this->ReductionFactor = 1;
    this->SetRendererSize(size[0], size[1]);
    
    // Get the z buffer.
    this->RenderWindow->GetZbufferData(0,0,size[0]-1, size[1]-1, 
                                       this->LocalZData);
    }
  
  if (x < 0 || x >= this->RendererSize[0] || 
      y < 0 || y >= this->RendererSize[1])
    {
    return 0.0;
    }
  
  if (this->ReductionFactor > 1)
    {
    idx = (x + (y * this->RendererSize[0] / this->ReductionFactor)) 
             / this->ReductionFactor;
    }
  else 
    {
    idx = (x + (y * this->RendererSize[0]));
    }

  return this->LocalZData->GetValue(idx);
}

//----------------------------------------------------------------------------
void vtkCompositeManager::Composite()
{
  int myId;
  int front;
  
  // Stop the timer that has been timing the render.
  this->Timer->StopTimer();
  this->MaxRenderTime = this->Timer->GetElapsedTime();

  vtkTimerLog *timer = vtkTimerLog::New();
  
  myId = this->Controller->GetLocalProcessId();

  // Get the z buffer.
  timer->StartTimer();
  vtkTimerLog::MarkStartEvent("GetZBuffer");
  this->RenderWindow->GetZbufferData(0,0,
                                     this->RendererSize[0]-1, 
                                     this->RendererSize[1]-1,
                                     this->LocalZData);  
  vtkTimerLog::MarkEndEvent("GetZBuffer");

  // If we are process 0 and using double buffering, then we want 
  // to get the back buffer, otherwise we need to get the front.
  if (myId == 0)
    {
    front = 0;
    }
  else
    {
    front = 1;
    }

  // Get the pixel data.
  if (this->UseChar) 
    {
    if (this->LocalPData->GetNumberOfComponents() == 4)
      {
      vtkTimerLog::MarkStartEvent("Get RGBA Char Buffer");
      this->RenderWindow->GetRGBACharPixelData(
        0,0,this->RendererSize[0]-1,this->RendererSize[1]-1, 
        front,static_cast<vtkUnsignedCharArray*>(this->LocalPData));
      vtkTimerLog::MarkEndEvent("Get RGBA Char Buffer");
      }
    else if (this->LocalPData->GetNumberOfComponents() == 3)
      {
      vtkTimerLog::MarkStartEvent("Get RGB Char Buffer");
      this->RenderWindow->GetPixelData(
        0,0,this->RendererSize[0]-1,this->RendererSize[1]-1, 
        front,static_cast<vtkUnsignedCharArray*>(this->LocalPData));
      vtkTimerLog::MarkEndEvent("Get RGB Char Buffer");
      }
    } 
  else 
    {
    vtkTimerLog::MarkStartEvent("Get RGBA Float Buffer");
    this->RenderWindow->GetRGBAPixelData(
      0,0,this->RendererSize[0]-1,this->RendererSize[1]-1, 
      front,static_cast<vtkFloatArray*>(this->LocalPData));
    vtkTimerLog::MarkEndEvent("Get RGBA Float Buffer");
    }
  
  timer->StopTimer();
  this->GetBuffersTime = timer->GetElapsedTime();
  
  timer->StartTimer();
  
  // Let the subclass use its owns composite algorithm to
  // collect the results into "localPData" on process 0.
  vtkTimerLog::MarkStartEvent("Composite Buffers");
  this->Compositer->CompositeBuffer(this->LocalPData, this->LocalZData,
                                    this->PData, this->ZData);
    
  vtkTimerLog::MarkEndEvent("Composite Buffers");

  timer->StopTimer();
  this->CompositeTime = timer->GetElapsedTime();
    
  if (myId == 0) 
    {
    int windowSize[2];
    // Default value (no reduction).
    windowSize[0] = this->RendererSize[0];
    windowSize[1] = this->RendererSize[1];

    vtkDataArray* magPdata = 0;
    
    if (this->ReductionFactor > 1 && this->DoMagnifyBuffer)
      {
      // localPdata gets freed (new memory is allocated and returned.
      // windowSize get modified.
      if (this->UseChar)
        {
        magPdata = vtkUnsignedCharArray::New();
        }
      else
        {
        magPdata = vtkFloatArray::New();
        }
      magPdata->SetNumberOfComponents(
        this->LocalPData->GetNumberOfComponents());
      vtkTimerLog::MarkStartEvent("Magnify Buffer");
      this->MagnifyBuffer(this->LocalPData, magPdata, windowSize);
      vtkTimerLog::MarkEndEvent("Magnify Buffer");
      
      vtkRenderer* renderer =
        ((vtkRenderer*)
         this->RenderWindow->GetRenderers()->GetItemAsObject(0));
      renderer->SetViewport(0, 0, 1.0, 1.0);
      renderer->GetActiveCamera()->UpdateViewport(renderer);
      }

    
    timer->StartTimer();
    if (this->UseChar) 
      {
      vtkUnsignedCharArray* buf;
      if (magPdata)
        {
        buf = static_cast<vtkUnsignedCharArray*>(magPdata);
        }
      else
        {
        buf = static_cast<vtkUnsignedCharArray*>(this->LocalPData);
        }
      if (this->LocalPData->GetNumberOfComponents() == 4)
        {
        vtkTimerLog::MarkStartEvent("Set RGBA Char Buffer");
        this->RenderWindow->SetRGBACharPixelData(0, 0, windowSize[0]-1, 
                                  windowSize[1]-1, buf, 0);
        vtkTimerLog::MarkEndEvent("Set RGBA Char Buffer");
        }
      else if (this->LocalPData->GetNumberOfComponents() == 3)
        {
        vtkTimerLog::MarkStartEvent("Set RGB Char Buffer");
        this->RenderWindow->SetPixelData(0, 0, windowSize[0]-1, 
                                  windowSize[1]-1, buf, 0);
        vtkTimerLog::MarkEndEvent("Set RGB Char Buffer");
        }
      } 
    else 
      {
      if (magPdata)
        {
        vtkTimerLog::MarkStartEvent("Set RGBA Float Buffer");
        this->RenderWindow->SetRGBAPixelData(0, 0, windowSize[0]-1, 
                                windowSize[1]-1,
                                static_cast<vtkFloatArray*>(magPdata), 
                                0);
        vtkTimerLog::MarkEndEvent("Set RGBA Float Buffer");
        }
      else
        {
        vtkTimerLog::MarkStartEvent("Set RGBA Float Buffer");
        this->RenderWindow->SetRGBAPixelData(
          0, 0, windowSize[0]-1, windowSize[1]-1,
          static_cast<vtkFloatArray*>(this->LocalPData), 0);
        vtkTimerLog::MarkEndEvent("Set RGBA Float Buffer");
        }
      }
    timer->StopTimer();
    this->SetBuffersTime = timer->GetElapsedTime();

    if (magPdata)
      {
      magPdata->Delete();
      }
    }
  
  timer->Delete();
  timer = NULL;
}



//----------------------------------------------------------------------------
// We have do do this backward so we can keep it inplace.     
void vtkCompositeManager::MagnifyBuffer(vtkDataArray* localP, 
                                        vtkDataArray* magP,
                                        int windowSize[2])
{
  float *rowp, *subp;
  float *pp1;
  float *pp2;
  int   x, y, xi, yi;
  int   xInDim, yInDim;
  int   xOutDim, yOutDim;
  // Local increments for input.
  int   pInIncY; 
  float *newLocalPData;
  int numComp = localP->GetNumberOfComponents();
  
  xInDim = this->RendererSize[0];
  yInDim = this->RendererSize[1];
  xOutDim = windowSize[0] = this->ReductionFactor * this->RendererSize[0];
  yOutDim = windowSize[1] = this->ReductionFactor * this->RendererSize[1];
  
  magP->SetNumberOfComponents(numComp);
  magP->SetNumberOfTuples(xOutDim*yOutDim);
  newLocalPData = reinterpret_cast<float*>(magP->GetVoidPointer(0));
  float* localPdata = reinterpret_cast<float*>(localP->GetVoidPointer(0));

  if (this->UseChar)
    {
    if (numComp == 4)
      {
      // Get the last pixel.
      rowp = localPdata;
      pp2 = newLocalPData;
      for (y = 0; y < yInDim; y++)
        {
        // Duplicate the row rowp N times.
        for (yi = 0; yi < this->ReductionFactor; ++yi)
          {
          pp1 = rowp;
          for (x = 0; x < xInDim; x++)
            {
            // Duplicate the pixel p11 N times.
            for (xi = 0; xi < this->ReductionFactor; ++xi)
              {
              *pp2++ = *pp1;
              }
            ++pp1;
            }
          }
        rowp += xInDim;
        }
      }
    else if (numComp == 3)
      { // RGB char pixel data.
      // Get the last pixel.
      pInIncY = numComp * xInDim;
      unsigned char* crowp = reinterpret_cast<unsigned char*>(localPdata);
      unsigned char* cpp2 = reinterpret_cast<unsigned char*>(newLocalPData);
      unsigned char *cpp1, *csubp;
      for (y = 0; y < yInDim; y++)
        {
        // Duplicate the row rowp N times.
        for (yi = 0; yi < this->ReductionFactor; ++yi)
          {
          cpp1 = crowp;
          for (x = 0; x < xInDim; x++)
            {
            // Duplicate the pixel p11 N times.
            for (xi = 0; xi < this->ReductionFactor; ++xi)
              {
              csubp = cpp1;
              *cpp2++ = *csubp++;
              *cpp2++ = *csubp++;
              *cpp2++ = *csubp;
              }
            cpp1 += numComp;
            }
          }
        crowp += pInIncY;
        }
      }
    }
  else
    {
    // Get the last pixel.
    pInIncY = numComp * xInDim;
    rowp = localPdata;
    pp2 = newLocalPData;
    for (y = 0; y < yInDim; y++)
      {
      // Duplicate the row rowp N times.
      for (yi = 0; yi < this->ReductionFactor; ++yi)
        {
        pp1 = rowp;
        for (x = 0; x < xInDim; x++)
          {
          // Duplicate the pixel p11 N times.
          for (xi = 0; xi < this->ReductionFactor; ++xi)
            {
            subp = pp1;
            if (numComp == 4)
              {
              *pp2++ = *subp++;
              }
            *pp2++ = *subp++;
            *pp2++ = *subp++;
            *pp2++ = *subp;
            }
          pp1 += numComp;
          }
        }
      rowp += pInIncY;
      }
    }
  
}
  

//----------------------------------------------------------------------------
void vtkCompositeManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  
  os << indent << "ReductionFactor: " << this->ReductionFactor << endl;
  if (this->UseChar)
    {
    os << indent << "UseChar: On\n";
    }
  else
    {
    os << indent << "UseChar: Off\n";
    }  
  
  if (this->UseRGB)
    {
    os << indent << "UseRGB: On\n";
    }
  else
    {
    os << indent << "UseRGB: Off\n";
    }  
  
  if ( this->RenderWindow )
    {
    os << indent << "RenderWindow: " << this->RenderWindow << "\n";
    }
  else
    {
    os << indent << "RenderWindow: (none)\n";
    }

  if (this->DoMagnifyBuffer)
    {
    os << indent << "DoMagnifyBuffer: On\n";
    }
  else
    {
    os << indent << "DoMagnifyBuffer: Off\n";
    }  
  
  os << indent << "SetBuffersTime: " << this->SetBuffersTime << "\n";
  os << indent << "GetBuffersTime: " << this->GetGetBuffersTime() << "\n";
  os << indent << "CompositeTime: " << this->CompositeTime << "\n";
  os << indent << "MaxRenderTime: " << this->MaxRenderTime << "\n";
  if (this->UseCompositing)
    {
    os << indent << "UseCompositing: On\n";
    }
  else
    {
    os << indent << "UseCompositing: Off\n";
    }

  if (this->Manual)
    {
    os << indent << "Manual: On\n";
    }
  else
    {
    os << indent << "Manual: Off\n";
    }

  os << indent << "Controller: (" << this->Controller << ")\n"; 
  if (this->Compositer)
    {
    os << indent << "Compositer: " << this->Compositer->GetClassName() 
       << " (" << this->Compositer << ")\n"; 
    }
  else
    {
    os << indent << "Compositer: NULL\n";
    }
  os << indent << "NumberOfProcesses: " << this->NumberOfProcesses << endl;
}



