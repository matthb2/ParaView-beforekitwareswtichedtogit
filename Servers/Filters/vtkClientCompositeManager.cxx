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

#ifdef VTK_USE_MPI
 #include <mpi.h>
#endif

#include "vtkClientCompositeManager.h"
#include "vtkCompositeManager.h"

#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkImageActor.h"
#include "vtkCompressCompositer.h"
#include "vtkFloatArray.h"
#include "vtkImageData.h"
#include "vtkLight.h"
#include "vtkLightCollection.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkRenderer.h"
#include "vtkRendererCollection.h"
#include "vtkRenderWindow.h"
#include "vtkSocketController.h"
#include "vtkTimerLog.h"
#include "vtkToolkits.h"
#include "vtkTreeCompositer.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedCharArray.h"
// Until we trigger LOD from AllocatedRenderTime ...
#include "vtkByteSwap.h"

#include "vtkOutlineFilter.h"
#include "vtkPolyDataMapper.h"
#include "vtkActor.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkBMPWriter.h"

#ifdef _WIN32
#include "vtkWin32OpenGLRenderWindow.h"
#elif defined(VTK_USE_MESA)
#include "vtkMesaRenderWindow.h"
#endif


vtkCxxRevisionMacro(vtkClientCompositeManager, "$Revision$");
vtkStandardNewMacro(vtkClientCompositeManager);

vtkCxxSetObjectMacro(vtkClientCompositeManager,Compositer,vtkCompositer);

// Structures to communicate render info.
struct vtkClientCompositeIntInfo 
{
  // I am sending the origianl window size.  
  // The server can composite an image of any size.
  int WindowSize[2];
  int ImageReductionFactor;
  int SquirtLevel;
};

struct vtkClientCompositeDoubleInfo 
{
  double CameraPosition[3];
  double CameraFocalPoint[3];
  double CameraViewUp[3];
  double CameraClippingRange[2];
  double LightPosition[3];
  double LightFocalPoint[3];
  double Background[3];
  double ParallelScale;
  double CameraViewAngle;
};

#define vtkInitializeVector3(v) { v[0] = 0; v[1] = 0; v[2] = 0; }
#define vtkInitializeVector2(v) { v[0] = 0; v[1] = 0; }
#define vtkInitializeClientCompositeDoubleInfoMacro(r)      \
  {                                                      \
  vtkInitializeVector3(r.CameraPosition);                \
  vtkInitializeVector3(r.CameraFocalPoint);              \
  vtkInitializeVector3(r.CameraViewUp);                  \
  vtkInitializeVector2(r.CameraClippingRange);           \
  vtkInitializeVector3(r.LightPosition);                 \
  vtkInitializeVector3(r.LightFocalPoint);               \
  vtkInitializeVector3(r.Background);                    \
  r.ParallelScale = 0.0;                                 \
  r.CameraViewAngle = 0.0;                               \
  }
  


//-------------------------------------------------------------------------
vtkClientCompositeManager::vtkClientCompositeManager()
{
  this->SquirtLevel = 0;
  this->ClientController = NULL;
  this->ClientFlag = 1;

  this->StartTag = 0;

  this->InternalReductionFactor = 2;
  this->ImageReductionFactor = 2;
  this->PDataSize[0] = this->PDataSize[1] = 0;
  this->PData = NULL;
  this->ZData = NULL;
  this->PData2 = NULL;
  this->ZData2 = NULL;
  this->SquirtArray = NULL;

  this->Compositer = vtkCompressCompositer::New();
  //this->Compositer = vtkTreeCompositer::New();

  this->UseRGB = 0;

  this->BaseArray = NULL;

  this->UseCompositing = 0;
  this->CompositeData = vtkImageData::New();
  this->ImageActor = vtkImageActor::New();
  this->SavedCamera = vtkCamera::New();
}

  
//-------------------------------------------------------------------------
vtkClientCompositeManager::~vtkClientCompositeManager()
{
  this->SetPDataSize(0,0);
  
  this->SetController(NULL);
  this->SetClientController(NULL);

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

  if (this->PData2)
    {
    vtkCompositeManager::DeleteArray(this->PData2);
    this->PData2 = NULL;
    }
  if (this->ZData2)
    {
    vtkCompositeManager::DeleteArray(this->ZData2);
    this->ZData2 = NULL;
    }

  if (this->SquirtArray)
    {
    vtkCompositeManager::DeleteArray(this->SquirtArray);
    this->SquirtArray = NULL;
    }
  this->SetCompositer(NULL);

  this->ImageActor->Delete();
  this->ImageActor = NULL;
  this->SavedCamera->Delete();
  this->SavedCamera = NULL;

  if (this->BaseArray)
    {
    this->BaseArray->Delete();
    }
  
  this->CompositeData->Delete();
}


