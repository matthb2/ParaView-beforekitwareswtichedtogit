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
#include "vtkRenderer.h"

#include "vtkAssemblyNode.h"
#include "vtkAssemblyPath.h"
#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkCuller.h"
#include "vtkCullerCollection.h"
#include "vtkFrustumCoverageCuller.h"
#include "vtkGraphicsFactory.h"
#include "vtkLight.h"
#include "vtkLightCollection.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkOutputWindow.h"
#include "vtkPicker.h"
#include "vtkProp3DCollection.h"
#include "vtkRenderWindow.h"
#include "vtkTimerLog.h"
#include "vtkVolume.h"

vtkCxxRevisionMacro(vtkRenderer, "$Revision$");

//----------------------------------------------------------------------------
// Needed when we don't use the vtkStandardNewMacro.
vtkInstantiatorNewMacro(vtkRenderer);
//----------------------------------------------------------------------------

// Create a vtkRenderer with a black background, a white ambient light, 
// two-sided lighting turned on, a viewport of (0,0,1,1), and backface culling
// turned off.
vtkRenderer::vtkRenderer()
{
  this->PickedProp   = NULL;
  this->ActiveCamera = NULL;

  this->Ambient[0] = 1;
  this->Ambient[1] = 1;
  this->Ambient[2] = 1;

  this->AllocatedRenderTime = 100;
  this->TimeFactor = 1.0;
  
  this->CreatedLight = NULL;
  this->AutomaticLightCreation = 1;
  
  this->TwoSidedLighting        = 1;
  this->BackingStore            = 0;
  this->BackingImage            = NULL;
  this->LastRenderTimeInSeconds = -1.0;
  
  this->RenderWindow = NULL;
  this->Lights  =  vtkLightCollection::New();
  this->Actors  =  vtkActorCollection::New();
  this->Volumes = vtkVolumeCollection::New();

  this->LightFollowCamera = 1;

  this->NumberOfPropsRendered = 0;

  this->PropArray                = NULL;

  this->Layer                    = 0;
  this->Interactive              = 1;
  this->Cullers = vtkCullerCollection::New();  
  vtkFrustumCoverageCuller *cull = vtkFrustumCoverageCuller::New();
  this->Cullers->AddItem(cull);
  cull->Delete();  
  this->NearClippingPlaneTolerance = 0.01;

  this->Erase = 1;
}

vtkRenderer::~vtkRenderer()
{
  this->SetRenderWindow( NULL );
  
  if (this->ActiveCamera)
    {
    this->ActiveCamera->UnRegister(this);
    this->ActiveCamera = NULL;
    }

  if (this->CreatedLight)
    {
    this->CreatedLight->UnRegister(this);
    this->CreatedLight = NULL;
    }

  if (this->BackingImage)
    {
    delete [] this->BackingImage;
    }
  
  this->Actors->Delete();
  this->Actors = NULL;
  this->Volumes->Delete();
  this->Volumes = NULL;
  this->Lights->Delete();
  this->Lights = NULL;
  this->Cullers->Delete();
  this->Cullers = NULL;
}

// return the correct type of Renderer 
vtkRenderer *vtkRenderer::New()
{ 
  // First try to create the object from the vtkObjectFactory
  vtkObject* ret = vtkGraphicsFactory::CreateInstance("vtkRenderer");
  return (vtkRenderer *)ret;
}

