// -*- c++ -*-

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

=========================================================================*/

#include "vtkDesktopDeliveryServer.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRendererCollection.h"
#include "vtkRenderer.h"
#include "vtkUnsignedCharArray.h"
#include "vtkFloatArray.h"
#include "vtkCamera.h"
#include "vtkLight.h"
#include "vtkTimerLog.h"
#include "vtkLightCollection.h"
#include "vtkCallbackCommand.h"
#include "vtkVersion.h"
#include "vtkMultiProcessController.h"

static void SatelliteStartRender(vtkObject *caller,
         unsigned long vtkNotUsed(event),
         void *clientData, void *);
static void SatelliteEndRender(vtkObject *caller,
             unsigned long vtkNotUsed(event),
             void *clientData, void *);
static void SatelliteStartParallelRender(vtkObject *caller,
           unsigned long vtkNotUsed(event),
           void *clientData, void *);
static void SatelliteEndParallelRender(vtkObject *caller,
               unsigned long vtkNotUsed(event),
               void *clientData, void *);

vtkCxxRevisionMacro(vtkDesktopDeliveryServer, "$Revision$");
vtkStandardNewMacro(vtkDesktopDeliveryServer);

vtkDesktopDeliveryServer::vtkDesktopDeliveryServer()
{
    this->ParallelRenderManager = NULL;
    this->RemoteDisplay = 1;
}

vtkDesktopDeliveryServer::~vtkDesktopDeliveryServer()
{
  this->SetParallelRenderManager(NULL);
}

void
vtkDesktopDeliveryServer::SetController(vtkMultiProcessController *controller)
{
  vtkDebugMacro("SetController");

  if (controller && (controller->GetNumberOfProcesses() != 2))
    {
    vtkErrorMacro("vtkDesktopDelivery needs controller with 2 processes");
    return;
    }

  this->Superclass::SetController(controller);

  if (this->Controller)
    {
    this->RootProcessId = 1 - this->Controller->GetLocalProcessId();
    }
}

void vtkDesktopDeliveryServer
    ::SetParallelRenderManager(vtkParallelRenderManager *prm)
{
  if (this->ParallelRenderManager == prm)
    {
    return;
    }
  this->Modified();

  if (this->ParallelRenderManager)
    {
    // Remove all observers.
    this->ParallelRenderManager->RemoveObserver(this->StartParallelRenderTag);
    this->ParallelRenderManager->RemoveObserver(this->EndParallelRenderTag);

    // Delete the reference.
    this->ParallelRenderManager->UnRegister(this);
    this->ParallelRenderManager = NULL;
    }

  this->ParallelRenderManager = prm;
  if (this->ParallelRenderManager)
    {
    // Create a reference.
    this->ParallelRenderManager->Register(this);

    // Attach observers.
    vtkCallbackCommand *cbc;

    cbc = vtkCallbackCommand::New();
    cbc->SetCallback(::SatelliteStartParallelRender);
    cbc->SetClientData((void *)this);
    this->StartParallelRenderTag
      = this->ParallelRenderManager->AddObserver(vtkCommand::StartEvent, cbc);
    // ParallelRenderManager will really delete cbc when observer is removed.
    cbc->Delete();

    cbc = vtkCallbackCommand::New();
    cbc->SetCallback(::SatelliteEndParallelRender);
    cbc->SetClientData((void *)this);
    this->EndParallelRenderTag
      = this->ParallelRenderManager->AddObserver(vtkCommand::EndEvent, cbc);
    // ParallelRenderManager will really delete cbc when observer is removed.
    cbc->Delete();

    // Remove observers to RenderWindow.  We use the prm instead.
    if (this->ObservingRenderWindow)
      {
      this->RenderWindow->RemoveObserver(this->StartRenderTag);
      this->RenderWindow->RemoveObserver(this->EndRenderTag);
      this->ObservingRenderWindow = false;
      }
    }
  else
    {
    // Apparently we added and then removed a ParallelRenderManager.
    // Restore RenderWindow observers.
    if (this->RenderWindow)
      {
      vtkCallbackCommand *cbc;
        
      this->ObservingRenderWindow = true;
        
      cbc= vtkCallbackCommand::New();
      cbc->SetCallback(::SatelliteStartRender);
      cbc->SetClientData((void*)this);
      this->StartRenderTag
  = this->RenderWindow->AddObserver(vtkCommand::StartEvent,cbc);
      // renWin will delete the cbc when the observer is removed.
      cbc->Delete();
        
      cbc = vtkCallbackCommand::New();
      cbc->SetCallback(::SatelliteEndRender);
      cbc->SetClientData((void*)this);
      this->EndRenderTag
  = this->RenderWindow->AddObserver(vtkCommand::EndEvent,cbc);
      // renWin will delete the cbc when the observer is removed.
      cbc->Delete();
      }
    }
}

void vtkDesktopDeliveryServer::SetRenderWindow(vtkRenderWindow *renWin)
{
  this->Superclass::SetRenderWindow(renWin);

  if (this->ObservingRenderWindow && this->ParallelRenderManager)
    {
    // Don't need the observers we just attached.
    this->RenderWindow->RemoveObserver(this->StartRenderTag);
    this->RenderWindow->RemoveObserver(this->EndRenderTag);
    this->ObservingRenderWindow = false;
    }
}