//----------------------------------------------------------------------------
// Called only on the client.
float vtkClientCompositeManager::GetZBufferValue(int x, int y)
{
  float z;
  int pArg[3];

  if (this->UseCompositing == 0)
    {
    // This could cause a problem between setting this ivar and rendering.
    // We could always composite, and always consider client z.
    float *pz;
    pz = this->RenderWindow->GetZbufferData(x, y, x, y);
    z = *pz;
    delete [] pz;
    return z;
    }
  
  // This first int is to check for byte swapping.
  pArg[0] = 1;
  pArg[1] = x;
  pArg[2] = y;
  this->ClientController->TriggerRMI(1, (void*)pArg, sizeof(int)*3, 
                                vtkClientCompositeManager::GATHER_Z_RMI_TAG);
  this->ClientController->Receive(&z, 1, 1, vtkClientCompositeManager::CLIENT_Z_TAG);
  return z;
}

//----------------------------------------------------------------------------
void vtkClientCompositeManagerGatherZBufferValueRMI(void *local, void *pArg, 
                                                    int pLength, int)
{
  vtkClientCompositeManager* self = (vtkClientCompositeManager*)local;
  int *p;
  int x, y;

  if (pLength != sizeof(int)*3)
    {
    vtkGenericWarningMacro("Integer sizes differ.");
    }

  p = (int*)pArg;
  if (p[0] != 1)
    { // Need to swap
    vtkByteSwap::SwapVoidRange(pArg, 3, sizeof(int));
    if (p[0] != 1)
      { // Swapping did not work.
      vtkGenericWarningMacro("Swapping failed.");
      }
    }
  x = p[1];
  y = p[2];
  
  self->GatherZBufferValueRMI(x, y);
}

//----------------------------------------------------------------------------
void vtkClientCompositeManager::GatherZBufferValueRMI(int x, int y)
{
  float z, otherZ;
  int pArg[3];

  // Get the z value.
  int *size = this->RenderWindow->GetSize();
  if (x < 0 || x >= size[0] || y < 0 || y >= size[1])
    {
    vtkErrorMacro("Point not contained in window.");
    z = 0;
    }
  else
    {
    float *tmp;
    tmp = this->RenderWindow->GetZbufferData(x, y, x, y);
    z = *tmp;
    delete [] tmp;
    }

  int myId = this->Controller->GetLocalProcessId();
  if (myId == 0)
    {
    int numProcs = this->Controller->GetNumberOfProcesses();
    int idx;
    pArg[0] = 1;
    pArg[1] = x;
    pArg[2] = y;
    for (idx = 1; idx < numProcs; ++idx)
      {
      this->Controller->TriggerRMI(1, (void*)pArg, sizeof(int)*3, 
                          vtkClientCompositeManager::GATHER_Z_RMI_TAG);
      }
    for (idx = 1; idx < numProcs; ++idx)
      {
      this->Controller->Receive(&otherZ, 1, idx, vtkClientCompositeManager::SERVER_Z_TAG);
      if (otherZ < z)
        {
        z = otherZ;
        }
      }
    // Send final result to the client.
    this->ClientController->Send(&z, 1, 1, vtkClientCompositeManager::CLIENT_Z_TAG);
    }
  else
    {
    // Send z to the root server node..
    this->Controller->Send(&z, 1, 1, vtkClientCompositeManager::SERVER_Z_TAG);
    }
}



//=======================  Client ========================



//-------------------------------------------------------------------------
// We may want to pass the render window as an argument for a sanity check.
void vtkClientCompositeManagerStartRender(vtkObject *caller,
                                 unsigned long vtkNotUsed(event), 
                                 void *clientData, void *)
{
  vtkClientCompositeManager *self = (vtkClientCompositeManager *)clientData;
  
  if (caller != self->GetRenderWindow())
    { // Sanity check.
    vtkGenericWarningMacro("Caller mismatch.");
    return;
    }
  
  self->StartRender();
}

//-------------------------------------------------------------------------
// Only called in process 0.
void vtkClientCompositeManager::StartRender()
{
  // If we are the satellite ...
  if ( ! this->ClientFlag)
    {
    this->SatelliteStartRender();
    return;
    }

  // This fixed some bug with the first render.
  // Something about the size of the render window I think.
  static int firstRender = 1;
  if (firstRender)
    {
    firstRender = 0;
    return;
    }

  struct vtkClientCompositeIntInfo winInfo;
  struct vtkClientCompositeDoubleInfo renInfo;
  int *size;
  vtkRendererCollection *rens;
  vtkRenderer* ren;
  vtkCamera *cam;
  vtkLightCollection *lc;
  vtkLight *light;
  float updateRate = this->RenderWindow->GetDesiredUpdateRate();
  
  if ( ! this->UseCompositing)
    {
    this->ImageActor->VisibilityOff();
    return;
    }

  // InternalReductionFactor changes based on still or interactive
  // renders.  The user set ImageReductionFactor can remain as set.
  this->InternalReductionFactor = this->ImageReductionFactor;
  if (this->InternalReductionFactor < 1)
    {
    this->InternalReductionFactor = 1;
    }
  // Still render never has pixel reduction.
  if (updateRate <= 2.0)
    {
    this->InternalReductionFactor = 1;
    }
  
  vtkDebugMacro("StartRender");
  
  vtkMultiProcessController *controller = this->ClientController;

  if (controller == NULL)
    {
    this->RenderWindow->EraseOn();
    return;
    }

  // Trigger the satellite processes to start their render routine.
  rens = this->RenderWindow->GetRenderers();
  size = this->RenderWindow->GetSize();
  winInfo.WindowSize[0] = size[0];
  winInfo.WindowSize[1] = size[1];
  winInfo.ImageReductionFactor = this->InternalReductionFactor;
  winInfo.SquirtLevel = this->SquirtLevel;
  
  controller->TriggerRMI(1, vtkClientCompositeManager::RENDER_RMI_TAG);

  // Synchronize the size of the windows.
  // Let the socket controller deal with byte swapping.
  controller->Send((int*)(&winInfo), 
                   sizeof(vtkClientCompositeIntInfo)/sizeof(int), 1, 
                   vtkClientCompositeManager::WIN_INFO_TAG);
  
  // Make sure the satellite renderers have the same camera I do.
  // Only deal with the first renderer.
  rens->InitTraversal();
  ren = rens->GetNextItem();
  cam = ren->GetActiveCamera();
  lc = ren->GetLights();
  lc->InitTraversal();
  light = lc->GetNextItem();
  cam->GetPosition(renInfo.CameraPosition);
  cam->GetFocalPoint(renInfo.CameraFocalPoint);
  cam->GetViewUp(renInfo.CameraViewUp);
  cam->GetClippingRange(renInfo.CameraClippingRange);
  renInfo.CameraViewAngle = cam->GetViewAngle();
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
  ren->Clear();
  // Let the socket controller deal with byte swapping.
  controller->Send((double*)(&renInfo), 
                   sizeof(struct vtkClientCompositeDoubleInfo)/sizeof(double),
                   1, vtkCompositeManager::REN_INFO_TAG);
  
  this->ReceiveAndSetColorBuffer();
}

