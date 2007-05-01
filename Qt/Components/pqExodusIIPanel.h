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
cxxPROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#ifndef _pqExodusIIPanel_h
#define _pqExodusIIPanel_h

#include "pqNamedObjectPanel.h"
#include "pqComponentsExport.h"

class pqTreeWidgetItemObject;
class vtkPVArrayInformation;
class vtkSMProperty;
class QPixmap;
class QTreeWidget;

class PQCOMPONENTS_EXPORT pqExodusIIPanel :
  public pqNamedObjectPanel
{
  Q_OBJECT
public:
  /// constructor
  pqExodusIIPanel(pqProxy* proxy, QWidget* p = NULL);
  /// destructor
  ~pqExodusIIPanel();

protected slots:
  void applyDisplacements(int);
  void displChanged(bool);

  void updateDataRanges();
  void propertyChanged();

protected:
  
  /// populate widgets with properties from the server manager
  virtual void linkServerManagerProperties();

  static QString formatDataFor(vtkPVArrayInformation* ai);

  enum PixmapType 
    {
    PM_NONE = -1, PM_NODE, PM_ELEM, PM_SIDESET, PM_NODESET
    };

  void addSelectionsToTreeWidget(const QString& property, 
                                 QTreeWidget* tree,
                                 PixmapType pix);

  void addSelectionToTreeWidget(const QString& name,
                                const QString& realName,
                                QTreeWidget* tree,
                                PixmapType pix,
                                const QString& prop,
                                int propIdx = -1);

  pqTreeWidgetItemObject* DisplItem;

  bool DataUpdateInProgress;

  class pqUI;
  pqUI* UI;

};

#endif

