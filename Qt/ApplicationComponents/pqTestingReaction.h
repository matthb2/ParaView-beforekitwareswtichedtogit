/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 
   
   See License_v1.2.txt for the full ParaView license.
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
#ifndef __pqTestingReaction_h 
#define __pqTestingReaction_h

#include "pqReaction.h"

/// @ingroup Reactions
/// pqTestingReaction can be used to recording or playing back tests.
class PQAPPLICATIONCOMPONENTS_EXPORT pqTestingReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;
public:
  enum Mode
    {
    RECORD,
    PLAYBACK,
    LOCK_VIEW_SIZE
    };

  pqTestingReaction(QAction* parentObject, Mode mode): Superclass(parentObject) 
  {
  this->ReactionMode = mode;
  if (mode == LOCK_VIEW_SIZE)
    {
    parentObject->setCheckable(true);
    }
  }

  /// Records test.
  static void recordTest(const QString& filename);
  static void recordTest();

  /// Plays test.
  static void playTest(const QString& filename);
  static void playTest();

  /// Locks the view size for testing.
  static void lockViewSize(bool);

protected:
  virtual void onTriggered()
    {
    switch (this->ReactionMode)
      {
    case RECORD:
      pqTestingReaction::recordTest();
      break;
    case PLAYBACK:
      pqTestingReaction::playTest();
      break;
    case LOCK_VIEW_SIZE:
      pqTestingReaction::lockViewSize(this->parentAction()->isChecked());
      break;
      }
    }
private:
  Q_DISABLE_COPY(pqTestingReaction)
  Mode ReactionMode;
};

#endif