//----------------------------------------------------------------------------
// Method executed only on client.
void vtkClientCompositeManager::ReceiveAndSetColorBuffer()
{
  int winSize[3];
  int length;

  // I hate to have a separate send for the rare case that the client asks
  // for a larger image than the server can provide, but this is no worse
  // than sending the length of the squirt array (previous implementation).
  // The length is now the third value in window size.
  // Maybe in the future we can encode the 
  // window size and length in the color buffer.
  this->ClientController->Receive(winSize, 3, 1, 123450);
  length = winSize[2];
  this->SetPDataSize(winSize[0], winSize[1]);

  if (!this->UseRGB && this->SquirtLevel)
    {
    this->SquirtArray->SetNumberOfTuples(length / (this->SquirtArray->GetNumberOfComponents()));
    this->ClientController->Receive((unsigned char*)(this->SquirtArray->GetVoidPointer(0)),
                                                    length, 1, 123451);
    this->SquirtDecompress(this->SquirtArray,
                           static_cast<vtkUnsignedCharArray*>(this->PData));
    //this->DeltaDecode(static_cast<vtkUnsignedCharArray*>(this->PData));
    }
  else
    {
    //int length = this->PData->GetMaxId() + 1;
    this->ClientController->Receive((unsigned char*)(this->PData->GetVoidPointer(0)),
                                    length, 1, 123451);
    }
 
  this->CompositeData->Initialize();
  
  // Set the color buffer
  vtkUnsignedCharArray* buf;
  buf = static_cast<vtkUnsignedCharArray*>(this->PData);

  this->CompositeData->GetPointData()->SetScalars(buf);
  this->CompositeData->SetScalarType(VTK_UNSIGNED_CHAR);
  this->CompositeData->SetNumberOfScalarComponents(buf->GetNumberOfComponents());

  this->CompositeData->SetDimensions(this->PDataSize[0],
                                     this->PDataSize[1], 1);

  // Sanity check.
  if (this->CompositeData->GetScalarType() != VTK_UNSIGNED_CHAR)
    {
    return;
    }

  this->ImageActor->VisibilityOn();
  this->ImageActor->SetInput(this->CompositeData);
  this->ImageActor->SetDisplayExtent(0, this->PDataSize[0]-1,
                                     0, this->PDataSize[1]-1, 0, 0);

  // int fixme
  // I would like to change SetRenderWindow to set renderer.  I believe the
  // only time that we use the render window now would be to synchronize
  // the swap buffers for tiled displays.
  // We also need to set the size of the render window, but this
  // could be done using the renderer.
  vtkRendererCollection* rens = this->RenderWindow->GetRenderers();
  rens->InitTraversal();
  vtkRenderer* ren = rens->GetNextItem();
  vtkCamera *cam = ren->GetActiveCamera();
  // Why doesn't camera have a Copy method?
  this->SavedCamera->SetPosition(cam->GetPosition());
  this->SavedCamera->SetFocalPoint(cam->GetFocalPoint());
  this->SavedCamera->SetViewUp(cam->GetViewUp());
  this->SavedCamera->SetParallelProjection(cam->GetParallelProjection());
  this->SavedCamera->SetParallelScale(cam->GetParallelScale());
  this->SavedCamera->SetClippingRange(cam->GetClippingRange());
  cam->ParallelProjectionOn();
  cam->SetParallelScale(
    (this->PDataSize[1]-1.0)*0.5);
  cam->SetPosition((this->PDataSize[0]-1.0)*0.5,
                   (this->PDataSize[1]-1.0)*0.5, 10.0);
  cam->SetFocalPoint((this->PDataSize[0]-1.0)*0.5,
                     (this->PDataSize[1]-1.0)*0.5, 0.0);
  cam->SetViewUp(0.0, 1.0, 0.0);
  cam->SetClippingRange(9.0, 11.0);
}