// Concrete render method.
void vtkRenderer::Render(void)
{
  double   t1, t2;
  int      i;
  vtkProp  *aProp;

  t1 = vtkTimerLog::GetCurrentTime();

  this->InvokeEvent(vtkCommand::StartEvent,NULL);

  // if backing store is on and we have a stored image
  if (this->BackingStore && this->BackingImage &&
      this->MTime < this->RenderTime &&
      this->ActiveCamera->GetMTime() < this->RenderTime &&
      this->RenderWindow->GetMTime() < this->RenderTime)
    {
    int mods = 0;
    vtkLight *light;
    
    // now we just need to check the lights and actors
    vtkCollectionSimpleIterator sit;
    for(this->Lights->InitTraversal(sit); 
        (light = this->Lights->GetNextLight(sit)); )
      {
      if (light->GetSwitch() && 
          light->GetMTime() > this->RenderTime)
        {
        mods = 1;
        goto completed_mod_check;
        }
      }
    vtkCollectionSimpleIterator pit;
    for (this->Props->InitTraversal(pit); 
         (aProp = this->Props->GetNextProp(pit)); )
      {
      // if it's invisible, we can skip the rest 
      if (aProp->GetVisibility())
        {
        if (aProp->GetRedrawMTime() > this->RenderTime)
          {
          mods = 1;
          goto completed_mod_check;
          }
        }
      }
    
    completed_mod_check:

    if (!mods)
      {
      int rx1, ry1, rx2, ry2;
      
      // backing store should be OK, lets use it
      // calc the pixel range for the renderer
      rx1 = (int)(this->Viewport[0]*(this->RenderWindow->GetSize()[0] - 1));
      ry1 = (int)(this->Viewport[1]*(this->RenderWindow->GetSize()[1] - 1));
      rx2 = (int)(this->Viewport[2]*(this->RenderWindow->GetSize()[0] - 1));
      ry2 = (int)(this->Viewport[3]*(this->RenderWindow->GetSize()[1] - 1));
      this->RenderWindow->SetPixelData(rx1,ry1,rx2,ry2,this->BackingImage,0);
      this->InvokeEvent(vtkCommand::EndEvent,NULL);
      return;
      }
    }

  // Create the initial list of visible props
  // This will be passed through AllocateTime(), where
  // a time is allocated for each prop, and the list
  // maybe re-ordered by the cullers. Also create the
  // sublists for the props that need ray casting, and
  // the props that need to be rendered into an image.
  // Fill these in later (in AllocateTime) - get a 
  // count of them there too
  if ( this->Props->GetNumberOfItems() > 0 )
    {
    this->PropArray = new vtkProp *[this->Props->GetNumberOfItems()];
    }
  else
    {
    this->PropArray = NULL;
    }

  this->PropArrayCount = 0;
  vtkCollectionSimpleIterator pit;
  for ( i = 0, this->Props->InitTraversal(pit); 
        (aProp = this->Props->GetNextProp(pit));i++ )
    {
    if ( aProp->GetVisibility() )
      {
      this->PropArray[this->PropArrayCount++] = aProp;
      }
    }
  
  if ( this->PropArrayCount == 0 )
    {
    vtkDebugMacro( << "There are no visible props!" );
    }
  else
    {
    // Call all the culling methods to set allocated time
    // for each prop and re-order the prop list if desired

    this->AllocateTime();
    }

  // do the render library specific stuff
  this->DeviceRender();

  // If we aborted, restore old estimated times
  // Setting the allocated render time to zero also sets the 
  // estimated render time to zero, so that when we add back
  // in the old value we have set it correctly.
  if ( this->RenderWindow->GetAbortRender() )
    {
    for ( i = 0; i < this->PropArrayCount; i++ )
      {
      this->PropArray[i]->RestoreEstimatedRenderTime();
      }
    }

  // Clean up the space we allocated before. If the PropArray exists,
  // they all should exist
  if ( this->PropArray)
    {
    delete [] this->PropArray;
    this->PropArray                = NULL;
    }

  if (this->BackingStore)
    {
    if (this->BackingImage)
      {
      delete [] this->BackingImage;
      }
    
    int rx1, ry1, rx2, ry2;
    
    // backing store should be OK, lets use it
    // calc the pixel range for the renderer
    rx1 = (int)(this->Viewport[0]*(this->RenderWindow->GetSize()[0] - 1));
    ry1 = (int)(this->Viewport[1]*(this->RenderWindow->GetSize()[1] - 1));
    rx2 = (int)(this->Viewport[2]*(this->RenderWindow->GetSize()[0] - 1));
    ry2 = (int)(this->Viewport[3]*(this->RenderWindow->GetSize()[1] - 1));
    this->BackingImage = this->RenderWindow->GetPixelData(rx1,ry1,rx2,ry2,0);
    }
    

  // If we aborted, do not record the last render time.
  // Lets play around with determining the acuracy of the 
  // EstimatedRenderTimes.  We can try to adjust for bad 
  // estimates with the TimeFactor.
  if ( ! this->RenderWindow->GetAbortRender() )
    {
    // Measure the actual RenderTime
    t2 = vtkTimerLog::GetCurrentTime();
    this->LastRenderTimeInSeconds = (double) (t2 - t1);

    if (this->LastRenderTimeInSeconds == 0.0)
      {
      this->LastRenderTimeInSeconds = 0.0001;
      }
    this->TimeFactor = this->AllocatedRenderTime/this->LastRenderTimeInSeconds;
    }
}

double vtkRenderer::GetAllocatedRenderTime()
{
  return this->AllocatedRenderTime;
}

double vtkRenderer::GetTimeFactor()
{
  return this->TimeFactor;
}

// Ask active camera to load its view matrix.
int vtkRenderer::UpdateCamera ()
{
  if (!this->ActiveCamera)
    {
    vtkDebugMacro(<< "No cameras are on, creating one.");
    // the get method will automagically create a camera
    // and reset it since one hasn't been specified yet
    this->GetActiveCamera();
    }

  // update the viewing transformation
  this->ActiveCamera->Render((vtkRenderer *)this); 
  
  return 1;
}

int vtkRenderer::UpdateLightsGeometryToFollowCamera()
{
  vtkCamera *camera;
  vtkLight *light;
  vtkMatrix4x4 *lightMatrix;

  // only update the light's geometry if this Renderer is tracking
  // this lights.  That allows one renderer to view the lights that
  // another renderer is setting up.
  camera = this->GetActiveCamera();
  lightMatrix = camera->GetCameraLightTransformMatrix();

  vtkCollectionSimpleIterator sit;
  for(this->Lights->InitTraversal(sit); 
      (light = this->Lights->GetNextLight(sit)); )
    {
    if (light->LightTypeIsSceneLight())
      {
      // Do nothing. Don't reset the transform matrix because applications
      // may have set a custom matrix. Only reset the transform matrix in
      // vtkLight::SetLightTypeToSceneLight()
      }
    else if (light->LightTypeIsHeadlight())
      {
      // update position and orientation of light to match camera.
      light->SetPosition(camera->GetPosition());
      light->SetFocalPoint(camera->GetFocalPoint());
      }
    else if (light->LightTypeIsCameraLight())
      {
      light->SetTransformMatrix(lightMatrix);
      }
    else 
      {
      vtkErrorMacro(<< "light has unknown light type");
      }
    }
  return 1;
}

