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
// .NAME vtkSMDisplayWindowProxy - composite proxy for renderer, render window...
// .SECTION Description
// vtkSMDisplayWindowProxy is a composite proxy that manages objects related
// to rendering including renderer, render window, composite manager,
// camera, image writer (for screenshots)
// .SECTION See Also
// vtkSMDisplayerProxy vtkSMProxy

#ifndef __vtkSMDisplayWindowProxy_h
#define __vtkSMDisplayWindowProxy_h

#include "vtkSMProxy.h"
#include "vtkClientServerID.h" // Needed for ClientServerID

class vtkSMDisplayerProxy;

class VTK_EXPORT vtkSMDisplayWindowProxy : public vtkSMProxy
{
public:
  static vtkSMDisplayWindowProxy* New();
  vtkTypeRevisionMacro(vtkSMDisplayWindowProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  virtual void CreateVTKObjects(int numObjects);

  // Description:
  void StillRender();

  // Description:
  // Render with LOD. Not used yet.
  void InteractiveRender();

  // Description:
  // Adds a display to the list of managed displays. This adds
  // the actor(s) to the renderer.
  void AddDisplay(vtkSMDisplayerProxy* display);

  // Description:
  // Update all VTK objects. Including the ones managed by the
  // sub-proxies.
  virtual void UpdateVTKObjects();

  // Description:
  // Generate a screenshot from the render window.
  void WriteImage(const char* filename, const char* writerName);

protected:
  vtkSMDisplayWindowProxy();
  ~vtkSMDisplayWindowProxy();

  vtkSMProxy* RendererProxy;
  vtkSMProxy* CameraProxy;
  vtkSMProxy* CompositeProxy;
  vtkSMProxy* WindowToImage;

private:
  vtkSMDisplayWindowProxy(const vtkSMDisplayWindowProxy&); // Not implemented
  void operator=(const vtkSMDisplayWindowProxy&); // Not implemented
};

#endif
