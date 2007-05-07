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

=========================================================================*/

/// \file pqPipelineDisplay.h
/// \date 4/24/2006

#ifndef _pqPipelineDisplay_h
#define _pqPipelineDisplay_h


#include "pqConsumerDisplay.h"
#include <QPair>

class pqPipelineDisplayInternal;
class pqPipelineSource;
class pqRenderViewModule;
class pqServer;
class vtkPVArrayInformation;
class vtkPVDataSetAttributesInformation;
class vtkPVDataSetAttributesInformation;
class vtkSMDataObjectDisplayProxy;

/// This is PQ representation for a single display. A pqDisplay represents
/// a single vtkSMDataObjectDisplayProxy. The display can be added to
/// only one render module or more (ofcouse on the same server, this class
/// doesn't worry about that.
class PQCORE_EXPORT pqPipelineDisplay : public pqConsumerDisplay
{
  Q_OBJECT
  typedef pqConsumerDisplay Superclass;
public:
  pqPipelineDisplay(const QString& name,
    vtkSMDataObjectDisplayProxy* display, pqServer* server,
    QObject* parent=NULL);
  virtual ~pqPipelineDisplay();

  // Get the internal display proxy.
  vtkSMDataObjectDisplayProxy* getDisplayProxy() const;

  // Sets the default color mapping for the display.
  // The rules are:
  // If the source created a NEW point scalar array, use it.
  // Else if the source created a NEW cell scalar array, use it.
  // Else if the input color by array exists in this source, use it.
  // Else color by property.
  virtual void setDefaultPropertyValues();

  // Call to select the coloring array. 
  void colorByArray(const char* arrayname, int fieldtype);

  /// Get the names of the arrays that a part may be colored by.
  /// This information may change based on the current reprentation
  /// of the display, eg. when showing Outline, only "Solid Color" is
  /// available; when rendering as Volume, can be colored only by 
  /// point data.
  QList<QString> getColorFields();

  /// get the data range for a particular component. if component == -1,
  /// range for the vector magnitude is returned.
  QPair<double, double> getColorFieldRange(const QString& array, int component);

  /// Returns the range for the currently selected color field i.e.
  /// the range for the array component (or magnitude) of the array by which
  /// this display is being colored, if at all.
  QPair<double, double> getColorFieldRange();

  /// set the array to color the part by
  void setColorField(const QString& field);

  /// get the array the part is colored by
  /// if raw is true, it will not add (point) or (cell) but simply
  /// return the array name
  QString getColorField(bool raw=false);

  /// Returns the number of components for the given field.
  /// field is a string of format "<arrayname> (cell|point)".
  int getColorFieldNumberOfComponents(const QString& field);

  /// Get the data bounds for the input of this display.
  /// Returns if the operation was successful.
  bool getDataBounds(double bounds[6]);

  /// Returns the proxy for the piecewise function used to
  /// map scalars to opacity.
  vtkSMProxy* getScalarOpacityFunctionProxy() const;

  /// Set the scalar range for the opacity function. This
  /// will rescale all the control points to fit the 
  /// scalar range.
  void setScalarOpacityRange(double min, double max);

  /// Set representation on the proxy.
  /// If representation is changed to volume, this method ensures that the
  /// scalar array is initialized.
  void setRepresentation(int type);
signals:
  /// This is fire when any property that affects the color
  /// mode for the display changes.
  void colorChanged();

public slots:
  // If lookuptable is set up and is used for coloring,
  // then calling this method resets the table ranges to match the current 
  // range of the selected array.
  void resetLookupTableScalarRange();

  /// If color lookuptable is set up and coloring is enabled, the this
  /// ensure that the lookuptable scalar range is greater than than the
  /// color array's scalar range. It also updates the scalar range on
  /// the scalar-opacity function, if any. Both the ColorLUT and the 
  /// ScalarOpacityFunction may choose to ignore the set scalar range
  /// based on value ScalePointsWithRange.
  void updateLookupTableScalarRange();

protected slots:
  /// Called when the "Representation" property changes. If
  /// representation changes to "Volume", we have to ensure that
  /// the color field is initialized to something other than
  /// "Solid Color" otherwise the volume mapper segfaults.
  void onRepresentationChanged();

protected:
  /// Creates helper proxies such as as the proxy
  /// for volume opacity function.
  void createHelperProxies();

private:
  pqPipelineDisplayInternal *Internal; 
  static void getColorArray(
    vtkPVDataSetAttributesInformation* attrInfo,
    vtkPVDataSetAttributesInformation* inAttrInfo,
    vtkPVArrayInformation*& arrayInfo);

  int getNumberOfComponents(const char* arrayname, int fieldType);
};

#endif