void vtkClientCompositeManager::EndRender()
{
  // If we are the satellite ...
  if ( ! this->ClientFlag)
    {
    this->SatelliteEndRender();
    return;
    }
  if (this->UseCompositing)
    {
    // Restore the camera.
    vtkRendererCollection* rens = this->RenderWindow->GetRenderers();
    rens->InitTraversal();
    vtkRenderer* ren = rens->GetNextItem();
    vtkCamera *cam = ren->GetActiveCamera();
    cam->SetPosition(this->SavedCamera->GetPosition());
    cam->SetFocalPoint(this->SavedCamera->GetFocalPoint());
    cam->SetViewUp(this->SavedCamera->GetViewUp());
    cam->SetParallelProjection(this->SavedCamera->GetParallelProjection());
    cam->SetParallelScale(this->SavedCamera->GetParallelScale());
    cam->SetClippingRange(this->SavedCamera->GetClippingRange());
    }
}






//=======================  Server ========================




//----------------------------------------------------------------------------
void vtkClientCompositeManagerRenderRMI(void *arg, void *, int, int)
{
  vtkClientCompositeManager* self = (vtkClientCompositeManager*) arg;
  
  self->RenderRMI();
}

//----------------------------------------------------------------------------
// Only Called by the satellite processes.
void vtkClientCompositeManager::RenderRMI()
{
  int i;

  if (this->ClientFlag)
    {
    vtkErrorMacro("Expecting the server side to call this method.");
    return;
    }

  // If this is root of server, trigger RenderRMI on satellites.
  if (this->Controller->GetLocalProcessId() == 0)
    {
    int numProcs = this->Controller->GetNumberOfProcesses();
    for (i = 1; i < numProcs; ++i)
      {
      this->Controller->TriggerRMI(i, 
                                    vtkClientCompositeManager::RENDER_RMI_TAG);
      }
    }

  this->RenderWindow->Render();
}

//-------------------------------------------------------------------------
void vtkClientCompositeManager::SatelliteStartRender()
{
  int j, myId, numProcs;
  vtkClientCompositeIntInfo winInfo;
  vtkClientCompositeDoubleInfo renInfo;
  vtkRendererCollection *rens;
  vtkRenderer* ren;
  vtkCamera *cam = 0;
  vtkLightCollection *lc;
  vtkLight *light;
  vtkRenderWindow* renWin = this->RenderWindow;
  vtkMultiProcessController *controller; 
  int otherId;

  myId = this->Controller->GetLocalProcessId();
  numProcs = this->Controller->GetNumberOfProcesses();

  if (myId == 0)
    { // server root receives from client.
    controller = this->ClientController;
    otherId = 1;
    }
  else
    { // Server satellite processes receive from server root.
    controller = this->Controller;
    otherId = 0;
    }
  
  vtkInitializeClientCompositeDoubleInfoMacro(renInfo);
  
  // Receive the window size.
  int winInfoSize = sizeof(struct vtkClientCompositeIntInfo)/sizeof(int);
  controller->Receive((int*)(&winInfo), winInfoSize, 
                      otherId, vtkCompositeManager::WIN_INFO_TAG);

  // In case the render window is smaller than requested.
  // This assumes that all server processes will have the
  // same (or larger) maximum render window size.
  int* screenSize = renWin->GetScreenSize();
  if (winInfo.WindowSize[0] > screenSize[0] ||
      winInfo.WindowSize[1] > screenSize[1])
    {
    if (myId == 0)
      {
      // We need to keep the same aspect ratio.
      int newSize[2];
      float k1, k2;
      k1 = (float)screenSize[0]/(float)winInfo.WindowSize[0];
      k2 = (float)screenSize[1]/(float)winInfo.WindowSize[1];
      if (k1 < k2)
        {
        newSize[0] = screenSize[0];
        newSize[1] = (int)((float)(winInfo.WindowSize[1]) * k1);
        }
      else
        {
        newSize[0] = (int)((float)(winInfo.WindowSize[0]) * k2);
        newSize[1] = screenSize[1];
        }
      winInfo.WindowSize[0] = newSize[0];
      winInfo.WindowSize[1] = newSize[1];
      }
    else
      { // Sanity check that all server 
        // procs have the same window limitation.
      vtkErrorMacro("Server window size mismatch.");
      }
    }

  renWin->SetSize(winInfo.WindowSize);

  if (myId == 0)
    {  
    // Relay info to server satellite processes.
    for (j = 1; j < numProcs; ++j)
      {
      this->Controller->Send((int*)(&winInfo), winInfoSize, 
                             j, vtkCompositeManager::WIN_INFO_TAG);
      }
    }
        
  this->InternalReductionFactor = winInfo.ImageReductionFactor;
  this->SquirtLevel = winInfo.SquirtLevel;

  // Synchronize the cameras on all processes.
  rens = renWin->GetRenderers();
  rens->InitTraversal();
  // Receive the camera information.
  // We put this before receive because we want the pipeline to be
  // updated the first time if the camera does not exist and we want
  // it to happen before we block in receive
  ren = rens->GetNextItem();
  if (ren)
    {
    cam = ren->GetActiveCamera();
    }
  int doubleInfoSize=sizeof(struct vtkClientCompositeDoubleInfo)/sizeof(double);

  controller->Receive((double*)(&renInfo), 
                      doubleInfoSize,
                      otherId, vtkCompositeManager::REN_INFO_TAG);
  if (myId == 0)
    {  // Relay info to server satellite processes.
    for (j = 1; j < numProcs; ++j)
      {
      this->Controller->Send((double*)(&renInfo), 
                      doubleInfoSize, 
                      j, vtkCompositeManager::REN_INFO_TAG);
      }
    }
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
    ren->SetViewport(0, 0, 1.0/(float)this->InternalReductionFactor, 
                     1.0/(float)this->InternalReductionFactor);
    }

  // This makes sure the arrays are large enough.
  this->SetPDataSize(winInfo.WindowSize[0]/winInfo.ImageReductionFactor, 
                     winInfo.WindowSize[1]/winInfo.ImageReductionFactor);
}

