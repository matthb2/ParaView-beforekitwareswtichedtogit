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

#include "pqColorScaleToolbar.h"

#include "pqBarChartRepresentation.h"
#include "pqColorScaleEditor.h"
#include "pqDataRepresentation.h"
#include "pqDisplayColorWidget.h"
#include "pqPipelineRepresentation.h"
#include "pqSMAdaptor.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QColorDialog>
#include <QList>
#include <QPointer>
#include <QVariant>

#include "vtkSMProxy.h"
#include "vtkSMProperty.h"


class pqColorScaleToolbarInternal
{
public:
  pqColorScaleToolbarInternal();
  ~pqColorScaleToolbarInternal() {}

  QPointer<pqDataRepresentation> Representation;
  QPointer<pqDisplayColorWidget> ColorBy;
  QPointer<pqColorScaleEditor> ColorScaleEditor;
};


//-----------------------------------------------------------------------------
pqColorScaleToolbarInternal::pqColorScaleToolbarInternal()
  : Representation(), ColorBy(), ColorScaleEditor()
{
}


//-----------------------------------------------------------------------------
pqColorScaleToolbar::pqColorScaleToolbar(QObject *parentObject)
  : QObject(parentObject)
{
  this->Internal = new pqColorScaleToolbarInternal();
  this->ColorAction = 0;
  this->RescaleAction = 0;
}

//-----------------------------------------------------------------------------
pqColorScaleToolbar::~pqColorScaleToolbar()
{
  delete this->Internal;
  this->Internal = 0;
}

//-----------------------------------------------------------------------------
void pqColorScaleToolbar::setColorAction(QAction *action)
{
  if(action != this->ColorAction)
    {
    if(this->ColorAction)
      {
      this->disconnect(this->ColorAction, 0, this, 0);
      }

    this->ColorAction = action;
    if(this->ColorAction)
      {
      this->connect(this->ColorAction, SIGNAL(triggered()),
          this, SLOT(changeColor()));
      }
    }
}

void pqColorScaleToolbar::setRescaleAction(QAction *action)
{
  if(action != this->RescaleAction)
    {
    if(this->RescaleAction)
      {
      this->disconnect(this->RescaleAction, 0, this, 0);
      }

    this->RescaleAction = action;
    if(this->RescaleAction)
      {
      this->connect(this->RescaleAction, SIGNAL(triggered()),
          this, SLOT(rescaleRange()));
      }
    }
}

void pqColorScaleToolbar::setColorWidget(pqDisplayColorWidget *widget)
{
  this->Internal->ColorBy = widget;
}

void pqColorScaleToolbar::setActiveRepresentation(
    pqDataRepresentation *display)
{
  this->Internal->Representation = display;
  if(!this->Internal->ColorScaleEditor.isNull())
    {
    this->Internal->ColorScaleEditor->setRepresentation(
        this->Internal->Representation);
    }
}

void pqColorScaleToolbar::editColorMap(pqDataRepresentation *display)
{
  if(display)
    {
    // Create the color map editor if needed.
    if (this->Internal->ColorScaleEditor.isNull())
      {
      QWidget* parentWidget = qobject_cast<QWidget*>(this->parent());
      if (!parentWidget)
        {
        parentWidget = QApplication::activeWindow();
        }

      this->Internal->ColorScaleEditor = new pqColorScaleEditor(parentWidget);
      this->Internal->ColorScaleEditor->setAttribute(Qt::WA_DeleteOnClose);
      }

    // Set the representation and show the dialog.
    this->Internal->ColorScaleEditor->setRepresentation(display);
    this->Internal->ColorScaleEditor->show();
    }
}

void pqColorScaleToolbar::changeColor()
{
  if(!this->Internal->ColorBy.isNull())
    {
    pqBarChartRepresentation *histogram =
        qobject_cast<pqBarChartRepresentation *>(this->Internal->Representation);
    if(histogram)
      {
      this->editColorMap(this->Internal->Representation);
      }
    else if(this->Internal->ColorBy->getCurrentText() == "Solid Color")
      {
      if(!this->Internal->Representation.isNull())
        {
        // Get the color property.
        vtkSMProxy *proxy = this->Internal->Representation->getProxy();
        vtkSMProperty *diffuse = proxy->GetProperty("DiffuseColor");
        if(diffuse)
          {
          // Get the current color from the property.
          QList<QVariant> rgb =
              pqSMAdaptor::getMultipleElementProperty(diffuse);
          QColor color(Qt::white);
          if(rgb.size() >= 3)
            {
            color = QColor::fromRgbF(rgb[0].toDouble(), rgb[1].toDouble(),
                rgb[2].toDouble());
            }

          // Let the user pick a new color.
          color = QColorDialog::getColor(color, QApplication::activeWindow());
          if(color.isValid())
            {
            // Set the properties to the new color.
            rgb.clear();
            rgb.append(color.redF());
            rgb.append(color.greenF());
            rgb.append(color.blueF());
            pqSMAdaptor::setMultipleElementProperty(diffuse, rgb);
            pqSMAdaptor::setMultipleElementProperty(
                proxy->GetProperty("AmbientColor"), rgb);
            proxy->UpdateVTKObjects();
            }
          }
        }
      }
    else
      {
      this->editColorMap(this->Internal->Representation);
      }
    }
}

void pqColorScaleToolbar::rescaleRange()
{
  pqPipelineRepresentation *pipeline =
      qobject_cast<pqPipelineRepresentation *>(this->Internal->Representation);
  pqBarChartRepresentation *histogram =
      qobject_cast<pqBarChartRepresentation *>(this->Internal->Representation);
  if(pipeline)
    {
    pipeline->resetLookupTableScalarRange();
    pipeline->renderViewEventually();
    }
  else if(histogram)
    {
    histogram->resetLookupTableScalarRange();
    histogram->renderViewEventually();
    }
}


