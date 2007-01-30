/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#ifndef __pqAnimationCue_h
#define __pqAnimationCue_h

#include "pqProxy.h"

class vtkSMProxy;
class vtkSMProperty;

class PQCORE_EXPORT pqAnimationCue : public pqProxy
{
  Q_OBJECT;
public:
  pqAnimationCue(const QString& group, const QString& name,
    vtkSMProxy* proxy, pqServer* server, QObject* parent=NULL);
  virtual ~pqAnimationCue();

  // Returns the number of keyframes in this cue.
  int getNumberOfKeyFrames() const;

  // Returns a list of the keyframes.
  QList<vtkSMProxy*> getKeyFrames() const;

  // Insert a new keyframe at the given index.
  // The time for the key frame is computed using the times
  // for the neighbouring keyframes if any.
  // Returns the newly created keyframe proxy on success,
  // NULL otherwise.
  vtkSMProxy* insertKeyFrame(int index);

  // Deletes the keyframe at the given index.
  // Consequently, the keyframesModified() signal will get fired.
  void deleteKeyFrame(int index);

  // Returns keyframe at a given index, if one exists,
  // NULL otherwise.
  vtkSMProxy* getKeyFrame(int index) const;

  // Returns the animated proxy, if any.
  vtkSMProxy* getAnimatedProxy() const;

  // Returns the property that is animated by this cue, if any.
  vtkSMProperty* getAnimatedProperty() const;

  // Returns the index of the property being animated.
  int getAnimatedPropertyIndex() const;

  // Set the type of manipulator to create by default.
  void setManipulatorType(const QString& type)
    { this->ManipulatorType = type; }

  // returns the manipulator proxy.
  vtkSMProxy* getManipulatorProxy() const;

public slots:
  void setDefaults();
    
signals:
  // emitted when something about the keyframes changes.
  void keyframesModified();

  // Fired when the animated proxy/property/index 
  // changes.
  void modified();

private slots:
  // Called when the "Manipulator" property is changed.
  void onManipulatorModified();

private:
  pqAnimationCue(const pqAnimationCue&); // Not implemented.
  void operator=(const pqAnimationCue&); // Not implemented.

  QString ManipulatorType;
  class pqInternals;
  pqInternals* Internal;
};


#endif