//-------------------------------------------------------------------------
void vtkClientCompositeManager::SatelliteEndRender()
{  
  int numProcs, myId;
  int front = 0;

  myId = this->Controller->GetLocalProcessId();
  numProcs = this->Controller->GetNumberOfProcesses();

  // Get the color buffer (pixel data).
  if (this->PData->GetNumberOfComponents() == 4)
    {
    vtkTimerLog::MarkStartEvent("Get RGBA Char Buffer");
    this->RenderWindow->GetRGBACharPixelData(
      0,0,this->PDataSize[0]-1, this->PDataSize[1]-1, 
      front,static_cast<vtkUnsignedCharArray*>(this->PData));
    vtkTimerLog::MarkEndEvent("Get RGBA Char Buffer");
    }
  else if (this->PData->GetNumberOfComponents() == 3)
    {
    vtkTimerLog::MarkStartEvent("Get RGB Char Buffer");
    this->RenderWindow->GetPixelData(
      0,0,this->PDataSize[0]-1, this->PDataSize[1]-1, 
      front,static_cast<vtkUnsignedCharArray*>(this->PData));
    vtkTimerLog::MarkEndEvent("Get RGB Char Buffer");
    }
 
  // Do not bother getting Z buffer and compositing if only one proc.
  if (numProcs > 1)
    { 
    // GetZBuffer.
    vtkTimerLog::MarkStartEvent("GetZBuffer");
    this->RenderWindow->GetZbufferData(0,0,
                                       this->PDataSize[0]-1, 
                                       this->PDataSize[1]-1,
                                       this->ZData);  
    vtkTimerLog::MarkEndEvent("GetZBuffer");

    // Let the subclass use its owns composite algorithm to
    // collect the results into "localPData" on process 0.
    vtkTimerLog::MarkStartEvent("Composite Buffers");
    this->Compositer->CompositeBuffer(this->PData, this->ZData,
                                      this->PData2, this->ZData2);
    vtkTimerLog::MarkEndEvent("Composite Buffers");

    // I believe the results end up in PData.
    }

  if (myId == 0)
    {
    int length;
    int winSize[3];
    winSize[0] = this->PDataSize[0];
    winSize[1] = this->PDataSize[1];
    if (! this->UseRGB && this->SquirtLevel)
      {
      this->SquirtCompress(static_cast<vtkUnsignedCharArray*>(this->PData),
                           this->SquirtArray, this->SquirtLevel);
      length = this->SquirtArray->GetMaxId() + 1;
      winSize[2] = length;
      this->ClientController->Send(winSize, 3, 1, 123450);
      this->ClientController->Send((unsigned char*)(this->SquirtArray->GetVoidPointer(0)),
                                                    length, 1, 123451);
      }
    else
      {
      length = this->PData->GetMaxId() + 1;
      winSize[2] = length;
      this->ClientController->Send(winSize, 3, 1, 123450);
      this->ClientController->Send((unsigned char*)(this->PData->GetVoidPointer(0)),
                                                    length, 1, 123451);
      }
    }
}




//-------------------------------------------------------------------------
void vtkClientCompositeManager::InitializeOffScreen()
{
  if (this->RenderWindow)
    { 
    if ( ! this->ClientFlag)
      {
      this->RenderWindow->OffScreenRenderingOn();
      }
    }
}




//-------------------------------------------------------------------------
// Only process 0 needs start and end render callbacks.
void vtkClientCompositeManager::SetRenderWindow(vtkRenderWindow *renWin)
{
  if (this->RenderWindow == renWin)
    {
    return;
    }
  this->Modified();

  //if (this->RenderWindow)
  //  {
  //  // Delete the reference.
  //  this->RenderWindow->UnRegister(this);
  //  this->RenderWindow =  NULL;
  //  }
  if (renWin)
    {
    // Add the image actor to the renderer.
    vtkRendererCollection* rens = renWin->GetRenderers();
    rens->InitTraversal();
    vtkRenderer *ren = rens->GetNextItem();
    ren->AddActor(this->ImageActor);
    }

  // Superclass sets up renderer start and end events.
  this->Superclass::SetRenderWindow(renWin);
}

