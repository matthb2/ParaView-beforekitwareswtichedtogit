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
#include "vtkSMCameraLink.h"

#include "vtkCallbackCommand.h"
#include "vtkObjectFactory.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkPVXMLElement.h"
#include "vtkSmartPointer.h"
#include "vtkSMProperty.h"
#include "vtkSMRenderModuleProxy.h"
#include "vtkSMStateLoader.h"

#include <vtkstd/list>

vtkStandardNewMacro(vtkSMCameraLink);
vtkCxxRevisionMacro(vtkSMCameraLink, "$Revision$");

//---------------------------------------------------------------------------
struct vtkSMCameraLinkInternals
{
  static void UpdateViewCallback(vtkObject* caller, unsigned long eid, 
                                 void* clientData, void* callData)
    {
    vtkSMCameraLink* camLink = reinterpret_cast<vtkSMCameraLink*>(clientData);
    if(eid == vtkCommand::EndEvent && clientData && caller && callData)
      {
      int *interactive = reinterpret_cast<int*>(callData);
      camLink->UpdateViews(vtkSMProxy::SafeDownCast(caller), (*interactive==1));
      }
    else if (eid == vtkCommand::StartInteractionEvent && clientData && caller)
      {
      camLink->StartInteraction(caller);
      }
    else if (eid == vtkCommand::EndInteractionEvent && clientData && caller)
      {
      camLink->EndInteraction(caller);
      }
    }

  struct LinkedCamera
  {
    LinkedCamera(vtkSMProxy* proxy, vtkSMCameraLink* camLink) : 
      Proxy(proxy)
      {
      this->Observer = vtkSmartPointer<vtkCallbackCommand>::New();
      this->Observer->SetClientData(camLink);
      this->Observer->SetCallback(vtkSMCameraLinkInternals::UpdateViewCallback);
      proxy->AddObserver(vtkCommand::EndEvent, this->Observer);

      vtkSMRenderModuleProxy* rmp = vtkSMRenderModuleProxy::SafeDownCast(proxy);
      if (rmp)
        {
        vtkPVGenericRenderWindowInteractor* iren = rmp->GetInteractor();
        iren->AddObserver(vtkCommand::StartInteractionEvent, this->Observer);
        iren->AddObserver(vtkCommand::EndInteractionEvent, this->Observer); 
        }
      };
    ~LinkedCamera()
      {
      this->Proxy->RemoveObserver(this->Observer);
      vtkSMRenderModuleProxy* rmp = 
        vtkSMRenderModuleProxy::SafeDownCast(this->Proxy);
      if (rmp)
        {
        vtkPVGenericRenderWindowInteractor* iren = rmp->GetInteractor();
        iren->RemoveObserver(this->Observer);
        iren->RemoveObserver(this->Observer); 
        }
      }
    vtkSmartPointer<vtkSMProxy> Proxy;
    vtkSmartPointer<vtkCallbackCommand> Observer;

    LinkedCamera(const LinkedCamera&);
    LinkedCamera& operator=(const LinkedCamera&);
  };

  typedef vtkstd::list<LinkedCamera*> LinkedProxiesType;
  LinkedProxiesType LinkedProxies;

  bool Updating;

  static const char* LinkedPropertyNames[];

  vtkSMCameraLinkInternals()
    {
    this->Updating = false;
    }
  ~vtkSMCameraLinkInternals()
    {
    LinkedProxiesType::iterator iter;
    for(iter = this->LinkedProxies.begin(); iter != LinkedProxies.end(); ++iter)
      {
      delete *iter;
      }
    }
};

const char* vtkSMCameraLinkInternals::LinkedPropertyNames[] =
{
  /* from */  /* to */
  "CameraPositionInfo", "CameraPosition",
  "CameraFocalPointInfo", "CameraFocalPoint",
  "CameraViewUpInfo", "CameraViewUp",
  "CenterOfRotation", "CenterOfRotation",
  0
};

//---------------------------------------------------------------------------
vtkSMCameraLink::vtkSMCameraLink()
{
  this->Internals = new vtkSMCameraLinkInternals;
}