int vtkRenderer::UpdateLightGeometry()
{
  if (this->LightFollowCamera) 
    {
    // only update the light's geometry if this Renderer is tracking
    // this lights.  That allows one renderer to view the lights that
    // another renderer is setting up.
  
    return this->UpdateLightsGeometryToFollowCamera();
    }
  
  return 1;
}

// Do all outer culling to set allocated time for each prop.
// Possibly re-order the actor list.
void vtkRenderer::AllocateTime()
{
  int          initialized = 0;
  double        renderTime;
  double        totalTime;
  int          i;
  vtkCuller    *aCuller;
  vtkProp      *aProp;

  // Give each of the cullers a chance to modify allocated rendering time
  // for the entire set of props. Each culler returns the total time given
  // by AllocatedRenderTime for all props. Each culler is required to
  // place any props that have an allocated render time of 0.0 
  // at the end of the list. The PropArrayCount value that is
  // returned is the number of non-zero, visible actors.
  // Some cullers may do additional sorting of the list (by distance,
  // importance, etc).
  //
  // The first culler will initialize all the allocated render times. 
  // Any subsequent culling will multiply the new render time by the 
  // existing render time for an actor.

  totalTime = this->PropArrayCount;
  this->ComputeAspect();

  vtkCollectionSimpleIterator sit;    
  for (this->Cullers->InitTraversal(sit); 
       (aCuller=this->Cullers->GetNextCuller(sit));)
    {
    totalTime = 
      aCuller->Cull((vtkRenderer *)this, 
                    this->PropArray, this->PropArrayCount,
                    initialized );
    }

  // loop through all props and set the AllocatedRenderTime
  for ( i = 0; i < this->PropArrayCount; i++ )
    {
    aProp = this->PropArray[i];

    // If we don't have an outer cull method in any of the cullers,
    // then the allocated render time has not yet been initialized
    renderTime = (initialized)?(aProp->GetRenderTimeMultiplier()):(1.0);

    // We need to divide by total time so that the total rendering time
    // (all prop's AllocatedRenderTime added together) would be equal
    // to the renderer's AllocatedRenderTime.
    aProp->
      SetAllocatedRenderTime(( renderTime / totalTime ) * 
                             this->AllocatedRenderTime, 
                             this );  
    }
}

// Ask actors to render themselves. As a side effect will cause 
// visualization network to update.
int vtkRenderer::UpdateGeometry()
{
  int        i;
  
  this->NumberOfPropsRendered = 0;

  if ( this->PropArrayCount == 0 ) 
    {
    this->InvokeEvent(vtkCommand::EndEvent,NULL);
    return 0;
    }

  // We can render everything because if it was
  // not visible it would not have been put in the
  // list in the first place, and if it was allocated
  // no time (culled) it would have been removed from
  // the list
  
  // loop through props and give them a chance to 
  // render themselves as opaque geometry
  for ( i = 0; i < this->PropArrayCount; i++ )
    {    
    this->NumberOfPropsRendered += 
      this->PropArray[i]->RenderOpaqueGeometry(this);
    }
 
  
  // loop through props and give them a chance to 
  // render themselves as translucent geometry
  for ( i = 0; i < this->PropArrayCount; i++ )
    {
    this->NumberOfPropsRendered += 
      this->PropArray[i]->RenderTranslucentGeometry(this);
    }

  // loop through props and give them a chance to 
  // render themselves as an overlay (or underlay)
  for ( i = 0; i < this->PropArrayCount; i++ )
    {
    this->NumberOfPropsRendered += 
      this->PropArray[i]->RenderOverlay(this);
    }

  this->InvokeEvent(vtkCommand::EndEvent,NULL);
  this->RenderTime.Modified();

  vtkDebugMacro( << "Rendered " << 
                    this->NumberOfPropsRendered << " actors" );

  return  this->NumberOfPropsRendered;
}

vtkWindow *vtkRenderer::GetVTKWindow()
{
  return this->RenderWindow;
}

// Specify the camera to use for this renderer.
void vtkRenderer::SetActiveCamera(vtkCamera *cam)
{
  if (this->ActiveCamera == cam)
    {
    return;
    }

  if (this->ActiveCamera)
    {
    this->ActiveCamera->UnRegister(this);
    this->ActiveCamera = NULL;
    }
  if (cam)
    {
    cam->Register(this);
    }

  this->ActiveCamera = cam;
  this->Modified();
}


vtkCamera* vtkRenderer::MakeCamera()
{
  return vtkCamera::New();
}
  
// Get the current camera.
vtkCamera *vtkRenderer::GetActiveCamera()
{
  if ( this->ActiveCamera == NULL )
    {
    vtkCamera *cam = this->MakeCamera();
    this->SetActiveCamera(cam);
    cam->Delete();
    this->ResetCamera();
    }

  return this->ActiveCamera;
}

// Add a light to the list of lights.
void vtkRenderer::AddLight(vtkLight *light)
{
  this->Lights->AddItem(light);
}

// look through the props and get all the actors
vtkActorCollection *vtkRenderer::GetActors()
{
  vtkProp *aProp;
  
  // clear the collection first
  this->Actors->RemoveAllItems();
  
  vtkCollectionSimpleIterator pit;
  for (this->Props->InitTraversal(pit); 
       (aProp = this->Props->GetNextProp(pit)); )
    {
    aProp->GetActors(this->Actors);
    }
  return this->Actors;
}