void vtkClientCompositeManager::SetController(
                                          vtkMultiProcessController *mpc)
{
  if (this->Controller == mpc)
    {
    return;
    }
  if (mpc)
    {
    mpc->Register(this);
    }
  if (this->Controller)
    {
    this->Controller->UnRegister(this);
    }
  this->Controller = mpc;
}

//-------------------------------------------------------------------------
void vtkClientCompositeManager::SetClientController(
                                          vtkSocketController *mpc)
{
  if (this->ClientController == mpc)
    {
    return;
    }
  if (mpc)
    {
    mpc->Register(this);
    }
  if (this->ClientController)
    {
    this->ClientController->UnRegister(this);
    }
  this->ClientController = mpc;
}


//-------------------------------------------------------------------------
void vtkClientCompositeManager::SetUseRGB(int useRGB)
{
  if (useRGB == this->UseRGB)
    {
    return;
    }
  this->Modified();
  this->UseRGB = useRGB;

  this->ReallocPDataArrays();
}

//-------------------------------------------------------------------------
// Only reallocs arrays if they have been allocated already.
// This method is only used when buffer options have been changed:
// Char vs. float, or RGB vs. RGBA.
void vtkClientCompositeManager::ReallocPDataArrays()
{
  int numComps = 4;
  int numTuples = this->PDataSize[0] * this->PDataSize[1];
  int numProcs = 1;

  if ( ! this->ClientFlag)
    {
    numProcs = this->Controller->GetNumberOfProcesses();
    }

  if (this->UseRGB)
    {
    numComps = 3;
    }

  if (this->PData)
    {
    vtkCompositeManager::DeleteArray(this->PData);
    this->PData = NULL;
    } 
  if (this->PData2)
    {
    vtkCompositeManager::DeleteArray(this->PData2);
    this->PData2 = NULL;
    } 
  if (this->SquirtArray)
    {
    vtkCompositeManager::DeleteArray(this->SquirtArray);
    this->SquirtArray = NULL;
    } 

  // Allocate squirt compressed array.
  if (! this->UseRGB)
    {
    if (this->ClientFlag || this->Controller->GetLocalProcessId() == 0)
      {
      if (this->SquirtArray == NULL)
        {
        this->SquirtArray = vtkUnsignedCharArray::New();
        }
      vtkCompositeManager::ResizeUnsignedCharArray(
          this->SquirtArray, 4, numTuples);
      }
    }
  this->PData = vtkUnsignedCharArray::New();
  vtkCompositeManager::ResizeUnsignedCharArray(
      static_cast<vtkUnsignedCharArray*>(this->PData),
      numComps, numTuples);
  if (numProcs > 1)
    { // Not client (numProcs == 1)
    this->PData2 = vtkUnsignedCharArray::New();
    vtkCompositeManager::ResizeUnsignedCharArray(
        static_cast<vtkUnsignedCharArray*>(this->PData2),
        numComps, numTuples);
    }
}

// Work this and realloc PData into compositer.
//-------------------------------------------------------------------------
void vtkClientCompositeManager::SetPDataSize(int x, int y)
{
  int numComps;  
  int numPixels;
  int numProcs = 1;

  if ( ! this->ClientFlag)
    {
    numProcs = this->Controller->GetNumberOfProcesses();
    }

  if (x < 0)
    {
    x = 0;
    }
  if (y < 0)
    {
    y = 0;
    }

  if (this->PDataSize[0] == x && this->PDataSize[1] == y)
    {
    return;
    }

  this->PDataSize[0] = x;
  this->PDataSize[1] = y;

  if (x == 0 || y == 0)
    {
    if (this->PData)
      {
      vtkCompositeManager::DeleteArray(this->PData);
      this->PData = NULL;
      }
    if (this->PData2)
      {
      vtkCompositeManager::DeleteArray(this->PData2);
      this->PData2 = NULL;
      }
    if (this->SquirtArray)
      {
      vtkCompositeManager::DeleteArray(this->SquirtArray);
      this->SquirtArray = NULL;
      }
    if (this->ZData)
      {
      vtkCompositeManager::DeleteArray(this->ZData);
      this->ZData = NULL;
      }
    if (this->ZData2)
      {
      vtkCompositeManager::DeleteArray(this->ZData2);
      this->ZData2 = NULL;
      }
    return;
    }    

  numPixels = x * y;


  // Allocate squirt compressed array.
  if (! this->UseRGB)
    {
    if (this->ClientFlag || this->Controller->GetLocalProcessId() == 0)
      {
      if ( this->SquirtArray == NULL)
        {
        this->SquirtArray = vtkUnsignedCharArray::New();
        }
      vtkCompositeManager::ResizeUnsignedCharArray(
          this->SquirtArray, 4, numPixels);
      }
    }

  if (numProcs > 1)
    { // Not client (numProcs == 1)
    if (!this->ZData)
      {
      this->ZData = vtkFloatArray::New();
      }
    vtkCompositeManager::ResizeFloatArray(
      static_cast<vtkFloatArray*>(this->ZData), 
      1, numPixels);
    if (!this->ZData2)
      {
      this->ZData2 = vtkFloatArray::New();
      }
    vtkCompositeManager::ResizeFloatArray(
      static_cast<vtkFloatArray*>(this->ZData2), 
      1, numPixels);
    }


  // 3 for RGB,  4 for RGBA (RGB option only for char).
  if (this->UseRGB)
    {
    numComps = 3;
    }
  else
    { // RGBA
    numComps = 4;
    }
  
  if (!this->PData)
    {
    this->PData = vtkUnsignedCharArray::New();
    }
  vtkCompositeManager::ResizeUnsignedCharArray(
    static_cast<vtkUnsignedCharArray*>(this->PData), 
    numComps, numPixels);
  if (numProcs > 1)
    { // Not client (numProcs == 1)
    if (!this->PData2)
      {
      this->PData2 = vtkUnsignedCharArray::New();
      }
    vtkCompositeManager::ResizeUnsignedCharArray(
      static_cast<vtkUnsignedCharArray*>(this->PData2), 
      numComps, numPixels);
    }
}