//---------------------------------------------------------------------------
vtkSMCameraLink::~vtkSMCameraLink()
{
  delete this->Internals;
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::AddLinkedProxy(vtkSMProxy* proxy, int updateDir)
{
  // must be render module to link cameras
  if(vtkSMRenderModuleProxy::SafeDownCast(proxy))
    {
    this->Superclass::AddLinkedProxy(proxy, updateDir);
    if(updateDir == vtkSMLink::INPUT)
      {
      proxy->CreateVTKObjects(1); 
        // ensure that the proxy is created.
        // This is necessary since when loading state the proxy may not yet be
        // created, however we want to observer events on the
        // interactor. 
      this->Internals->LinkedProxies.push_back(
        new vtkSMCameraLinkInternals::LinkedCamera(proxy, this));
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::RemoveLinkedProxy(vtkSMProxy* proxy)
{
  this->Superclass::RemoveLinkedProxy(proxy);

  vtkSMCameraLinkInternals::LinkedProxiesType::iterator iter;
  for(iter = this->Internals->LinkedProxies.begin();
      iter != this->Internals->LinkedProxies.end();
      ++iter)
    {
    if((*iter)->Proxy == proxy)
      {
      delete *iter;
      this->Internals->LinkedProxies.erase(iter);
      break;
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::UpdateProperties(vtkSMProxy* fromProxy, 
                                       const char* pname)
{
  if (pname && strcmp(pname, "CenterOfRotation") == 0)
    {
    // We are linking center of rotations for linked views as well.
    // Center of rotation is not changed during interaction, hence
    // we listen to center of rotation changed events explicitly.
    this->UpdateViews(fromProxy, false);
    }
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::UpdateVTKObjects(vtkSMProxy* vtkNotUsed(fromProxy))
{
  return;  // do nothing
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::UpdateViews(vtkSMProxy* caller, bool interactive)
{
  if(this->Internals->Updating)
    {
    return;
    }


  this->Internals->Updating = true;

  const char** props = this->Internals->LinkedPropertyNames;

  for(; *props; props+=2)
    {
    vtkSMProperty* fromProp = caller->GetProperty(props[0]);
    
    int numObjects = this->GetNumberOfLinkedProxies();
    for(int i=0; i<numObjects; i++)
      {
      vtkSMProxy* p = this->GetLinkedProxy(i);
      if(p != caller &&
         this->GetLinkedProxyDirection(i) == vtkSMLink::OUTPUT)
        {
        vtkSMProperty* toProp = p->GetProperty(props[1]);
        toProp->Copy(fromProp);
        p->UpdateProperty(props[1]);
        }
      }
    }

  int numObjects = this->GetNumberOfLinkedProxies();
  for(int i=0; i<numObjects; i++)
    {
    vtkSMProxy* p = this->GetLinkedProxy(i);
    if(this->GetLinkedProxyDirection(i) == vtkSMLink::OUTPUT && p != caller)
      {
      vtkSMRenderModuleProxy* rmp;
      rmp = vtkSMRenderModuleProxy::SafeDownCast(p);
      if(rmp)
        {
        if (interactive)
          {
          rmp->InteractiveRender();
          }
        else
          {
          rmp->StillRender();
          }
        }
      }
    }
  this->Internals->Updating = false;
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::StartInteraction(vtkObject* caller)
{
  if (this->Internals->Updating)
    {
    return;
    }

  this->Internals->Updating = true;
  int numObjects = this->GetNumberOfLinkedProxies();
  for(int i=0; i<numObjects; i++)
    {
    vtkSMRenderModuleProxy* rmp = vtkSMRenderModuleProxy::SafeDownCast(
      this->GetLinkedProxy(i));
    if(rmp && this->GetLinkedProxyDirection(i) == vtkSMLink::OUTPUT && 
      rmp->GetInteractor() != caller)
      {
      rmp->GetInteractor()->InvokeEvent(vtkCommand::StartInteractionEvent);
      }
    }
  this->Internals->Updating = false;
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::EndInteraction(vtkObject* caller)
{
  if (this->Internals->Updating)
    {
    return;
    }

  this->Internals->Updating = true;
  int numObjects = this->GetNumberOfLinkedProxies();
  for(int i=0; i<numObjects; i++)
    {
    vtkSMRenderModuleProxy* rmp = vtkSMRenderModuleProxy::SafeDownCast(
      this->GetLinkedProxy(i));
    if(rmp && this->GetLinkedProxyDirection(i) == vtkSMLink::OUTPUT && 
      rmp->GetInteractor() != caller)
      {
      rmp->GetInteractor()->InvokeEvent(vtkCommand::EndInteractionEvent);
      }
    }
  this->Internals->Updating = false;
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::SaveState(const char* linkname, vtkPVXMLElement* parent)
{
  vtkPVXMLElement* root = vtkPVXMLElement::New();
  Superclass::SaveState(linkname, root);
  unsigned int numElems = root->GetNumberOfNestedElements();
  for (unsigned int cc=0; cc < numElems; cc++)
    {
    vtkPVXMLElement* child = root->GetNestedElement(cc);
    child->SetName("CameraLink");
    parent->AddNestedElement(child);
    }
  root->Delete();
}

//---------------------------------------------------------------------------
int vtkSMCameraLink::LoadState(vtkPVXMLElement* linkElement, vtkSMStateLoader* loader)
{
  return this->Superclass::LoadState(linkElement, loader);
}

//---------------------------------------------------------------------------
void vtkSMCameraLink::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