// look through the props and get all the volumes
vtkVolumeCollection *vtkRenderer::GetVolumes()
{
  vtkProp *aProp;
  
  // clear the collection first
  this->Volumes->RemoveAllItems();
  
  vtkCollectionSimpleIterator pit;
  for (this->Props->InitTraversal(pit); 
       (aProp = this->Props->GetNextProp(pit)); )
    {
    aProp->GetVolumes(this->Volumes);
    }
  return this->Volumes;
}

// Remove a light from the list of lights.
void vtkRenderer::RemoveLight(vtkLight *light)
{
  this->Lights->RemoveItem(light);
}

// Add an culler to the list of cullers.
void vtkRenderer::AddCuller(vtkCuller *culler)
{
  this->Cullers->AddItem(culler);
}

// Remove an actor from the list of cullers.
void vtkRenderer::RemoveCuller(vtkCuller *culler)
{
  this->Cullers->RemoveItem(culler);
}

vtkLight *vtkRenderer::MakeLight()
{
  return vtkLight::New();
}

void vtkRenderer::CreateLight(void)
{
  if ( !this->AutomaticLightCreation )
    {
    return;
    }

  if (this->CreatedLight)
    {
    this->CreatedLight->UnRegister(this);
    this->CreatedLight = NULL;
    }

  // I do not see why UnRegister is used on CreatedLight, but lets be
  // consistent.
  vtkLight *l = this->MakeLight();
  this->CreatedLight = l;
  this->CreatedLight->Register(this);
  this->AddLight(this->CreatedLight);
  l->Delete();
  l = NULL;

  this->CreatedLight->SetLightTypeToHeadlight();

  // set these values just to have a good default should LightFollowCamera
  // be turned off.
  this->CreatedLight->SetPosition(this->GetActiveCamera()->GetPosition());
  this->CreatedLight->SetFocalPoint(this->GetActiveCamera()->GetFocalPoint());
}

// Compute the bounds of the visible props
void vtkRenderer::ComputeVisiblePropBounds( double allBounds[6] )
{
  vtkProp    *prop;
  double      *bounds;
  int        nothingVisible=1;

  allBounds[0] = allBounds[2] = allBounds[4] = VTK_DOUBLE_MAX;
  allBounds[1] = allBounds[3] = allBounds[5] = -VTK_DOUBLE_MAX;
  
  // loop through all props
  vtkCollectionSimpleIterator pit;
  for (this->Props->InitTraversal(pit); 
       (prop = this->Props->GetNextProp(pit)); )
    {
    // if it's invisible, or has no geometry, we can skip the rest 
    if ( prop->GetVisibility() )
      {
      bounds = prop->GetBounds();
      // make sure we haven't got bogus bounds
      if ( bounds != NULL && vtkMath::AreBoundsInitialized(bounds))
        {
        nothingVisible = 0;
        
        if (bounds[0] < allBounds[0])
          {
          allBounds[0] = bounds[0]; 
          }
        if (bounds[1] > allBounds[1])
          {
          allBounds[1] = bounds[1]; 
          }
        if (bounds[2] < allBounds[2])
          {
          allBounds[2] = bounds[2]; 
          }
        if (bounds[3] > allBounds[3])
          {
          allBounds[3] = bounds[3]; 
          }
        if (bounds[4] < allBounds[4])
          {
          allBounds[4] = bounds[4]; 
          }
        if (bounds[5] > allBounds[5])
          {
          allBounds[5] = bounds[5]; 
          }
        }//not bogus
      }
    }
  
  if ( nothingVisible )
    {
    vtkMath::UninitializeBounds(allBounds);
    vtkDebugMacro(<< "Can't compute bounds, no 3D props are visible");
    return;
    }
}

double *vtkRenderer::ComputeVisiblePropBounds()
  {
  this->ComputeVisiblePropBounds(this->ComputedVisiblePropBounds);
  return this->ComputedVisiblePropBounds;
  }

// Automatically set up the camera based on the visible actors.
// The camera will reposition itself to view the center point of the actors,
// and move along its initial view plane normal (i.e., vector defined from 
// camera position to focal point) so that all of the actors can be seen.
void vtkRenderer::ResetCamera()
{
  double      allBounds[6];

  this->ComputeVisiblePropBounds( allBounds );

  if (!vtkMath::AreBoundsInitialized(allBounds))
    {
    vtkDebugMacro( << "Cannot reset camera!" );
    }
  else
    {
    this->ResetCamera(allBounds);
    }

  // Here to let parallel/distributed compositing intercept 
  // and do the right thing.
  this->InvokeEvent(vtkCommand::ResetCameraEvent,this);
}

// Automatically set the clipping range of the camera based on the
// visible actors
void vtkRenderer::ResetCameraClippingRange()
{
  double      allBounds[6];

  this->ComputeVisiblePropBounds( allBounds );

  if (!vtkMath::AreBoundsInitialized(allBounds))
    {
    vtkDebugMacro( << "Cannot reset camera clipping range!" );
    }
  else
    {
    this->ResetCameraClippingRange(allBounds);
    }

  // Here to let parallel/distributed compositing intercept 
  // and do the right thing.
  this->InvokeEvent(vtkCommand::ResetCameraClippingRangeEvent,this);
}