//-------------------------------------------------------------------------
// This is only called in the satellite processes (not 0).
void vtkClientCompositeManager::InitializeRMIs()
{
  if (this->ClientFlag)
    { // Just in case.
    return;
    }
  if (this->Controller->GetLocalProcessId() == 0)
    { // Root on server waits for RMIs triggered by client.
    if (this->ClientController == NULL)
      {
      vtkErrorMacro("Missing Controller.");
      return;
      }
    this->ClientController->AddRMI(vtkClientCompositeManagerRenderRMI, (void*)this, 
                                   vtkClientCompositeManager::RENDER_RMI_TAG); 
    this->ClientController->AddRMI(vtkClientCompositeManagerGatherZBufferValueRMI, 
                                   (void*)this, 
                                   vtkClientCompositeManager::GATHER_Z_RMI_TAG); 
    }
  else
    { // Other satellite processes wait for RMIs for root.
    this->Controller->AddRMI(vtkClientCompositeManagerRenderRMI, (void*)this, 
                                      vtkClientCompositeManager::RENDER_RMI_TAG); 
    this->Controller->AddRMI(vtkClientCompositeManagerGatherZBufferValueRMI, 
                                      (void*)this, 
                                      vtkClientCompositeManager::GATHER_Z_RMI_TAG); 
    }
}

//-------------------------------------------------------------------------
void vtkClientCompositeManager::SquirtCompress(vtkUnsignedCharArray *in,
                                               vtkUnsignedCharArray *out,
                                               int compress_level)
{

  if (in->GetNumberOfComponents() != 4)
    {
    vtkErrorMacro("Squirt only works with RGBA");
    return;
    }

  int count=0;
  int index=0;
  int comp_index=0;
  int end_index;
  unsigned int current_color;
  unsigned char compress_masks[6][4] = {  {0xFF, 0xFF, 0xFF, 0xFF},
                                          {0xFE, 0xFF, 0xFE, 0xFF},
                                          {0xFC, 0xFE, 0xFC, 0xFF},
                                          {0xF8, 0xFC, 0xF8, 0xFF},
                                          {0xF0, 0xF8, 0xF0, 0xFF},
                                          {0xE0, 0xF0, 0xE0, 0xFF}};

  // Set bitmask based on compress_level
  unsigned int compress_mask;
  // I shifted the level by one so that 0 means no compression.
  memcpy(&compress_mask, &compress_masks[compress_level-1], 4);

  // Access raw arrays directly
  unsigned int* _rawColorBuffer;
  unsigned int* _rawCompressedBuffer;
  int numPixels = in->GetNumberOfTuples();
  _rawColorBuffer = (unsigned int*)in->GetPointer(0);
  _rawCompressedBuffer = (unsigned int*)out->WritePointer(0,numPixels*4);
  end_index = numPixels;

  // Go through color buffer and put RLE format into compressed buffer
  while((index < end_index) && (comp_index < end_index)) 
    {
                
    // Record color
    current_color = _rawCompressedBuffer[comp_index] =_rawColorBuffer[index];
    index++;

    // Compute Run
    while(((current_color&compress_mask) == (_rawColorBuffer[index]&compress_mask)) &&
          (index<end_index) && (count<255))
      { 
      index++; count++;   
      }

    // Record Run length
    *((unsigned char*)_rawCompressedBuffer+comp_index*4+3) =(unsigned char)count;
    comp_index++;

    count = 0;
    
    }

    // Back to vtk arrays :)
    out->SetNumberOfTuples(comp_index);
 
}

