/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
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

=========================================================================*/

#ifndef _MainWindow_h
#define _MainWindow_h

#include "OverViewCoreExport.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QVariant>
#include <vtkIOStream.h>

class pqGenericViewModule;
class pqPipelineSource;
class pqRepresentation;
class pqServer;
class pqView;

/// Provides the main window for the ParaView application
class OVERVIEW_CORE_EXPORT MainWindow :
  public QMainWindow
{
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

  bool compareView(const QString& ReferenceImage, double Threshold, ostream& Output, const QString& TempDirectory);

  void setupAnnotationManager(QDockWidget* parent);

public slots:
  QVariant findToolBarActionsNotInMenus();
  
private slots:
  void onUndoLabel(const QString&);
  void onRedoLabel(const QString&);

  void onCameraUndoLabel(const QString&);
  void onCameraRedoLabel(const QString&);

  void onPreAccept();
  void onPostAccept();
  void endWaitCursor();

  void onHelpAbout();
  void onHelpHelp();

  void onSelectionShortcut();
  void onQuickLaunchShortcut();

  void assistantError(const QString& err);

  void onShowCenterAxisChanged(bool);

  void setTimeRanges(double, double);

  void onPlaying(bool);

  void onAddCameraLink();
  
  void onDeleteAll();

  void onSelectionModeChanged(int mode);

  void onEditSettings();

  void onSourceAdded(pqPipelineSource*);
  void onSourceRemoved(pqPipelineSource*);

  void onActiveViewChanged(pqView*);
  void onRepresentationVisibilityChanged(pqRepresentation*,bool);

  void onServerAdded(pqServer*);

private:
  class pqImplementation;
  pqImplementation* const Implementation;
};

#endif // !_MainWindow_h