// Automatically set up the camera based on a specified bounding box
// (xmin,xmax, ymin,ymax, zmin,zmax). Camera will reposition itself so
// that its focal point is the center of the bounding box, and adjust its
// distance and position to preserve its initial view plane normal 
// (i.e., vector defined from camera position to focal point). Note: is 
// the view plane is parallel to the view up axis, the view up axis will
// be reset to one of the three coordinate axes.
void vtkRenderer::ResetCamera(double bounds[6])
{
  double center[3];
  double distance;
  double width;
  double vn[3], *vup;
  
  this->GetActiveCamera();
  if ( this->ActiveCamera != NULL )
    {
    this->ActiveCamera->GetViewPlaneNormal(vn);
    }
  else
    {
    vtkErrorMacro(<< "Trying to reset non-existant camera");
    return;
    }

  center[0] = (bounds[0] + bounds[1])/2.0;
  center[1] = (bounds[2] + bounds[3])/2.0;
  center[2] = (bounds[4] + bounds[5])/2.0;

  width = bounds[3] - bounds[2];
  if (width < (bounds[1] - bounds[0]))
    {
    width = bounds[1] - bounds[0];
    }
  
  // If we have just a single point, pick a width of 1.0
  width = (width==0)?(1.0):(width);
  
  distance = 
    0.8*width/tan(this->ActiveCamera->GetViewAngle()*vtkMath::Pi()/360.0);
  distance = distance + (bounds[5] - bounds[4])/2.0;

  // check view-up vector against view plane normal
  vup = this->ActiveCamera->GetViewUp();
  if ( fabs(vtkMath::Dot(vup,vn)) > 0.999 )
    {
    vtkWarningMacro(<<"Resetting view-up since view plane normal is parallel");
    this->ActiveCamera->SetViewUp(-vup[2], vup[0], vup[1]);
    }

  // update the camera
  this->ActiveCamera->SetFocalPoint(center[0],center[1],center[2]);
  this->ActiveCamera->SetPosition(center[0]+distance*vn[0],
                                  center[1]+distance*vn[1],
                                  center[2]+distance*vn[2]);

  this->ResetCameraClippingRange( bounds );

  // setup default parallel scale
  this->ActiveCamera->SetParallelScale(width);
}
  
// Alternative version of ResetCamera(bounds[6]);
void vtkRenderer::ResetCamera(double xmin, double xmax, double ymin, double ymax, 
                              double zmin, double zmax)
{
  double bounds[6];

  bounds[0] = xmin;
  bounds[1] = xmax;
  bounds[2] = ymin;
  bounds[3] = ymax;
  bounds[4] = zmin;
  bounds[5] = zmax;

  this->ResetCamera(bounds);
}

// Reset the camera clipping range to include this entire bounding box
void vtkRenderer::ResetCameraClippingRange( double bounds[6] )
{
  double  vn[3], position[3], a, b, c, d;
  double  range[2], dist;
  int     i, j, k;

  // Don't reset the clipping range when we don't have any 3D visible props
  if (!vtkMath::AreBoundsInitialized(bounds))
    {
    return;
    }
  
  this->GetActiveCamera();
  if ( this->ActiveCamera == NULL )
    {
    vtkErrorMacro(<< "Trying to reset clipping range of non-existant camera");
    return;
    }
  
  // Find the plane equation for the camera view plane
  this->ActiveCamera->GetViewPlaneNormal(vn);
  this->ActiveCamera->GetPosition(position);
  a = -vn[0];
  b = -vn[1];
  c = -vn[2];
  d = -(a*position[0] + b*position[1] + c*position[2]);

  // Set the max near clipping plane and the min far clipping plane
  range[0] = a*bounds[0] + b*bounds[2] + c*bounds[4] + d;
  range[1] = 1e-18;

  // Find the closest / farthest bounding box vertex
  for ( k = 0; k < 2; k++ )
    {
    for ( j = 0; j < 2; j++ )
      {
      for ( i = 0; i < 2; i++ )
        {
        dist = a*bounds[i] + b*bounds[2+j] + c*bounds[4+k] + d;
        range[0] = (dist<range[0])?(dist):(range[0]);
        range[1] = (dist>range[1])?(dist):(range[1]);
        }
      }
    }
  
  // Do not let the range behind the camera throw off the calculation.
  if (range[0] < 0.0)
    {
    range[0] = 0.0;
    }

  // Give ourselves a little breathing room
  range[0] = 0.99*range[0] - (range[1] - range[0])*0.5;
  range[1] = 1.01*range[1] + (range[1] - range[0])*0.5;

  // Make sure near is not bigger than far
  range[0] = (range[0] >= range[1])?(0.01*range[1]):(range[0]);

  // Make sure near is at least some fraction of far - this prevents near
  // from being behind the camera or too close in front. How close is too
  // close depends on the resolution of the depth buffer
  int ZBufferDepth = 16;
  if (this->RenderWindow)
    {
      ZBufferDepth = this->RenderWindow->GetDepthBufferSize();
    }
  //
  if ( ZBufferDepth <= 16 )
    {
    range[0] = (range[0] < this->NearClippingPlaneTolerance*range[1])?(this->NearClippingPlaneTolerance*range[1]):(range[0]);
    }
  else if ( ZBufferDepth <= 24 )
    {
    range[0] = (range[0] < this->NearClippingPlaneTolerance*range[1])?(this->NearClippingPlaneTolerance*range[1]):(range[0]);
    }
  else
    {
    range[0] = (range[0] < this->NearClippingPlaneTolerance*range[1])?(this->NearClippingPlaneTolerance*range[1]):(range[0]);
    }

  this->ActiveCamera->SetClippingRange( range );
}