//------------------------------------------------------------
void vtkClientCompositeManager::SquirtDecompress(vtkUnsignedCharArray *in,
                                                  vtkUnsignedCharArray *out)
{
  int count=0;
  int index=0;
  unsigned int current_color;
  unsigned int* _rawColorBuffer;
  unsigned int* _rawCompressedBuffer;

  // Get compressed buffer size
  int CompSize = in->GetNumberOfTuples();

  // Access raw arrays directly
  _rawColorBuffer = (unsigned int*)out->GetPointer(0);
  _rawCompressedBuffer = (unsigned int*)in->GetPointer(0);

  // Go through compress buffer and extract RLE format into color buffer
  for(int i=0; i<CompSize; i++)
    {
    // Get color and count
    current_color = _rawCompressedBuffer[i];

    // Get run length count;
    count = *((unsigned char*)&current_color+3);

    // Fixed Alpha
    *((unsigned char*)&current_color+3) = 0xFF;

    // Set color
    _rawColorBuffer[index++] = current_color;

    // Blast color into color buffer
    for(int j=0; j< count; j++)
      _rawColorBuffer[index++] = current_color;
    }

    // Save out compression stats.
    vtkTimerLog::FormatAndMarkEvent("Squirt ratio: %f", (float)CompSize/(float)index);
}


//-------------------------------------------------------------------------
void vtkClientCompositeManager::DeltaEncode(vtkUnsignedCharArray *buf)
{
  int idx;
  int numPixels = buf->GetNumberOfTuples();
  unsigned char* ptr1;
  unsigned char* ptr2;
  short a, b, c;

  if (this->BaseArray == NULL)
    {
    this->BaseArray = vtkUnsignedCharArray::New();
    this->BaseArray->SetNumberOfComponents(4);
    this->BaseArray->SetNumberOfTuples(numPixels);
    ptr1 = this->BaseArray->GetPointer(0);
    memset(ptr1, 0, numPixels*4);
    }
  if (this->BaseArray->GetNumberOfTuples() != numPixels)
    {
    this->BaseArray->SetNumberOfTuples(numPixels);
    ptr1 = this->BaseArray->GetPointer(0);
    memset(ptr1, 0, numPixels*4);
    }
  ptr1 = this->BaseArray->GetPointer(0);  
  ptr2 = buf->GetPointer(0);
  for (idx = 0; idx < numPixels; ++idx)
    {
    a = ptr1[0];
    b = ptr2[0];
    c = b-a + 256;
    c = c >> 1;
    if (c > 255) {c = 255;}
    if (c < 0) {c = 0;} 
    ptr2[0] = (unsigned char)(c);
    c = c << 1;
    ptr1[0] = (unsigned char)(c + a - 255);

    a = ptr1[1];
    b = ptr2[1];
    c = b-a + 256;
    c = c >> 1;
    if (c > 255) {c = 255;}
    if (c < 0) {c = 0;}
    ptr2[1] = (unsigned char)(c);
    c = c << 1;
    ptr1[1] = (unsigned char)(c + a - 255);

    a = ptr1[2];
    b = ptr2[2];
    c = b-a + 256;
    c = c >> 1;
    if (c > 255) {c = 255;}
    if (c < 0) {c = 0;}
    ptr2[2] = (unsigned char)(c);
    c = c << 1;
    ptr1[2] = (unsigned char)(c + a - 255);

    ptr1 += 4;
    ptr2 += 4;
    }
}

//-------------------------------------------------------------------------
void vtkClientCompositeManager::DeltaDecode(vtkUnsignedCharArray *buf)
{
  int idx;
  int numPixels = buf->GetNumberOfTuples();
  unsigned char* ptr1;
  unsigned char* ptr2;
  short dif;

  if (this->BaseArray == NULL)
    {
    this->BaseArray = vtkUnsignedCharArray::New();
    this->BaseArray->SetNumberOfComponents(4);
    this->BaseArray->SetNumberOfTuples(numPixels);
    ptr1 = this->BaseArray->GetPointer(0);
    memset(ptr1, 0, numPixels*4);
    }
  if (this->BaseArray->GetNumberOfTuples() != numPixels)
    {
    this->BaseArray->SetNumberOfTuples(numPixels);
    ptr1 = this->BaseArray->GetPointer(0);
    memset(ptr1, 0, numPixels*4);
    }
  ptr1 = this->BaseArray->GetPointer(0);  
  ptr2 = buf->GetPointer(0);
  for (idx = 0; idx < numPixels; ++idx)
    {
    dif = (short)(ptr2[0]);
    dif = dif << 1;
    dif = dif + (short)(ptr1[0]) - 255;
    ptr2[0] = ptr1[0] = (unsigned char)(dif);

    dif = (short)(ptr2[1]);
    dif = dif << 1;
    dif = dif + (short)(ptr1[1]) - 255;
    ptr2[1] = ptr1[1] = (unsigned char)(dif);

    dif = (short)(ptr2[2]);
    dif = dif << 1;
    dif = dif + (short)(ptr1[2]) - 255;
    ptr2[2] = ptr1[2] = (unsigned char)(dif);

    ptr1 += 4;
    ptr2 += 4;
    }
}

//----------------------------------------------------------------------------
void vtkClientCompositeManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  
  os << indent << "ImageReductionFactor: " << this->ImageReductionFactor
     << endl;
  
  os << indent << "ClientController: (" << this->ClientController << ")\n"; 
  
  os << indent << "UseRGB: " << this->UseRGB << endl;
  os << indent << "SquirtLevel: " << this->SquirtLevel << endl;
  os << indent << "ClientFlag: " << this->ClientFlag << endl;

  os << indent << "Compositer: " << this->Compositer << endl;

}



