/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  Copyright 2003 Sandia Corporation. Under the terms of Contract
  DE-AC04-94AL85000, there is a non-exclusive license for use of this work by
  or on behalf of the U.S. Government. Redistribution and use in source and
  binary forms, with or without modification, are permitted provided that this
  Notice and any statement of authorship are reproduced on all copies.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkParallelRenderManager.h"

#include "vtkMultiProcessController.h"
#include "vtkCallbackCommand.h"
#include "vtkActorCollection.h"
#include "vtkActor.h"
#include "vtkPolyDataMapper.h"
#include "vtkCamera.h"
#include "vtkDoubleArray.h"
#include "vtkLightCollection.h"
#include "vtkLight.h"
#include "vtkMath.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkRendererCollection.h"
#include "vtkTimerLog.h"
#include "vtkUnsignedCharArray.h"

static void AbortRenderCheck(vtkObject *caller, unsigned long vtkNotUsed(event),
                 void *clientData, void *);
static void StartRender(vtkObject *caller, unsigned long vtkNotUsed(event),
            void *clientData, void *);
static void EndRender(vtkObject *caller, unsigned long vtkNotUsed(event),
            void *clientData, void *);
static void SatelliteStartRender(vtkObject *caller,
                unsigned long vtkNotUsed(event),
                void *clientData, void *);
static void SatelliteEndRender(vtkObject *caller,
                  unsigned long vtkNotUsed(event),
                  void *clientData, void *);
/*
static void ResetCamera(vtkObject *caller,
                        unsigned long vtkNotUsed(event),
                        void *clientData, void *);
static void ResetCameraClippingRange(vtkObject *caller,
                                     unsigned long vtkNotUsed(event),
                                     void *clientData, void *);
*/
static void RenderRMI(void *arg, void *, int, int);
static void ComputeVisiblePropBoundsRMI(void *arg, void *, int, int);
const int vtkParallelRenderManager::WIN_INFO_INT_SIZE = 
  sizeof(vtkParallelRenderManager::RenderWindowInfoInt)/sizeof(int);
const int vtkParallelRenderManager::WIN_INFO_DOUBLE_SIZE =
  sizeof(vtkParallelRenderManager::RenderWindowInfoDouble)/sizeof(double);
const int vtkParallelRenderManager::REN_INFO_INT_SIZE =
  sizeof(vtkParallelRenderManager::RendererInfoInt)/sizeof(int);
const int vtkParallelRenderManager::REN_INFO_DOUBLE_SIZE =
  sizeof(vtkParallelRenderManager::RendererInfoDouble)/sizeof(double);
const int vtkParallelRenderManager::LIGHT_INFO_DOUBLE_SIZE =
  sizeof(vtkParallelRenderManager::LightInfoDouble)/sizeof(double);

vtkCxxRevisionMacro(vtkParallelRenderManager, "$Revision$");

vtkParallelRenderManager::vtkParallelRenderManager()
{
  this->RenderWindow = NULL;
  this->ObservingRenderWindow = 0;
  this->ObservingRenderer = 0;
  this->ObservingAbort = 0;

  this->Controller = NULL;
  this->RootProcessId = 0;

  this->Lock = 0;

  this->ImageReductionFactor = 1;
  this->MaxImageReductionFactor = 16;
  this->AutoImageReductionFactor = 0;
  this->AverageTimePerPixel = 0.0;

  this->RenderTime = 0.0;
  this->ImageProcessingTime = 0.0;

  this->ParallelRendering = 1;
  this->WriteBackImages = 1;
  this->MagnifyImages = 1;
  this->MagnifyImageMethod = vtkParallelRenderManager::NEAREST;
  this->RenderEventPropagation = 1;
  this->UseCompositing = 1;

  this->FullImage = vtkUnsignedCharArray::New();
  this->ReducedImage = vtkUnsignedCharArray::New();
  this->FullImageUpToDate = 0;
  this->ReducedImageUpToDate = 0;
  this->RenderWindowImageUpToDate = 0;
  this->FullImageSize[0] = 0;
  this->FullImageSize[1] = 0;

  this->Viewports = vtkDoubleArray::New();
  this->Viewports->SetNumberOfComponents(4);

  this->Timer = vtkTimerLog::New();
}

vtkParallelRenderManager::~vtkParallelRenderManager()
{
  this->SetRenderWindow(NULL);
  this->SetController(NULL);
  this->FullImage->Delete();
  this->ReducedImage->Delete();
  this->Viewports->Delete();
  this->Timer->Delete();
}

void vtkParallelRenderManager::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "ParallelRendering: "
     << (this->ParallelRendering ? "on" : "off") << endl;
  os << indent << "RenderEventPropagation: "
     << (this->RenderEventPropagation ? "on" : "off") << endl;
  os << indent << "UseCompositing: "
     << (this->UseCompositing ? "on" : "off") << endl;

  os << indent << "ObservingRendererWindow: "
     << (this->ObservingRenderWindow ? "yes" : "no") << endl;
  os << indent << "ObservingRenderer: "
     << (this->ObservingRenderer ? "yes" : "no") << endl;
  os << indent << "Locked: " << (this->Lock ? "yes" : "no") << endl;

  os << indent << "ImageReductionFactor: "
     << this->ImageReductionFactor << endl;
  os << indent << "MaxImageReductionFactor: "
     << this->MaxImageReductionFactor << endl;
  os << indent << "AutoImageReductionFactor: "
     << (this->AutoImageReductionFactor ? "on" : "off") << endl;

  if (this->MagnifyImageMethod == vtkParallelRenderManager::LINEAR)
    {
    os << indent << "MagnifyImageMethod: LINEAR\n";
    }
  else if (this->MagnifyImageMethod == vtkParallelRenderManager::NEAREST)
    {
    os << indent << "MagnifyImageMethod: NEAREST\n";
    }

  os << indent << "WriteBackImages: "
     << (this->WriteBackImages ? "on" : "off") << endl;
  os << indent << "MagnifyImages: "
     << (this->MagnifyImages ? "on" : "off") << endl;

  os << indent << "FullImageSize: ("
     << this->FullImageSize[0] << ", " << this->FullImageSize[1] << ")" << endl;
  os << indent << "ReducedImageSize: ("
     << this->ReducedImageSize[0] << ", "
     << this->ReducedImageSize[1] << ")" << endl;

  os << indent << "RenderWindow: " << this->RenderWindow << endl;
  os << indent << "Controller: " << this->Controller << endl;
  os << indent << "RootProcessId: " << this->RootProcessId << endl;

  os << indent << "Last render time: " << this->GetRenderTime() << endl;
  //os << indent << "ImageProcessingTime:\n ";
  os << indent << "Last image processing time: "
     << this->GetImageProcessingTime() << endl;
}