// Alternative version of ResetCameraClippingRange(bounds[6]);
void vtkRenderer::ResetCameraClippingRange(double xmin, double xmax, 
                                           double ymin, double ymax, 
                                           double zmin, double zmax)
{
  double bounds[6];

  bounds[0] = xmin;
  bounds[1] = xmax;
  bounds[2] = ymin;
  bounds[3] = ymax;
  bounds[4] = zmin;
  bounds[5] = zmax;

  this->ResetCameraClippingRange(bounds);
}

// Specify the rendering window in which to draw. This is automatically set
// when the renderer is created by MakeRenderer.  The user probably
// shouldn't ever need to call this method.
// no reference counting!
void vtkRenderer::SetRenderWindow(vtkRenderWindow *renwin)
{
  vtkProp *aProp;
  
  if (renwin != this->RenderWindow)
    {
    // This renderer is be dis-associated with its previous render window.
    // this information needs to be passed to the renderer's actors and
    // volumes so they can release and render window specific (or graphics
    // context specific) information (such as display lists and texture ids)
    vtkCollectionSimpleIterator pit;
    this->Props->InitTraversal(pit);
    for ( aProp = this->Props->GetNextProp(pit);
          aProp != NULL;
          aProp = this->Props->GetNextProp(pit) )
      {
      aProp->ReleaseGraphicsResources(this->RenderWindow);
      }
    // what about lights?
    // what about cullers?
    
    }
  this->VTKWindow = renwin;
  this->RenderWindow = renwin;
}

// Given a pixel location, return the Z value
double vtkRenderer::GetZ (int x, int y)
{
  float *zPtr;
  double z;

  zPtr = this->RenderWindow->GetZbufferData (x, y, x, y);
  if (zPtr)
    {
    z = *zPtr;
    delete [] zPtr;
    }
  else
    {
    z = 1.0;
    }
  return z;
}


// Convert view point coordinates to world coordinates.
void vtkRenderer::ViewToWorld()
{
  double result[4];
  result[0] = this->ViewPoint[0];
  result[1] = this->ViewPoint[1];
  result[2] = this->ViewPoint[2];
  result[3] = 1.0;
  this->ViewToWorld(result[0],result[1],result[2]);
  this->SetWorldPoint(result);
}

void vtkRenderer::ViewToWorld(double &x, double &y, double &z)
{
  vtkMatrix4x4 *mat = vtkMatrix4x4::New();
  double result[4];

  // get the perspective transformation from the active camera 
  mat->DeepCopy(this->ActiveCamera->
                GetCompositePerspectiveTransformMatrix(
                  this->GetTiledAspectRatio(),0,1));
  
  // use the inverse matrix 
  mat->Invert();
 
  // Transform point to world coordinates 
  result[0] = x;
  result[1] = y;
  result[2] = z;
  result[3] = 1.0;

  mat->MultiplyPoint(result,result);
  
  // Get the transformed vector & set WorldPoint 
  // while we are at it try to keep w at one
  if (result[3])
    {
    x = result[0] / result[3];
    y = result[1] / result[3];
    z = result[2] / result[3];
    }
  mat->Delete();
}

// Convert world point coordinates to view coordinates.
void vtkRenderer::WorldToView()
{
  double result[3];
  result[0] = this->WorldPoint[0];
  result[1] = this->WorldPoint[1];
  result[2] = this->WorldPoint[2];
  this->WorldToView(result[0], result[1], result[2]);
  this->SetViewPoint(result[0], result[1], result[2]);
}

// Convert world point coordinates to view coordinates.
void vtkRenderer::WorldToView(double &x, double &y, double &z)
{
  vtkMatrix4x4 *matrix = vtkMatrix4x4::New();
  double     view[4];

  // get the perspective transformation from the active camera 
  matrix->DeepCopy(this->ActiveCamera->
                GetCompositePerspectiveTransformMatrix(
                  this->GetTiledAspectRatio(),0,1));

  view[0] = x*matrix->Element[0][0] + y*matrix->Element[0][1] +
    z*matrix->Element[0][2] + matrix->Element[0][3];
  view[1] = x*matrix->Element[1][0] + y*matrix->Element[1][1] +
    z*matrix->Element[1][2] + matrix->Element[1][3];
  view[2] = x*matrix->Element[2][0] + y*matrix->Element[2][1] +
    z*matrix->Element[2][2] + matrix->Element[2][3];
  view[3] = x*matrix->Element[3][0] + y*matrix->Element[3][1] +
    z*matrix->Element[3][2] + matrix->Element[3][3];

  if (view[3] != 0.0)
    {
    x = view[0]/view[3];
    y = view[1]/view[3];
    z = view[2]/view[3];
    }
  matrix->Delete();
}