void vtkDesktopDeliveryServer::PreRenderProcessing()
{
  vtkDebugMacro("PreRenderProcessing");

  // Send remote display flag.
  this->Controller->Send(&this->RemoteDisplay, 1, this->RootProcessId,
       vtkDesktopDeliveryServer::REMOTE_DISPLAY_TAG);

  if (this->ParallelRenderManager)
    {
    // If we are chaining this to another parallel render manager, restore
    // the renderers so that the other parallel render manager may render
    // the desired image size.
    if (this->ImageReductionFactor > 1)
      {
      vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
      vtkRenderer *ren;
      int i;
      for (rens->InitTraversal(), i = 0; ren = rens->GetNextItem(); i++)
  {
  float *viewport = ren->GetViewport();
  ren->SetViewport(viewport[0]*this->ImageReductionFactor,
       viewport[1]*this->ImageReductionFactor,
       viewport[2]*this->ImageReductionFactor,
       viewport[3]*this->ImageReductionFactor);
  }
      }

    // Make sure the prm has the correct image reduction factor.
    if (  this->ParallelRenderManager->GetImageReductionFactor()
  < this->ImageReductionFactor)
      {
      this->ParallelRenderManager
  ->SetMaxImageReductionFactor(this->ImageReductionFactor);
      }
    this->ParallelRenderManager
      ->SetImageReductionFactor(this->ImageReductionFactor);
    this->ParallelRenderManager->AutoImageReductionFactorOff();
    }
}

void vtkDesktopDeliveryServer::PostRenderProcessing()
{
  vtkDebugMacro("PostRenderProcessing");

  this->Controller->Barrier();
  if (this->RemoteDisplay)
    {
    this->ReadReducedImage();

    this->Controller->Send(this->ReducedImage->GetPointer(0),
         3*this->ReducedImage->GetNumberOfTuples(),
         this->RootProcessId,
         vtkDesktopDeliveryServer::IMAGE_TAG);

    }

  // Send timing metics
  vtkDesktopDeliveryServer::TimingMetrics tm;
  if (this->ParallelRenderManager)
    {
    tm.ImageProcessingTime
      = this->ParallelRenderManager->GetImageProcessingTime();
    }
  else
    {
    tm.ImageProcessingTime = 0.0;
    }

  this->Controller->Send((double *)(&tm),
       vtkDesktopDeliveryServer::TIMING_METRICS_SIZE,
       this->RootProcessId,
       vtkDesktopDeliveryServer::TIMING_METRICS_TAG);

  // If another parallel render manager has already made an image, don't
  // clober it.
  if (this->ParallelRenderManager)
    {
    this->RenderWindowImageUpToDate = true;
    }
}

void vtkDesktopDeliveryServer::SetRenderWindowSize()
{
  if (this->RemoteDisplay)
    {
    this->Superclass::SetRenderWindowSize();
    }
  else
    {
    int *size = this->RenderWindow->GetSize();
    this->FullImageSize[0] = size[0];
    this->FullImageSize[1] = size[1];
    this->ReducedImageSize[0] = size[0]/this->ImageReductionFactor;
    this->ReducedImageSize[1] = size[1]/this->ImageReductionFactor;
    }
}

void vtkDesktopDeliveryServer::ReadReducedImage()
{
  if (this->ParallelRenderManager)
    {
    int *size = this->ParallelRenderManager->GetReducedImageSize();
    if (   (this->ReducedImageSize[0] != size[0])
  || (this->ReducedImageSize[1] != size[1]) )
      {
      vtkWarningMacro("Coupled parallel render manager reports unexpected reduced image size");
      }
    this->ParallelRenderManager->GetReducedPixelData(this->ReducedImage);
    this->ReducedImageUpToDate = true;
    }
  else
    {
    this->Superclass::ReadReducedImage();
    }
}

void vtkDesktopDeliveryServer::LocalComputeVisiblePropBounds(vtkRenderer *ren,
                   float bounds[6])
{
  if (this->ParallelRenderManager)
    {
    this->ParallelRenderManager->ComputeVisiblePropBounds(ren, bounds);
    }
  else
    {
    this->Superclass::LocalComputeVisiblePropBounds(ren, bounds);
    }
}

void vtkDesktopDeliveryServer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "ParallelRenderManager: "
     << this->ParallelRenderManager << endl;
  os << indent << "RemoteDisplay: "
     << (this->RemoteDisplay ? "on" : "off") << endl;
}


static void SatelliteStartRender(vtkObject *caller,
         unsigned long vtkNotUsed(event),
         void *clientData, void *)
{
  vtkDesktopDeliveryServer *self = (vtkDesktopDeliveryServer *)clientData;
  if (caller != self->GetRenderWindow())
    {
    vtkGenericWarningMacro("vtkDesktopDeliveryServer caller mismatch");
    return;
    }
  self->SatelliteStartRender();
}
static void SatelliteEndRender(vtkObject *caller,
             unsigned long vtkNotUsed(event),
             void *clientData, void *)
{
  vtkDesktopDeliveryServer *self = (vtkDesktopDeliveryServer *)clientData;
  if (caller != self->GetRenderWindow())
    {
    vtkGenericWarningMacro("vtkDesktopDeliveryServer caller mismatch");
    return;
    }
  self->SatelliteEndRender();
}

static void SatelliteStartParallelRender(vtkObject *caller,
           unsigned long vtkNotUsed(event),
           void *clientData, void *)
{
  vtkDesktopDeliveryServer *self = (vtkDesktopDeliveryServer *)clientData;
  if (caller != self->GetParallelRenderManager())
    {
    vtkGenericWarningMacro("vtkDesktopDeliveryServer caller mismatch");
    return;
    }
  self->SatelliteStartRender();
}
static void SatelliteEndParallelRender(vtkObject *caller,
               unsigned long vtkNotUsed(event),
               void *clientData, void *)
{
  vtkDesktopDeliveryServer *self = (vtkDesktopDeliveryServer *)clientData;
  if (caller != self->GetParallelRenderManager())
    {
    vtkGenericWarningMacro("vtkDesktopDeliveryServer caller mismatch");
    return;
    }
  self->SatelliteEndRender();
}
