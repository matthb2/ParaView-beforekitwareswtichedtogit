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
#include "pqTestingReaction.h"

#include "pqPVApplicationCore.h"
#include "pqCoreUtilities.h"
#include "pqFileDialog.h"
#include "pqLockViewSizeCustomDialog.h"
#include "pqTestUtility.h"
#include "pqViewManager.h"

//-----------------------------------------------------------------------------
pqTestingReaction::pqTestingReaction(QAction* parentObject, Mode mode)
  : Superclass(parentObject)
{
  this->ReactionMode = mode;
  if (mode == LOCK_VIEW_SIZE)
    {
    parentObject->setCheckable(true);
    pqViewManager* viewManager = qobject_cast<pqViewManager*>(
                   pqApplicationCore::instance()->manager("MULTIVIEW_MANAGER"));
    QObject::connect(viewManager, SIGNAL(maxViewWindowSizeSet(bool)),
                     parentObject, SLOT(setChecked(bool)));
    }
}

//-----------------------------------------------------------------------------
void pqTestingReaction::recordTest()
{
  QString filters;
  filters += "XML Files (*.xml);;";
#ifdef QT_TESTING_WITH_PYTHON
  filters += "Python Files (*.py);;";
#endif
  filters += "All Files (*)";
  pqFileDialog fileDialog (NULL,
      pqCoreUtilities::mainWidget(),
      tr("Record Test"), QString(), filters);
  fileDialog.setObjectName("ToolsRecordTestDialog");
  fileDialog.setFileMode(pqFileDialog::AnyFile);
  if (fileDialog.exec() == QDialog::Accepted)
    {
    pqTestingReaction::recordTest(fileDialog.getSelectedFiles()[0]);
    }
}

//-----------------------------------------------------------------------------
void pqTestingReaction::recordTest(const QString& filename)
{
  if (!filename.isEmpty())
    {
    pqApplicationCore::instance()->testUtility()->recordTests(filename);
    }
}

//-----------------------------------------------------------------------------
void pqTestingReaction::playTest()
{
  QString filters;
  filters += "XML Files (*.xml);;";
#ifdef QT_TESTING_WITH_PYTHON
  filters += "Python Files (*.py);;";
#endif
  filters += "All Files (*)";
  pqFileDialog fileDialog (NULL,
      pqCoreUtilities::mainWidget(),
      tr("Play Test"), QString(), filters);
  fileDialog.setObjectName("ToolsPlayTestDialog");
  fileDialog.setFileMode(pqFileDialog::ExistingFile);
  if (fileDialog.exec() == QDialog::Accepted)
    {
    pqTestingReaction::playTest(fileDialog.getSelectedFiles()[0]);
    }
}

//-----------------------------------------------------------------------------
void pqTestingReaction::playTest(const QString& filename)
{
  if (!filename.isEmpty())
    {
    pqApplicationCore::instance()->testUtility()->playTests(filename);
    }
}

//-----------------------------------------------------------------------------
void pqTestingReaction::lockViewSize(bool lock)
{
  pqViewManager* viewManager = qobject_cast<pqViewManager*>(
    pqApplicationCore::instance()->manager("MULTIVIEW_MANAGER"));
  if (viewManager)
    {
    viewManager->setMaxViewWindowSize(lock? QSize(300, 300) : QSize(-1, -1));
    }
  else
    {
    qCritical("pqTestingReaction requires pqViewManager.");
    }
}
 
//-----------------------------------------------------------------------------
void pqTestingReaction::lockViewSizeCustom()
{
  // Launch the dialog box.  The box will take care of everything else.
  pqLockViewSizeCustomDialog *sizeDialog
    = new pqLockViewSizeCustomDialog(pqCoreUtilities::mainWidget());
  sizeDialog->setAttribute(Qt::WA_DeleteOnClose, true);
  sizeDialog->show();
}