void vtkRenderer::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Near Clipping Plane Tolerance: " 
     << this->NearClippingPlaneTolerance << "\n";

  os << indent << "Ambient: (" << this->Ambient[0] << ", " 
     << this->Ambient[1] << ", " << this->Ambient[2] << ")\n";

  os << indent << "Backing Store: " << (this->BackingStore ? "On\n":"Off\n");
  os << indent << "Display Point: ("  << this->DisplayPoint[0] << ", " 
    << this->DisplayPoint[1] << ", " << this->DisplayPoint[2] << ")\n";
  os << indent << "Lights:\n";
  this->Lights->PrintSelf(os,indent.GetNextIndent());

  os << indent << "Light Follow Camera: "
     << (this->LightFollowCamera ? "On\n" : "Off\n");

  os << indent << "View Point: (" << this->ViewPoint[0] << ", " 
    << this->ViewPoint[1] << ", " << this->ViewPoint[2] << ")\n";

  os << indent << "Two Sided Lighting: " 
     << (this->TwoSidedLighting ? "On\n" : "Off\n");

  os << indent << "Automatic Light Creation: " 
     << (this->AutomaticLightCreation ? "On\n" : "Off\n");

  os << indent << "Layer = " << this->Layer << "\n";
  os << indent << "Interactive = " << (this->Interactive ? "On" : "Off") 
     << "\n";

  os << indent << "Allocated Render Time: " << this->AllocatedRenderTime
     << "\n";

  os << indent << "Last Time To Render (Seconds): " 
     << this->LastRenderTimeInSeconds << endl;
  os << indent << "TimeFactor: " << this->TimeFactor << endl;

  os << indent << "Erase: " 
     << (this->Erase ? "On\n" : "Off\n");

  // I don't want to print this since it is used just internally
  // os << indent << this->NumberOfPropsRendered;
}

int vtkRenderer::VisibleActorCount()
{
  vtkProp *aProp;
  int count = 0;

  // loop through Props
  vtkCollectionSimpleIterator pit;
  for (this->Props->InitTraversal(pit);
       (aProp = this->Props->GetNextProp(pit)); )
    {
    if (aProp->GetVisibility())
      {
      count++;
      }
    }
  return count;
}

int vtkRenderer::VisibleVolumeCount()
{
  int count = 0;
  vtkProp *aProp;

  // loop through volumes
  vtkCollectionSimpleIterator pit;
  for (this->Props->InitTraversal(pit); 
        (aProp = this->Props->GetNextProp(pit)); )
    {
    if (aProp->GetVisibility())
      {
      count++;
      }
    }
  return count;
}

unsigned long int vtkRenderer::GetMTime()
{
  unsigned long mTime=this-> vtkViewport::GetMTime();
  unsigned long time;

  if ( this->ActiveCamera != NULL )
    {
    time = this->ActiveCamera ->GetMTime();
    mTime = ( time > mTime ? time : mTime );
    }
  if ( this->CreatedLight != NULL )
    {
    time = this->CreatedLight ->GetMTime();
    mTime = ( time > mTime ? time : mTime );
    }

  return mTime;
}


vtkAssemblyPath* vtkRenderer::PickProp(double selectionX, double selectionY)
{
  // initialize picking information
  this->CurrentPickId = 1; // start at 1, so 0 can be a no pick
  this->PickX = selectionX;
  this->PickY = selectionY;
  int numberPickFrom;
  vtkPropCollection *props;

  // Initialize the pick (we're picking a path, the path 
  // includes info about nodes) 
  if (this->PickFromProps)
    {
    props = this->PickFromProps;
    }
  else
    {
    props = this->Props;
    }
  // number determined from number of rendering passes plus reserved "0" slot
  numberPickFrom = 2*props->GetNumberOfPaths()*3 + 1;
  
  this->IsPicking = 1; // turn on picking
  this->StartPick(numberPickFrom);
  this->PathArray = new vtkAssemblyPath *[numberPickFrom];
  this->PathArrayCount = 0;

  // Actually perform the pick
  this->PickRender(props);  // do the pick render
  this->IsPicking = 0; // turn off picking
  this->DonePick();
  vtkDebugMacro(<< "z value for pick " << this->GetPickedZ() << "\n");
  vtkDebugMacro(<< "pick time " <<  this->LastRenderTimeInSeconds << "\n");

  // Get the pick id of the object that was picked
  if ( this->PickedProp != NULL )
    {
    this->PickedProp->UnRegister(this);
    this->PickedProp = NULL;
    }
  unsigned int pickedId = this->GetPickedId();
  if ( pickedId != 0 )
    {
    pickedId--; // pick ids start at 1, so move back one

    // wrap around, as there are twice as many pickid's as PropArrayCount,
    // because each Prop has both RenderOpaqueGeometry and 
    // RenderTranslucentGeometry called on it
    pickedId = pickedId % this->PathArrayCount;
    this->PickedProp = this->PathArray[pickedId];
    this->PickedProp->Register(this);
    }

  // Clean up stuff from picking after we use it
  delete [] this->PathArray;
  this->PathArray = NULL;

  // Return the pick!
  return this->PickedProp; //returns an assembly path
}

