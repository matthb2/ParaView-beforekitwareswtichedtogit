// -*- c++ -*-

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

=========================================================================*/
// .NAME vtkDesktopDeliveryServer - An object for remote rendering.
//
// .SECTION Description
// The two vtkDesktopDelivery objects (vtkDesktopDeliveryClient and
// vtkDesktopDeliveryServer) work together to enable interactive viewing of
// remotely rendered data.  The server attaches itself to a vtkRenderWindow
// and, optionally, another vtkParallelRenderManager.  Whenever a new
// rendering is requested, the client alerts the server, the server renders
// a new frame, and ships the image back to the client, which will display
// the image in the vtkRenderWindow.
//
// .SECTION note
//
// .SECTION see also
// vtkDesktopDeliveryClient vtkMultiProcessController vtkRenderWindow
// vtkParallelRenderManager

#ifndef __vtkDesktopDeliveryServer_h
#define __vtkDesktopDeliveryServer_h

#include "vtkParallelRenderManager.h"

class VTK_EXPORT vtkDesktopDeliveryServer : public vtkParallelRenderManager
{
public:
  vtkTypeRevisionMacro(vtkDesktopDeliveryServer, vtkParallelRenderManager);
  virtual void PrintSelf(ostream &os, vtkIndent indent);

  static vtkDesktopDeliveryServer *New();

  // Description:
  // Set/Get the controller that is attached to a vtkDesktopDeliveryClient.
  // This object will assume that the controller has two processors, and
  // that the controller on the opposite side of the controller has been
  // given to the server object.
  virtual void SetController(vtkMultiProcessController *controller);

  // Description:
  // Set/Get a parallel render manager that is used for parallel rendering.
  // If not set or set to NULL, rendering is done on the render window
  // directly in single process mode.  It will be assumed that the
  // ParallelRenderManager will have the final image stored at the local
  // processes.
  virtual void SetParallelRenderManager(vtkParallelRenderManager *prm);
  vtkGetObjectMacro(ParallelRenderManager, vtkParallelRenderManager);

  virtual void SetRenderWindow(vtkRenderWindow *renWin);

  // Description:
  // If on (the default) locally rendered images will be shipped back to
  // the client.  To facilitate this, the local rendering windows will be
  // resized based on the remote window settings.  If off, the images are
  // assumed to be displayed locally.  The render window maintains its
  // current size.
  vtkSetMacro(RemoteDisplay, int);
  vtkGetMacro(RemoteDisplay, int);
  vtkBooleanMacro(RemoteDisplay, int);

//BTX

  enum Tags {
    IMAGE_TAG=12433,
    IMAGE_SIZE_TAG=12434,
    REMOTE_DISPLAY_TAG=834340,
    TIMING_METRICS_TAG=834341
  };

  struct TimingMetrics {
    double ImageProcessingTime;
  };

  enum StructSizes {
    TIMING_METRICS_SIZE=sizeof(struct TimingMetrics)/sizeof(double)
  };

//ETX
  
protected:
  vtkDesktopDeliveryServer();
  virtual ~vtkDesktopDeliveryServer();

  virtual void PreRenderProcessing();
  virtual void PostRenderProcessing();
  virtual void LocalComputeVisiblePropBounds(vtkRenderer *ren, float bounds[6]);

  vtkParallelRenderManager *ParallelRenderManager;

  unsigned long StartParallelRenderTag;
  unsigned long EndParallelRenderTag;

  virtual void SetRenderWindowSize();

  virtual void ReadReducedImage();

  int RemoteDisplay;

private:
  vtkDesktopDeliveryServer(const vtkDesktopDeliveryServer &); //Not implemented
  void operator=(const vtkDesktopDeliveryServer &);  //Not implemented
};


#endif //__vtkDesktopDeliveryServer_h