vtkRenderWindow *vtkParallelRenderManager::MakeRenderWindow()
{
  vtkDebugMacro("MakeRenderWindow");

  return vtkRenderWindow::New();
}

vtkRenderer *vtkParallelRenderManager::MakeRenderer()
{
  vtkDebugMacro("MakeRenderer");

  return vtkRenderer::New();
}

void vtkParallelRenderManager::SetRenderWindow(vtkRenderWindow *renWin)
{
  vtkDebugMacro("SetRenderWindow");

  vtkRendererCollection *rens;
  vtkRenderer *ren;

  if (this->RenderWindow == renWin)
    {
    return;
    }
  this->Modified();

  if (this->RenderWindow)
    {
    // Remove all of the observers.
    if (this->ObservingRenderWindow)
      {
      rens = this->RenderWindow->GetRenderers();
      rens->InitTraversal();
      ren = rens->GetNextItem();
      if (ren)
        {
        ren->RemoveObserver(this->StartRenderTag);
        ren->RemoveObserver(this->EndRenderTag);
        }

      // Will make do with first renderer. (Assumes renderer does not change.)
      if (this->ObservingRenderer)
        {
        rens = this->RenderWindow->GetRenderers();
        rens->InitTraversal();
        ren = rens->GetNextItem();
        if (ren)
          {
          //ren->RemoveObserver(this->ResetCameraTag);
          //ren->RemoveObserver(this->ResetCameraClippingRangeTag);
          }
        this->ObservingRenderer = 0;
        }

      this->ObservingRenderWindow = 0;
      }
    if (this->ObservingAbort)
      {
      this->RenderWindow->RemoveObserver(this->AbortRenderCheckTag);
      this->ObservingAbort = 0;
      }

    // Delete the reference.
    this->RenderWindow->UnRegister(this);
    this->RenderWindow = NULL;
    }

  this->RenderWindow = renWin;
  if (this->RenderWindow)
    {
    vtkCallbackCommand *cbc;

    this->RenderWindow->Register(this);

    // In case a subclass wants to raise aborts.
    cbc = vtkCallbackCommand::New();
    cbc->SetCallback(::AbortRenderCheck);
    cbc->SetClientData((void*)this);
    // renWin will delete the cbc when the observer is removed.
    this->AbortRenderCheckTag = renWin->AddObserver(vtkCommand::AbortCheckEvent,
      cbc);
    cbc->Delete();
    this->ObservingAbort = 1;

    if (this->Controller)
      {
      if (this->Controller->GetLocalProcessId() == this->RootProcessId)
        {
        rens = this->RenderWindow->GetRenderers();
        rens->InitTraversal();
        ren = rens->GetNextItem();
        if (ren)
          {
        this->ObservingRenderWindow = 1;

        cbc = vtkCallbackCommand::New();
        cbc->SetCallback(::StartRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
          this->StartRenderTag = ren->AddObserver(vtkCommand::StartEvent,cbc);
        cbc->Delete();

        cbc = vtkCallbackCommand::New();
        cbc->SetCallback(::EndRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
          this->EndRenderTag = ren->AddObserver(vtkCommand::EndEvent,cbc);
        cbc->Delete();

          this->ObservingRenderer = 1;

          //cbc = vtkCallbackCommand::New();
          //cbc->SetCallback(::ResetCameraClippingRange);
          //cbc->SetClientData((void*)this);
          // ren will delete the cbc when the observer is removed.
          //this->ResetCameraClippingRangeTag = 
          //ren->AddObserver(vtkCommand::ResetCameraClippingRangeEvent,cbc);
          //cbc->Delete();

          //cbc = vtkCallbackCommand::New();
          //cbc->SetCallback(::ResetCamera);
          //cbc->SetClientData((void*)this);
          // ren will delete the cbc when the observer is removed.
          //this->ResetCameraTag =
          //ren->AddObserver(vtkCommand::ResetCameraEvent,cbc);
          //cbc->Delete();
          }
        }
      else // LocalProcessId != RootProcessId
        {
        rens = this->RenderWindow->GetRenderers();
        rens->InitTraversal();
        ren = rens->GetNextItem();
        if (ren)
          {
        this->ObservingRenderWindow = 1;

        cbc= vtkCallbackCommand::New();
        cbc->SetCallback(::SatelliteStartRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
          this->StartRenderTag = ren->AddObserver(vtkCommand::StartEvent,cbc);
        cbc->Delete();

        cbc = vtkCallbackCommand::New();
        cbc->SetCallback(::SatelliteEndRender);
        cbc->SetClientData((void*)this);
        // renWin will delete the cbc when the observer is removed.
          this->EndRenderTag = ren->AddObserver(vtkCommand::EndEvent,cbc);
        cbc->Delete();
        }
      }
    }
    }
}

void vtkParallelRenderManager::SetController(vtkMultiProcessController *controller)
{
  vtkDebugMacro("SetController");

  if (this->Controller == controller)
    {
    return;
    }
  this->Controller = controller;
  this->Modified();

  // We've changed the controller.  This may change how observers are attached
  // to the render window.
  if (this->RenderWindow)
    {
    vtkRenderWindow *saveRenWin = this->RenderWindow;
    saveRenWin->Register(this);
    this->SetRenderWindow(NULL);
    this->SetRenderWindow(saveRenWin);
    saveRenWin->UnRegister(this);
    }
}

void vtkParallelRenderManager::InitializePieces()
{
  vtkDebugMacro("InitializePieces");

  vtkRendererCollection *rens;
  vtkRenderer *ren;
  vtkActorCollection *actors;
  vtkActor *actor;
  vtkMapper *mapper;
  vtkPolyDataMapper *pdMapper;
  int piece, numPieces;

  if ((this->RenderWindow == NULL) || (this->Controller == NULL))
    {
    vtkWarningMacro("Called InitializePieces before setting RenderWindow or Controller");
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

void vtkParallelRenderManager::InitializeOffScreen()
{
  vtkDebugMacro("InitializeOffScreen");

  if ((this->RenderWindow == NULL) || (this->Controller == NULL))
    {
    vtkWarningMacro("Called InitializeOffScreen before setting RenderWindow or Controller");
    return;
    }

  if ( (this->Controller->GetLocalProcessId() != this->RootProcessId) ||
       !this->WriteBackImages )
    {
    this->RenderWindow->OffScreenRenderingOn();
    }
  else
    {
    this->RenderWindow->OffScreenRenderingOff();
    }
}

void vtkParallelRenderManager::StartInteractor()
{
  vtkDebugMacro("StartInteractor");

  if ((this->Controller == NULL) || (this->RenderWindow == NULL))
    {
    vtkErrorMacro("Must set Controller and RenderWindow before starting interactor.");
    return;
    }

  if (this->Controller->GetLocalProcessId() == this->RootProcessId)
    {
    vtkRenderWindowInteractor *inter = this->RenderWindow->GetInteractor();
    if (!inter)
      {
      vtkErrorMacro("Render window does not have an interactor.");
      }
    else
      {
      inter->Initialize();
      inter->Start();
      }
    //By the time we reach here, the interaction is finished.
    this->StopServices();
    }
  else
    {
    this->StartService();
    }
}

void vtkParallelRenderManager::StartService()
{
  vtkDebugMacro("StartService");
  
  if (!this->Controller)
    {
    vtkErrorMacro("Must set Controller before starting service");
    return;
    }
  if (this->Controller->GetLocalProcessId() == this->RootProcessId)
    {
    vtkWarningMacro("Starting service on root process (probably not what you wanted to do)");
    }

  this->InitializeRMIs();
  this->Controller->ProcessRMIs();
}

void vtkParallelRenderManager::StopServices()
{
  vtkDebugMacro("StopServices");

  if (!this->Controller)
    {
    vtkErrorMacro("Must set Controller before stopping service");
    return;
    }
  if (this->Controller->GetLocalProcessId() != this->RootProcessId)
    {
    vtkErrorMacro("Can only stop services on root node");
    return;
    }

  int numProcs = this->Controller->GetNumberOfProcesses();
  for (int id = 0; id < numProcs; id++)
    {
    if (id == this->RootProcessId) continue;
    this->Controller->TriggerRMI(id,vtkMultiProcessController::BREAK_RMI_TAG);
    }
}

void vtkParallelRenderManager::StartRender()
{
  vtkParallelRenderManager::RenderWindowInfoInt winInfoInt;
  vtkParallelRenderManager::RenderWindowInfoDouble winInfoDouble;
  vtkParallelRenderManager::RendererInfoInt renInfoInt;
  vtkParallelRenderManager::RendererInfoDouble renInfoDouble;
  vtkParallelRenderManager::LightInfoDouble lightInfoDouble;

  vtkDebugMacro("StartRender");

  if ((this->Controller == NULL) || (this->Lock))
    {
    return;
    }
  this->Lock = 1;

  this->FullImageUpToDate = 0;
  this->ReducedImageUpToDate = 0;
  this->RenderWindowImageUpToDate = 0;

  if (this->FullImage->GetPointer(0) == this->ReducedImage->GetPointer(0))
    {
    // "Un-share" pointer for full/reduced images in case we need separate
    // arrays this run.
    this->ReducedImage->Initialize();
    }

  if (!this->ParallelRendering)
    {
    this->Lock = 0;
    return;
    }

  this->InvokeEvent(vtkCommand::StartEvent, NULL);

  // Used to time the total render (without compositing).
  this->Timer->StartTimer();

  if (this->AutoImageReductionFactor)
    {
    this->SetImageReductionFactorForUpdateRate(
      this->RenderWindow->GetDesiredUpdateRate());
    }

  int id;
  int numProcs = this->Controller->GetNumberOfProcesses();

  // Make adjustments for window size.
  int *size = this->RenderWindow->GetSize();
  if ((size[0] == 0) || (size[1] == 0))
    {
    // It helps to have a real window size.
    vtkDebugMacro("Resetting window size to 300x300");
    size[0] = size[1] = 300;
    this->RenderWindow->SetSize(size[0], size[1]);
    }
  this->FullImageSize[0] = size[0];
  this->FullImageSize[1] = size[1];
  //Round up.
  this->ReducedImageSize[0] =
    (size[0]+this->ImageReductionFactor-1)/this->ImageReductionFactor;
  this->ReducedImageSize[1] =
    (size[1]+this->ImageReductionFactor-1)/this->ImageReductionFactor;

  // Collect and distribute information about current state of RenderWindow
  vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
  winInfoInt.FullSize[0] = this->FullImageSize[0];
  winInfoInt.FullSize[1] = this->FullImageSize[1];
  winInfoInt.ReducedSize[0] = this->ReducedImageSize[0];
  winInfoInt.ReducedSize[1] = this->ReducedImageSize[1];
//  winInfoInt.NumberOfRenderers = rens->GetNumberOfItems();
  winInfoInt.NumberOfRenderers = 1;
  winInfoInt.ImageReductionFactor = this->ImageReductionFactor;
  winInfoInt.UseCompositing = this->UseCompositing;
  winInfoDouble.DesiredUpdateRate = this->RenderWindow->GetDesiredUpdateRate();

  for (id = 0; id < numProcs; id++)
    {
    if (id == this->RootProcessId)
      {
      continue;
      }
    if (this->RenderEventPropagation)
      {
      this->Controller->TriggerRMI(id, NULL, 0,
                   vtkParallelRenderManager::RENDER_RMI_TAG);
      }
    this->Controller->Send((int *)(&winInfoInt), 
                           vtkParallelRenderManager::WIN_INFO_INT_SIZE, 
                           id,
                           vtkParallelRenderManager::WIN_INFO_INT_TAG);
    this->Controller->Send((double *)(&winInfoDouble), 
                           vtkParallelRenderManager::WIN_INFO_DOUBLE_SIZE,
                           id, 
                           vtkParallelRenderManager::WIN_INFO_DOUBLE_TAG);
    this->SendWindowInformation();
    }

  if (this->ImageReductionFactor > 1)
    {
    this->Viewports->SetNumberOfTuples(rens->GetNumberOfItems());
    }
  vtkRenderer *ren;
  rens->InitTraversal();
  ren = rens->GetNextItem();
  
  if (ren)
    {
    ren->GetViewport(renInfoDouble.Viewport);

    // Adjust Renderer viewports to get reduced size image.
    if (this->ImageReductionFactor > 1)
      {
      this->Viewports->SetTuple(0, renInfoDouble.Viewport);
      renInfoDouble.Viewport[0] /= this->ImageReductionFactor;
      renInfoDouble.Viewport[1] /= this->ImageReductionFactor;
      renInfoDouble.Viewport[2] /= this->ImageReductionFactor;
      renInfoDouble.Viewport[3] /= this->ImageReductionFactor;
      ren->SetViewport(renInfoDouble.Viewport);
      }

    vtkCamera *cam = ren->GetActiveCamera();
    cam->GetPosition(renInfoDouble.CameraPosition);
    cam->GetFocalPoint(renInfoDouble.CameraFocalPoint);
    cam->GetViewUp(renInfoDouble.CameraViewUp);
    cam->GetClippingRange(renInfoDouble.CameraClippingRange);
    ren->GetBackground(renInfoDouble.Background);
    if (cam->GetParallelProjection())
      {
      renInfoDouble.ParallelScale = cam->GetParallelScale();
      }
    else
      {
      renInfoDouble.ParallelScale = 0.0;
      }
    vtkLightCollection *lc = ren->GetLights();
    renInfoInt.NumberOfLights = lc->GetNumberOfItems();

    for (id = 0; id < numProcs; id++)
      {
      if (id == this->RootProcessId)
        {
        continue;
        }
      this->Controller->Send((int *)(&renInfoInt), 
                             vtkParallelRenderManager::REN_INFO_INT_SIZE, 
                             id,
                             vtkParallelRenderManager::REN_INFO_INT_TAG);
      this->Controller->Send((double *)(&renInfoDouble), 
                             vtkParallelRenderManager::REN_INFO_DOUBLE_SIZE,
                             id, 
                             vtkParallelRenderManager::REN_INFO_DOUBLE_TAG);
      }

    vtkLight *light;
    for (lc->InitTraversal(); (light = lc->GetNextItem()); )
      {
      lightInfoDouble.Type = (double)(light->GetLightType());
      light->GetPosition(lightInfoDouble.Position);
      light->GetFocalPoint(lightInfoDouble.FocalPoint);
      
      for (id = 0; id < numProcs; id++)
        {
        if (id == this->RootProcessId) continue;
        this->Controller->Send((double *)(&lightInfoDouble),
                               vtkParallelRenderManager::LIGHT_INFO_DOUBLE_SIZE, 
                               id,
                               vtkParallelRenderManager::LIGHT_INFO_DOUBLE_TAG);
        }
      }

    this->SendRendererInformation(ren);
    }
//    }

  this->PreRenderProcessing();
}

void vtkParallelRenderManager::EndRender()
{
  if (!this->ParallelRendering)
    {
    return;
    }

  this->Timer->StopTimer();
  this->RenderTime = this->Timer->GetElapsedTime();
  this->ImageProcessingTime = 0;

  if (!this->UseCompositing)
    {
    this->Lock = 0;
    return;
    }

  // EndRender only happens on root.
  if (this->CheckForAbortComposite())
    {
    this->Lock = 0;
    return;
    }  

  this->PostRenderProcessing();

  // Restore renderer viewports, if necessary.
  if (this->ImageReductionFactor > 1)
    {
    vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
    vtkRenderer *ren;
    rens->InitTraversal();
    ren = rens->GetNextItem();
    ren->SetViewport(this->Viewports->GetPointer(0));
    }

  this->WriteFullImage();

  this->InvokeEvent(vtkCommand::EndEvent, NULL);

  this->Lock = 0;
}

void vtkParallelRenderManager::SatelliteEndRender()
{
  if (this->CheckForAbortComposite())
    {
    return;
    }

  if (!this->ParallelRendering)
    {
    return;
    }
  if (!this->UseCompositing)
    {
    return;
    }

  this->PostRenderProcessing();

  this->WriteFullImage();

  this->InvokeEvent(vtkCommand::EndEvent, NULL);
}

void vtkParallelRenderManager::RenderRMI()
{
  this->RenderWindow->Render();
}

void vtkParallelRenderManager::ResetCamera(vtkRenderer *ren)
{
  vtkDebugMacro("ResetCamera");

  double bounds[6];

  if (this->Lock)
    {
    // Can't query other processes in the middle of a render.
    // Just grab local value instead.
    this->LocalComputeVisiblePropBounds(ren, bounds);
    ren->ResetCamera(bounds);
    return;
    }

  this->Lock = 1;

  this->ComputeVisiblePropBounds(ren, bounds);
  // Keep from setting camera from some outrageous value.
  if (!vtkMath::AreBoundsInitialized(bounds))
    {
    // See if the not pickable values are better.
    ren->ComputeVisiblePropBounds(bounds);
    if (!vtkMath::AreBoundsInitialized(bounds))
      {
      this->Lock = 0;
      return;
      }
    }
  ren->ResetCamera(bounds);
  
  this->Lock = 0;
}

void vtkParallelRenderManager::ResetCameraClippingRange(vtkRenderer *ren)
{
  vtkDebugMacro("ResetCameraClippingRange");

  double bounds[6];

  if (this->Lock)
    {
    // Can't query other processes in the middle of a render.
    // Just grab local value instead.
    this->LocalComputeVisiblePropBounds(ren, bounds);
    ren->ResetCameraClippingRange(bounds);
    return;
    }

  this->Lock = 1;
  
  this->ComputeVisiblePropBounds(ren, bounds);
  ren->ResetCameraClippingRange(bounds);

  this->Lock = 0;
}

void vtkParallelRenderManager::ComputeVisiblePropBoundsRMI()
{
  vtkDebugMacro("ComputeVisiblePropBoundsRMI");
  int i;

  // Get proper renderer.
  int renderId = -1;
  if (!this->Controller->Receive(&renderId, 1, this->RootProcessId,
                                 vtkParallelRenderManager::REN_ID_TAG))
    {
    return;
    }
  vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
  vtkRenderer *ren = NULL;
  rens->InitTraversal();
  for (i = 0; i <= renderId; i++)
    {
    ren = rens->GetNextItem();
    }

  if (ren == NULL)
    {
    vtkWarningMacro("Client requested invalid renderer in "
            "ComputeVisiblePropBoundsRMI\n"
            "Defaulting to first renderer");
    rens->InitTraversal();
    ren = rens->GetNextItem();
    }

  double bounds[6];
  this->LocalComputeVisiblePropBounds(ren, bounds);

  this->Controller->Send(bounds, 6, this->RootProcessId,
                         vtkParallelRenderManager::BOUNDS_TAG);
}

void vtkParallelRenderManager::LocalComputeVisiblePropBounds(vtkRenderer *ren,
                                                             double bounds[6])
{
  ren->ComputeVisiblePropBounds(bounds);
}


void vtkParallelRenderManager::ComputeVisiblePropBounds(vtkRenderer *ren,
                                                        double bounds[6])
{
  vtkDebugMacro("ComputeVisiblePropBounds");

  if (!this->ParallelRendering)
    {
    ren->ComputeVisiblePropBounds(bounds);
    return;
    }

  if (this->Controller)
    {
    if (this->Controller->GetLocalProcessId() != this->RootProcessId)
      {
      vtkErrorMacro("ComputeVisiblePropBounds/ResetCamera can only be called on root process");
      return;
      }

    vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
    rens->InitTraversal();
    int renderId = 0;
    while (1)
      {
      vtkRenderer *myren = rens->GetNextItem();
      if (myren == NULL)
        {
        vtkWarningMacro("ComputeVisiblePropBounds called with unregistered renderer " << ren << "\nDefaulting to first renderer.");
        renderId = 0;
        break;
        }
      if (myren == ren)
        {
        //Found correct renderer.
        break;
        }
      renderId++;
      }

    //Invoke RMI's on servers to perform their own ComputeVisiblePropBounds.
    int numProcs = this->Controller->GetNumberOfProcesses();
    int id;
    for (id = 0; id < numProcs; id++)
      {
      if (id == this->RootProcessId)
        {
        continue;
        }
      this->Controller->TriggerRMI(
        id, vtkParallelRenderManager::COMPUTE_VISIBLE_PROP_BOUNDS_RMI_TAG);
      this->Controller->Send(&renderId, 1, id,
                             vtkParallelRenderManager::REN_ID_TAG);
      }
    
    //Now that all the RMI's have been invoked, we can safely query our
    //local bounds even if an Update requires a parallel operation.

    this->LocalComputeVisiblePropBounds(ren, bounds);
  
    //Collect all the bounds.
    for (id = 0; id < numProcs; id++)
      {
      double tmp[6];

      if (id == this->RootProcessId)
        {
        continue;
        }

      this->Controller->Receive(tmp, 6, id, vtkParallelRenderManager::BOUNDS_TAG);
      
      if (tmp[0] < bounds[0])
        {
        bounds[0] = tmp[0];
        }
      if (tmp[1] > bounds[1])
        {
        bounds[1] = tmp[1];
        }
      if (tmp[2] < bounds[2])
        {
        bounds[2] = tmp[2];
        }
      if (tmp[3] > bounds[3])
        {
        bounds[3] = tmp[3];
        }
      if (tmp[4] < bounds[4])
        {
        bounds[4] = tmp[4];
        }
      if (tmp[5] > bounds[5])
        {
        bounds[5] = tmp[5];
        }
      }
    }
  else
    {
    vtkWarningMacro("ComputeVisiblePropBounds/ResetCamera called before Controller set");

    ren->ComputeVisiblePropBounds(bounds);
    }
}

void vtkParallelRenderManager::InitializeRMIs()
{
  vtkDebugMacro("InitializeRMIs");

  if (this->Controller == NULL)
    {
    vtkErrorMacro("InitializeRMIs requires a controller.");
    return;
    }

  this->Controller->AddRMI(::RenderRMI, this,
                           vtkParallelRenderManager::RENDER_RMI_TAG);
  this->Controller->AddRMI(::ComputeVisiblePropBoundsRMI, this,
                           vtkParallelRenderManager::
                           COMPUTE_VISIBLE_PROP_BOUNDS_RMI_TAG);
}

void vtkParallelRenderManager::ResetAllCameras()
{
  vtkDebugMacro("ResetAllCameras");

  if (!this->RenderWindow)
    {
    vtkErrorMacro("Called ResetAllCameras before RenderWindow set");
    return;
    }

  vtkRendererCollection *rens;
  vtkRenderer *ren;

  rens = this->RenderWindow->GetRenderers();
  for (rens->InitTraversal(); (ren = rens->GetNextItem()); )
    {
    this->ResetCamera(ren);
    }
}

void vtkParallelRenderManager::SetImageReductionFactor(int factor)
{
  // Clamp factor.
  factor = (factor < 1) ? 1 : factor;
  factor = (factor > this->MaxImageReductionFactor)
    ? this->MaxImageReductionFactor : factor;

  if (this->MagnifyImageMethod == LINEAR)
    {
    // Make factor be a power of 2.
    int pow_of_2 = 1;
    while (pow_of_2 <= factor)
      {
      pow_of_2 <<= 1;
      }
    factor = pow_of_2 >> 1;
    }

  if (factor == this->ImageReductionFactor)
    {
    return;
    }

  this->ImageReductionFactor = factor;
  this->Modified();
}

void vtkParallelRenderManager::SetMagnifyImageMethod(int method)
{
  if (this->MagnifyImageMethod == method)
    {
    return;
    }

  this->MagnifyImageMethod = method;
  // May need to modify image reduction factor.
  this->SetImageReductionFactor(this->ImageReductionFactor);
}

void vtkParallelRenderManager::SetImageReductionFactorForUpdateRate(double desiredUpdateRate)
{
  vtkDebugMacro("Setting reduction factor for update rate of "
        << desiredUpdateRate);

  if (desiredUpdateRate == 0.0)
    {
    this->SetImageReductionFactor(1);
    return;
    }

  int *size = this->RenderWindow->GetSize();
  int numPixels = size[0]*size[1];
  int numReducedPixels
    = numPixels/(this->ImageReductionFactor*this->ImageReductionFactor);

  double renderTime = this->GetRenderTime();
  double pixelTime = this->GetImageProcessingTime();

  double timePerPixel;
  if (numReducedPixels > 0)
    {
    timePerPixel = pixelTime/numReducedPixels;
    }
  else
    {
    // Must be before first render.
    this->SetImageReductionFactor(1);
    return;
    }

  this->AverageTimePerPixel = (3*this->AverageTimePerPixel + timePerPixel)/4;

  double allottedPixelTime = 1.0/desiredUpdateRate - renderTime;
  // Give ourselves at least 15% of render time.
  if (allottedPixelTime < 0.15*renderTime)
    {
    allottedPixelTime = 0.15*renderTime;
    }

  vtkDebugMacro("TimePerPixel: " << timePerPixel
        << ", AverageTimePerPixel: " << this->AverageTimePerPixel
        << ", AllottedPixelTime: " << allottedPixelTime);

  double pixelsToUse = allottedPixelTime/this->AverageTimePerPixel;

  if ( (pixelsToUse < 1) ||
       (numPixels/pixelsToUse > this->MaxImageReductionFactor) )
    {
    this->SetImageReductionFactor(this->MaxImageReductionFactor);
    }
  else if (pixelsToUse >= numPixels)
    {
    this->SetImageReductionFactor(1);
    }
  else
    {
    this->SetImageReductionFactor((int)(numPixels/pixelsToUse));
    }
}

void vtkParallelRenderManager::SetRenderWindowSize()
{
  this->RenderWindow->SetSize(this->FullImageSize[0], this->FullImageSize[1]);
}

int vtkParallelRenderManager::LastRenderInFrontBuffer()
{
  return this->RenderWindow->GetSwapBuffers();
}

int vtkParallelRenderManager::ChooseBuffer()
{
  int myId = this->Controller->GetLocalProcessId();
  if (myId == 0)
    {
    return 0;
    }
  return 1;
}

static void MagnifyImageNearest(vtkUnsignedCharArray *fullImage,
                                int fullImageSize[2],
                                vtkUnsignedCharArray *reducedImage,
                                int reducedImageSize[2],
                                vtkTimerLog *timer)
{
  int numComp = reducedImage->GetNumberOfComponents();;

  fullImage->SetNumberOfComponents(numComp);
  fullImage->SetNumberOfTuples(fullImageSize[0]*fullImageSize[1]);

  timer->StartTimer();

  // Inflate image.
  double xstep = (double)reducedImageSize[0]/fullImageSize[0];
  double ystep = (double)reducedImageSize[1]/fullImageSize[1];
  unsigned char *lastsrcline = NULL;
  for (int y = 0; y < fullImageSize[1]; y++)
    {
    unsigned char *destline =
      fullImage->GetPointer(numComp*fullImageSize[0]*y);
    unsigned char *srcline =
      reducedImage->GetPointer(numComp*reducedImageSize[0]*(int)(ystep*y));
    if (srcline == lastsrcline)
      {
      // This line same as last one.
      memcpy(destline,
             (const unsigned char *)(destline - numComp*fullImageSize[0]),
             numComp*fullImageSize[0]);
      }
    else
      {
      for (int x = 0; x < fullImageSize[0]; x++)
        {
        int srcloc = numComp*(int)(x*xstep);
        int destloc = numComp*x;
        for (int i = 0; i < numComp; i++)
          {
          destline[destloc + i] = srcline[srcloc + i];
          }
        }
      lastsrcline = srcline;
      }
    }

  timer->StopTimer();
}

// A neat trick to quickly divide all 4 of the bytes in an integer by 2.
#define VTK_VEC_DIV_2(intvector)    (((intvector) >> 1) & 0x7F7F7F7F)

static void MagnifyImageLinear(vtkUnsignedCharArray *fullImage,
                               int fullImageSize[2],
                               vtkUnsignedCharArray *reducedImage,
                               int reducedImageSize[2],
                               vtkTimerLog *timer)
{
  int xmag, ymag;
  int x, y;
  int srcComp = reducedImage->GetNumberOfComponents();;

  //Allocate full image so all pixels are on 4-byte integer boundaries.
  fullImage->SetNumberOfComponents(4);
  fullImage->SetNumberOfTuples(fullImageSize[0]*fullImageSize[1]);

  timer->StartTimer();

  // Guess x and y magnification.  Round up to ensure we do not try to
  // read data from the image data that does not exist.
  xmag = (fullImageSize[0]+reducedImageSize[0]-1)/reducedImageSize[0];
  ymag = (fullImageSize[1]+reducedImageSize[1]-1)/reducedImageSize[1];

  // For speed, we only magnify by powers of 2.  Round up to the nearest
  // power of 2 to ensure that the reduced image is large enough.
  int powOf2;
  for (powOf2 = 1; powOf2 < xmag; powOf2 <<= 1);
  xmag = powOf2;
  for (powOf2 = 1; powOf2 < ymag; powOf2 <<= 1);
  ymag = powOf2;

  unsigned char *srcline = reducedImage->GetPointer(0);
  unsigned char *destline = fullImage->GetPointer(0);
  for (y = 0; y < fullImageSize[1]; y += ymag)
    {
    unsigned char *srcval = srcline;
    unsigned char *destval = destline;
    for (x = 0; x < fullImageSize[0]; x += xmag)
      {
      destval[0] = srcval[0];
      destval[1] = srcval[1];
      destval[2] = srcval[2];
      destval[3] = 0xFF;    //Hope we don't need the alpha value.
      srcval += srcComp;
      destval += 4*xmag;
      }
    srcline += srcComp*reducedImageSize[0];
    destline += 4*fullImageSize[0]*ymag;
    }

  // Now that we have everything on 4-byte boundaries, we will treat
  // everything as integers for much faster computation.
  unsigned int *image = (unsigned int *)fullImage->GetPointer(0);

  // Fill in scanlines.
  for (; xmag > 1; xmag >>= 1)
    {
    int halfXMag = xmag/2;
    for (y = 0; y < fullImageSize[1]; y += ymag)
      {
      unsigned int *scanline = image + y*fullImageSize[0];
      int maxX = fullImageSize[0] - halfXMag;    //Don't access bad memory.
      for (x = halfXMag; x < maxX; x += xmag)
        {
        scanline[x] =
          VTK_VEC_DIV_2(scanline[x-halfXMag]) +
          VTK_VEC_DIV_2(scanline[x+halfXMag]);
        }
      }
    }

  // Add blank scanlines.
  for (; ymag > 1; ymag >>= 1)
    {
    int halfYMag = ymag/2;
    int maxY = fullImageSize[1] - halfYMag;    //Don't access bad memory.
    for (y = halfYMag; y < maxY; y += ymag)
      {
      unsigned int *destline2 = image + y*fullImageSize[0];
      unsigned int *srcline1 = image + (y-halfYMag)*fullImageSize[0];
      unsigned int *srcline2 = image + (y+halfYMag)*fullImageSize[0];
      for (x = 0; x < fullImageSize[0]; x++)
        {
        destline2[x] = VTK_VEC_DIV_2(srcline1[x]) + VTK_VEC_DIV_2(srcline2[x]);
        }
      }
    }
}

void vtkParallelRenderManager::MagnifyReducedImage()
{
  if ((this->FullImageUpToDate))
    {
    return;
    }

  this->ReadReducedImage();

  if (this->FullImage->GetPointer(0) != this->ReducedImage->GetPointer(0))
    {
    switch (this->MagnifyImageMethod)
      {
      case vtkParallelRenderManager::NEAREST:
        MagnifyImageNearest(this->FullImage, this->FullImageSize,
                            this->ReducedImage, this->ReducedImageSize,
                            this->Timer);
        break;
      case LINEAR:
        MagnifyImageLinear(this->FullImage, this->FullImageSize,
                           this->ReducedImage, this->ReducedImageSize,
                           this->Timer);
        break;
      }
    // We log the image inflation under render time because it is inversely
    // proportional to the image size.  This makes the auto image reduction
    // calculation work better.
    this->RenderTime += this->Timer->GetElapsedTime();
    }

  this->FullImageUpToDate = 1;
}

void vtkParallelRenderManager::WriteFullImage()
{
  if (this->RenderWindowImageUpToDate || !this->WriteBackImages)
    {
    return;
    }

  if (this->MagnifyImages && (this->ImageReductionFactor > 1))
    {
    this->MagnifyReducedImage();
    this->SetRenderWindowPixelData(this->FullImage, this->FullImageSize);
    }
  else
    {
    // Only write back image if it has already been read and potentially
    // changed.
    if (this->ReducedImageUpToDate)
      {
      this->SetRenderWindowPixelData(this->ReducedImage,
                     this->ReducedImageSize);
      }
    }

  this->RenderWindowImageUpToDate = 1;
}

void vtkParallelRenderManager::SetRenderWindowPixelData(
  vtkUnsignedCharArray *pixels, const int pixelDimensions[2])
{
  if (pixels->GetNumberOfComponents() == 4)
    {
    this->RenderWindow->SetRGBACharPixelData(0, 0,
                                             pixelDimensions[0]-1,
                                             pixelDimensions[1]-1,
                                             pixels,
                                             this->ChooseBuffer());
    }
  else
    {
    this->RenderWindow->SetPixelData(0, 0,
                                     pixelDimensions[0]-1,
                                     pixelDimensions[1]-1,
                                     pixels,
                                     this->ChooseBuffer());
    }
}

void vtkParallelRenderManager::ReadReducedImage()
{
  if (this->ReducedImageUpToDate)
    {
    return;
    }

  this->Timer->StartTimer();

  if (this->ImageReductionFactor > 1)
    {
    this->RenderWindow->GetRGBACharPixelData(0, 0, this->ReducedImageSize[0]-1,
                                             this->ReducedImageSize[1]-1,
                                             this->ChooseBuffer(),
                                             this->ReducedImage);
    }
  else
    {
    this->RenderWindow->GetRGBACharPixelData(0, 0, this->FullImageSize[0]-1,
                                             this->FullImageSize[1]-1,
                                             this->ChooseBuffer(),
                                             this->FullImage);
    this->FullImageUpToDate = 1;
    this->ReducedImage
      ->SetNumberOfComponents(this->FullImage->GetNumberOfComponents());
    this->ReducedImage->SetArray(this->FullImage->GetPointer(0),
                                 this->FullImage->GetSize(), 1);
    this->ReducedImage->SetNumberOfTuples(this->FullImage->GetNumberOfTuples());
    }

  this->Timer->StopTimer();
  this->ImageProcessingTime += this->Timer->GetElapsedTime();

  this->ReducedImageUpToDate = 1;
}

void vtkParallelRenderManager::GetPixelData(vtkUnsignedCharArray *data)
{
  if (!this->RenderWindow)
    {
    vtkErrorMacro("Tried to read pixel data from non-existent RenderWindow");
    return;
    }

  // Read image from RenderWindow and magnify if necessary.
  this->MagnifyReducedImage();

  data->SetNumberOfComponents(this->FullImage->GetNumberOfComponents());
  data->SetArray(this->FullImage->GetPointer(0),
                 this->FullImage->GetSize(), 1);
  data->SetNumberOfTuples(this->FullImage->GetNumberOfTuples());
}

void vtkParallelRenderManager::GetPixelData(int x1, int y1, int x2, int y2,
                                            vtkUnsignedCharArray *data)
{
  if (!this->RenderWindow)
    {
    vtkErrorMacro("Tried to read pixel data from non-existent RenderWindow");
    return;
    }

  this->MagnifyReducedImage();

  if (x1 > x2)
    {
    int tmp = x1;
    x1 = x2;
    x2 = tmp;
    }
  if (y1 > y2)
    {
    int tmp = y1;
    y1 = y2;
    y2 = tmp;
    }

  if ( (x1 < 0) || (x2 >= this->FullImageSize[0]) ||
       (y1 < 0) || (y2 >= this->FullImageSize[1]) )
    {
    vtkErrorMacro("Requested pixel data out of RenderWindow bounds");
    return;
    }

  vtkIdType width = x2 - x1 + 1;
  vtkIdType height = y2 - y1 + 1;

  int numComp = this->FullImage->GetNumberOfComponents();

  data->SetNumberOfComponents(numComp);
  data->SetNumberOfTuples(width*height);

  const unsigned char *src = this->FullImage->GetPointer(0);
  unsigned char *dest = data->WritePointer(0, width*height*numComp);

  for (int row = 0; row < height; row++)
    {
    memcpy(dest + row*width*numComp,
           src + (row+y1)*this->FullImageSize[0]*numComp + x1*numComp,
           width*numComp);
    }
}

void vtkParallelRenderManager::GetReducedPixelData(vtkUnsignedCharArray *data)
{
  if (!this->RenderWindow)
    {
    vtkErrorMacro("Tried to read pixel data from non-existent RenderWindow");
    return;
    }

  // Read image from RenderWindow and magnify if necessary.
  this->ReadReducedImage();

  data->SetNumberOfComponents(this->ReducedImage->GetNumberOfComponents());
  data->SetArray(this->ReducedImage->GetPointer(0),
                 this->ReducedImage->GetSize(), 1);
  data->SetNumberOfTuples(this->ReducedImage->GetNumberOfTuples());
}

void vtkParallelRenderManager::GetReducedPixelData(int x1, int y1,
                                                   int x2, int y2,
                                                   vtkUnsignedCharArray *data)
{
  if (!this->RenderWindow)
    {
    vtkErrorMacro("Tried to read pixel data from non-existent RenderWindow");
    return;
    }

  this->ReadReducedImage();

  if (x1 > x2)
    {
    int tmp = x1;
    x1 = x2;
    x2 = tmp;
    }
  if (y1 > y2)
    {
    int tmp = y1;
    y1 = y2;
    y2 = tmp;
    }

  if ( (x1 < 0) || (x2 >= this->ReducedImageSize[0]) ||
       (y1 < 0) || (y2 >= this->ReducedImageSize[1]) )
    {
    vtkErrorMacro("Requested pixel data out of RenderWindow bounds");
    return;
    }

  vtkIdType width = x2 - x1 + 1;
  vtkIdType height = y2 - y1 + 1;

  int numComp = this->ReducedImage->GetNumberOfComponents();

  data->SetNumberOfComponents(numComp);
  data->SetNumberOfTuples(width*height);

  const unsigned char *src = this->ReducedImage->GetPointer(0);
  unsigned char *dest = data->WritePointer(0, width*height*numComp);

  for (int row = 0; row < height; row++)
    {
    memcpy(dest + row*width*numComp,
           src + (row+y1)*this->ReducedImageSize[0]*numComp + x1*numComp,
           width*numComp);
    }
}

// Static function prototypes --------------------------------------------

static void AbortRenderCheck(vtkObject *vtkNotUsed(caller), 
                             unsigned long vtkNotUsed(event),
                 void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  self->CheckForAbortRender();
}

static void StartRender(vtkObject *vtkNotUsed(caller), 
                        unsigned long vtkNotUsed(event),
            void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  self->StartRender();
}

static void EndRender(vtkObject *vtkNotUsed(caller), 
                      unsigned long vtkNotUsed(event),
            void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  self->EndRender();
}

static void SatelliteStartRender(vtkObject *vtkNotUsed(caller),
                unsigned long vtkNotUsed(event),
                void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  self->SatelliteStartRender();
}

static void SatelliteEndRender(vtkObject *vtkNotUsed(caller),
                  unsigned long vtkNotUsed(event),
                  void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  self->SatelliteEndRender();
}

/*
static void ResetCamera(vtkObject *caller,
                        unsigned long vtkNotUsed(event),
                        void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  vtkRenderer *ren = (vtkRenderer *)caller;
  self->ResetCamera(ren);
}

static void ResetCameraClippingRange(vtkObject *caller,
                                     unsigned long vtkNotUsed(event),
                                     void *clientData, void *)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)clientData;
  vtkRenderer *ren = (vtkRenderer *)caller;
  self->ResetCameraClippingRange(ren);
}
*/

static void RenderRMI(void *arg, void *, int, int)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)arg;
  self->RenderRMI();
}

static void ComputeVisiblePropBoundsRMI(void *arg, void *, int, int)
{
  vtkParallelRenderManager *self = (vtkParallelRenderManager *)arg;
  self->ComputeVisiblePropBoundsRMI();
}



// the variables such as winInfoInt are initialzed prior to use  
#if defined(_MSC_VER) && !defined(VTK_DISPLAY_WIN32_WARNINGS)
#pragma warning ( disable : 4701 )
#endif

void vtkParallelRenderManager::SatelliteStartRender()
{
  vtkParallelRenderManager::RenderWindowInfoInt winInfoInt;
  vtkParallelRenderManager::RenderWindowInfoDouble winInfoDouble;
  vtkParallelRenderManager::RendererInfoInt renInfoInt;
  vtkParallelRenderManager::RendererInfoDouble renInfoDouble;
  vtkParallelRenderManager::LightInfoDouble lightInfoDouble;
  int i, j;

  vtkDebugMacro("SatelliteStartRender");

  this->FullImageUpToDate = 0;
  this->ReducedImageUpToDate = 0;
  this->RenderWindowImageUpToDate = 0;

  if (this->FullImage->GetPointer(0) == this->ReducedImage->GetPointer(0))
    {
    // "Un-share" pointer for full/reduced images in case we need separate
    // arrays this run.
    this->ReducedImage->Initialize();
    }

  if (!this->ParallelRendering)
    {
    return;
    }

  this->InvokeEvent(vtkCommand::StartEvent, NULL);

  if (!this->Controller->Receive((int *)(&winInfoInt), 
                                 vtkParallelRenderManager::WIN_INFO_INT_SIZE,
                                 this->RootProcessId,
                                 vtkParallelRenderManager::WIN_INFO_INT_TAG))
    {
    return;
    }
  
  if (!this->Controller->Receive((double *)(&winInfoDouble),
                                 vtkParallelRenderManager::WIN_INFO_DOUBLE_SIZE, 
                                 this->RootProcessId,
                                 vtkParallelRenderManager::WIN_INFO_DOUBLE_TAG))
    {
    return;
    }
  
  this->ReceiveWindowInformation();

  this->RenderWindow->SetDesiredUpdateRate(winInfoDouble.DesiredUpdateRate);
  this->UseCompositing = winInfoInt.UseCompositing;
  this->ImageReductionFactor = winInfoInt.ImageReductionFactor;
  this->FullImageSize[0] = winInfoInt.FullSize[0];
  this->FullImageSize[1] = winInfoInt.FullSize[1];
  this->ReducedImageSize[0] = winInfoInt.ReducedSize[0];
  this->ReducedImageSize[1] = winInfoInt.ReducedSize[1];
  this->SetRenderWindowSize();

  vtkRendererCollection *rens = this->RenderWindow->GetRenderers();
  rens->InitTraversal();
  for (i = 0; i < winInfoInt.NumberOfRenderers; i++)
    {
    if (!this->Controller->Receive((int *)(&renInfoInt), 
                                   vtkParallelRenderManager::REN_INFO_INT_SIZE,
                                   this->RootProcessId,
                                   vtkParallelRenderManager::REN_INFO_INT_TAG))
      {
      continue;
      }
    if (!this->Controller->Receive((double *)(&renInfoDouble),
                                   vtkParallelRenderManager::REN_INFO_DOUBLE_SIZE,
                                   this->RootProcessId,
                                   vtkParallelRenderManager::REN_INFO_DOUBLE_TAG))
      {
      continue;
      }
    
    vtkLightCollection *lc = NULL;
    vtkRenderer *ren = rens->GetNextItem();
    if (ren == NULL)
      {
      vtkErrorMacro("Not enough renderers");
      }
    else
      {
      ren->SetViewport(renInfoDouble.Viewport);
      ren->SetBackground(renInfoDouble.Background[0],
                         renInfoDouble.Background[1],
                         renInfoDouble.Background[2]);
      vtkCamera *cam = ren->GetActiveCamera();
      cam->SetPosition(renInfoDouble.CameraPosition);
      cam->SetFocalPoint(renInfoDouble.CameraFocalPoint);
      cam->SetViewUp(renInfoDouble.CameraViewUp);
      cam->SetClippingRange(renInfoDouble.CameraClippingRange);
      if (renInfoDouble.ParallelScale != 0.0)
        {
        cam->ParallelProjectionOn();
        cam->SetParallelScale(renInfoDouble.ParallelScale);
        }
      else
        {
        cam->ParallelProjectionOff();
        }
      lc = ren->GetLights();
      lc->InitTraversal();
      }

    for (j = 0; j < renInfoInt.NumberOfLights; j++)
      {
      if (ren != NULL && lc != NULL)
        {
        vtkLight *light = lc->GetNextItem();
        if (light == NULL)
          {
          // Not enough lights?  Just create them.
          vtkDebugMacro("Adding light");
          light = vtkLight::New();
          ren->AddLight(light);
          light->Delete();
          }

        this->Controller->Receive((double *)(&lightInfoDouble),
                                  vtkParallelRenderManager::LIGHT_INFO_DOUBLE_SIZE,
                                  this->RootProcessId,
                                  vtkParallelRenderManager::LIGHT_INFO_DOUBLE_TAG);
        light->SetLightType((int)(lightInfoDouble.Type));
        light->SetPosition(lightInfoDouble.Position);
        light->SetFocalPoint(lightInfoDouble.FocalPoint);
        }
      }

    if (ren != NULL)
      {
      vtkLight *light;
      while ((light = lc->GetNextItem()))
        {
        // To many lights?  Just remove the extras.
        ren->RemoveLight(light);
        }
      }

    this->ReceiveRendererInformation(ren);
    }

  this->PreRenderProcessing();
}
