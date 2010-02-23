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
#include "pqDataQueryReaction.h"

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqFiltersMenuReaction.h"
#include "pqPVApplicationCore.h"
#include "pqQueryDialog.h"
#include "pqSelectionManager.h"

//-----------------------------------------------------------------------------
pqDataQueryReaction::pqDataQueryReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  QObject::connect(activeObjects, SIGNAL(portChanged(pqOutputPort*)),
    this, SLOT(updateEnableState()));
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
pqDataQueryReaction::~pqDataQueryReaction()
{
}

//-----------------------------------------------------------------------------
void pqDataQueryReaction::updateEnableState()
{
  pqActiveObjects& activeObjects = pqActiveObjects::instance();
  pqOutputPort* port = activeObjects.activePort();
  bool enable_state = (port != NULL);
  this->parentAction()->setEnabled(enable_state);
}

//-----------------------------------------------------------------------------
void pqDataQueryReaction::showQueryDialog()
{
  pqQueryDialog dialog(
    pqActiveObjects::instance().activePort(),
    pqCoreUtilities::mainWidget());

  // We want to make the query the active application wide selection, so we
  // hookup the query action to selection manager so that the application
  // realizes a new selection has been made.
  pqSelectionManager* selManager =
    pqPVApplicationCore::instance()->selectionManager();
  if (selManager)
    {
    QObject::connect(&dialog, SIGNAL(selected(pqOutputPort*)),
      selManager, SLOT(select(pqOutputPort*)));
    }
  dialog.exec();

  if (dialog.extractSelection())
    {
    pqFiltersMenuReaction::createFilter("filters", "ExtractSelection");
    }
  else if (dialog.extractSelectionOverTime())
    {
    pqFiltersMenuReaction::createFilter("filters", "ExtractSelectionOverTime");
    }
}

