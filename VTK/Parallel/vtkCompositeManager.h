/*=========================================================================
  
  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$
  
Copyright (c) 1993-2001 Ken Martin, Will Schroeder, Bill Lorensen 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor the names
   of any contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
// .NAME vtkCompositeManager - An object to composite multiple render windows.
// .SECTION Description
// vtkCompositeManager operates in multiple processes.  Each compositer has 
// a render window.  They use vtkMultiProcessControllers to comunicate 
// the color and depth buffer to process 0's render window.
// Subclass implementation may not handle transparency well.
// .SECTION note
// You should set up the renders and render window interactor before setting
// the compositers render window.  We set up observers on the renderer,
// An have no easy way of knowing when the renderers change.  We could 
// create AddRenderer and RemoveRenderer events ...
// .SECTION see also
// vtkMultiProcessController vtkRenderWindow vtkTreeComposite.

#ifndef __vtkCompositeManager_h
#define __vtkCompositeManager_h

#include "vtkObject.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkMultiProcessController.h"

class vtkTimerLog;


class VTK_EXPORT vtkCompositeManager : public vtkObject
{
public:
  vtkTypeMacro(vtkCompositeManager,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set/Get the RenderWindow to use for compositing.
  // We add a start and end observer to the window.
  vtkGetObjectMacro(RenderWindow, vtkRenderWindow);
  void SetRenderWindow(vtkRenderWindow *renWin);

  // Description:
  // This method sets the piece and number of pieces for each
  // actor with a polydata mapper. My other option is to 
  // do it every render, but that would force a partioning scheme.
  void InitializePieces();
  
  // Description:
  // turn off screen rendering on for the
  void InitializeOffScreen();
  
  // Description:
  // Callbacks that initialize and finish the compositing.
  void StartInteractor();
  void ExitInteractor();
  void StartRender();
  void EndRender();
  void RenderRMI();
  void ResetCamera(vtkRenderer *ren);
  void ResetCameraClippingRange(vtkRenderer *ren);
  void ComputeVisiblePropBoundsRMI();
  
  // Description:
  // If the user wants to handle the event loop, then they must call this
  // method to initialize the RMIs.
  void InitializeRMIs();
  
  // Description:
  // The reduction facor makes the transfered images smaller to decrease 
  // the render time.  The final image is pixel replicated to be the original 
  // size.  This option can be used by an interactor style to help get desired 
  // frame rates.  The factor only needs to be set on process 0.
  // Call SetRenderWindow before calling SetReductionFactor.
  void SetReductionFactor(int factor);
  vtkGetMacro(ReductionFactor, int);

  // Description:
  // This flag tells the compositer to use char values for pixel data rather than float.
  // Default is float.  I have seen some artifacts on some systems with char.
  vtkSetMacro(UseChar, int);
  vtkGetMacro(UseChar, int);
  vtkBooleanMacro(UseChar, int);

  // Description:
  // This flag turns the compositer on or off.
  vtkSetMacro(UseCompositing, int);
  vtkGetMacro(UseCompositing, int);
  vtkBooleanMacro(UseCompositing, int);
  
  // Description:
  // Get the times to set/get the buffers, Composite,
  // and the time it takes all processes to finish the rendering step.
  vtkGetMacro(GetBuffersTime, double);
  vtkGetMacro(SetBuffersTime, double);
  vtkGetMacro(CompositeTime, double);
  vtkGetMacro(MaxRenderTime, double);

  // Description:
  // Get the value of the z buffer at a position. 
  float GetZ(int x, int y);

  // Description:
  // Set/Get the controller use in compositing (set to
  // the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  vtkSetObjectMacro(Controller, vtkMultiProcessController);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

//BTX

  enum Tags {
    RENDER_RMI_TAG=12721,
    COMPUTE_VISIBLE_PROP_BOUNDS_RMI_TAG=56563,
    WIN_INFO_TAG=22134,
    REN_INFO_TAG=22135,
    BOUNDS_TAG=94135
  };

  // Description:
  // Used by call backs.  Not intended to be called by the user.
  // Empty methods that can be used by the subclass to interupt a parallel render.
  virtual void CheckForAbortRender() {}
  virtual int CheckForAbortComposite() {return 0;}  
//ETX

protected:
  vtkCompositeManager();
  ~vtkCompositeManager();
  vtkCompositeManager(const vtkCompositeManager&);
  void operator=(const vtkCompositeManager&);
  
  // A compositing algorithm to be implemented by the subclass.
  virtual void CompositeBuffer(int width, int height, int useCharFlag,
                               void *pBuf, float *zBuf,
                               void *pTmp, float *zTmp) = 0;

  void Composite();
  virtual void ComputeVisiblePropBounds(vtkRenderer *ren, float bounds[6]);
  void SetRendererSize(int x, int y);
  float* MagnifyBuffer(float *localPdata, int windowSize[2]);

  vtkRenderWindow* RenderWindow;
  vtkRenderWindowInteractor* RenderWindowInteractor;
  vtkMultiProcessController* Controller;

  unsigned long StartInteractorTag;
  unsigned long EndInteractorTag;
  unsigned long StartTag;
  unsigned long EndTag;
  unsigned long ResetCameraTag;
  unsigned long ResetCameraClippingRangeTag;
  int UseChar;
  int UseCompositing;
  
  // Convenience method used internally. It set up the start observer
  // and allows the render window's interactor to be set before or after
  // the compositer's render window (not exactly true).
  void SetRenderWindowInteractor(vtkRenderWindowInteractor *iren);

  // Arrays for compositing.
  float *PData;
  float *ZData;
  int RendererSize[2];

  // Reduction factor (For fast interactive compositing).
  int ReductionFactor;
  
  // This cause me a head ache while trying to debug a lockup.
  // I am taking it out in retaliation.  I do not think nested
  // RMI's can occur anyway.
  // This flag stops nested RMIs from occuring.  Some rmis send and receive information.
  // Nesting them can lock up the processes.
  int Lock;

  double GetBuffersTime;
  double SetBuffersTime;
  double CompositeTime;
  double MaxRenderTime;

  // Needed to compute the MaxRenderTime.
  vtkTimerLog *Timer;
};

#endif