// Do a render in pick mode.  This is normally done with rendering turned off.
// Before each Prop is rendered the pick id is incremented
void vtkRenderer::PickRender(vtkPropCollection *props)
{
  vtkProp  *aProp;
  vtkAssemblyPath *path;

  this->InvokeEvent(vtkCommand::StartEvent,NULL);
  if( props->GetNumberOfItems() <= 0)
    {
    return;
    }
  
  // Create a place to store all props that remain after culling
  vtkPropCollection* pickFrom = vtkPropCollection::New();

  // Extract all the prop3D's out of the props collection.
  // This collection will be further culled by using a bounding box
  // pick later (vtkPicker). Things that are not vtkProp3D will get 
  // put into the Paths list directly.
  vtkCollectionSimpleIterator pit;
  for (  props->InitTraversal(pit); (aProp = props->GetNextProp(pit)); )
    {
    if ( aProp->GetPickable() && aProp->GetVisibility() )
      {
      if ( aProp->IsA("vtkProp3D") )
        {
        pickFrom->AddItem(aProp);
        }
      else //must be some other type of prop (e.g., vtkActor2D)
        {
        for ( aProp->InitPathTraversal(); (path=aProp->GetNextPath()); )
          {
          this->PathArray[this->PathArrayCount++] = path;
          }
        }
      }//pickable & visible
    }//for all props

  // For a first pass at the pick process, just use a vtkPicker to
  // intersect with bounding boxes of the objects.  This should greatly
  // reduce the number of polygons that the hardware has to pick from, and
  // speeds things up substantially.
  //
  // Create a picker to do the culling process
  vtkPicker* cullPicker = vtkPicker::New();
  // Add each of the Actors from the pickFrom list into the picker
  for ( pickFrom->InitTraversal(pit); (aProp = pickFrom->GetNextProp(pit)); )
    {
    cullPicker->AddPickList(aProp);
    }

  // make sure this selects from the pickers list and not the renderers list
  cullPicker->PickFromListOn();

  // do the pick
  cullPicker->Pick(this->PickX, this->PickY, 0, this);
  vtkProp3DCollection* cullPicked = cullPicker->GetProp3Ds();

  // Put all the ones that were picked by the cull process
  // into the PathArray to be picked from
  vtkCollectionSimpleIterator p3dit;
  for (cullPicked->InitTraversal(p3dit); 
       (aProp = cullPicked->GetNextProp3D(p3dit));)
    {
    if ( aProp != NULL )
      {
      for ( aProp->InitPathTraversal(); (path=aProp->GetNextPath()); )
        {
        this->PathArray[this->PathArrayCount++] = path;
        }
      }
    }

  // Clean picking support objects up
  pickFrom->Delete();
  cullPicker->Delete();
  
  if ( this->PathArrayCount == 0 )
    {
    vtkDebugMacro( << "There are no visible props!" );
    return;
    }

  // do the render library specific pick render
  this->DevicePickRender();
}

void vtkRenderer::PickGeometry()
{  
  int        i;
  
  this->NumberOfPropsRendered = 0;

  if ( this->PathArrayCount == 0 ) 
    {
    return ;
    }

  // We can render everything because if it was
  // not visible it would not have been put in the
  // list in the first place, and if it was allocated
  // no time (culled) it would have been removed from
  // the list
  
  // loop through props and give them a change to 
  // render themselves as opaque geometry
  vtkProp *prop;
  vtkMatrix4x4 *matrix;
  for ( i = 0; i < this->PathArrayCount; i++ )
    {
    this->UpdatePickId();
    prop = this->PathArray[i]->GetLastNode()->GetProp();
    matrix = this->PathArray[i]->GetLastNode()->GetMatrix();
    prop->PokeMatrix(matrix);
    this->NumberOfPropsRendered += prop->RenderOpaqueGeometry(this);
    prop->PokeMatrix(NULL);
    }
 
  // loop through props and give them a chance to 
  // render themselves as translucent geometry
  for ( i = 0; i < this->PathArrayCount; i++ )
    {
    this->UpdatePickId();
    prop = this->PathArray[i]->GetLastNode()->GetProp();
    matrix = this->PathArray[i]->GetLastNode()->GetMatrix();
    prop->PokeMatrix(matrix);
    this->NumberOfPropsRendered += 
      prop->RenderTranslucentGeometry(this);
    prop->PokeMatrix(NULL);
    }

  for ( i = 0; i < this->PathArrayCount; i++ )
    {
    this->UpdatePickId();
    prop = this->PathArray[i]->GetLastNode()->GetProp();
    matrix = this->PathArray[i]->GetLastNode()->GetMatrix();
    prop->PokeMatrix(matrix);
    this->NumberOfPropsRendered += 
      prop->RenderOverlay(this);
    prop->PokeMatrix(NULL);
    }

  vtkDebugMacro( << "Pick Rendered " << 
                    this->NumberOfPropsRendered << " actors" );

}


int  vtkRenderer::Transparent()
{
  // If our layer is the 0th layer, then we are not transparent, else we are.
  return (this->Layer == 0 ? 0 : 1);
}

double vtkRenderer::GetTiledAspectRatio()
{
  int usize, vsize;
  this->GetTiledSize(&usize,&vsize);

  // some renderer subclasses may have more complicated computations for the
  // aspect ratio. SO take that into account by computing the difference
  // between our simple aspect ratio and what the actual renderer is
  // reporting.
  double aspect[2];
  this->ComputeAspect();
  this->GetAspect(aspect);
  double aspect2[2];
  this->vtkViewport::ComputeAspect();
  this->vtkViewport::GetAspect(aspect2);
  double aspectModification = aspect[0]*aspect2[1]/(aspect[1]*aspect2[0]);
  
  double finalAspect = 1.0;
  if(vsize && usize)
    {
    finalAspect = aspectModification*usize/vsize;
    }
  return finalAspect;
}
