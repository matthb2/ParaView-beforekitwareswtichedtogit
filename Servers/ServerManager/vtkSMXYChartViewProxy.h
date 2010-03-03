/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMXYChartViewProxy - view proxy for vtkQtBarChartView
// .SECTION Description
// vtkSMXYChartViewProxy is a concrete subclass of vtkSMChartViewProxy that
// creates a vtkQtBarChartView as the chart view.

#ifndef __vtkSMXYChartViewProxy_h
#define __vtkSMXYChartViewProxy_h

#include "vtkSMContextViewProxy.h"

class vtkChartView;
class vtkChartXY;

class VTK_EXPORT vtkSMXYChartViewProxy : public vtkSMContextViewProxy
{
public:
  static vtkSMXYChartViewProxy* New();
  vtkTypeRevisionMacro(vtkSMXYChartViewProxy, vtkSMContextViewProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set the chart type, defaults to line chart
  void SetChartType(const char *type);

  // Description:
  // Set the title of the chart.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetTitle(const char* title);

  // Description:
  // Set the chart title's font.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetTitleFont(const char* family, int pointSize, bool bold, bool italic);

  // Description:
  // Set the chart title's color.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetTitleColor(double red, double green, double blue);

  // Description:
  // Set the chart title's alignment.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetTitleAlignment(int alignment);

  // Description:
  // Set the legend visibility.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetLegendVisibility(int visible);

  // Description:
  // Sets whether or not the given axis is visible.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisVisibility(int index, bool visible);

  // Description:
  // Sets whether or not the grid for the given axis is visible.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetGridVisibility(int index, bool visible);

  // Description:
  // Sets the color for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisColor(int index, double red, double green, double blue);

  // Description:
  // Sets the color for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetGridColor(int index, double red, double green, double blue);

  // Description:
  // Sets whether or not the labels for the given axis are visible.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLabelVisibility(int index, bool visible);

  // Description:
  // Set the axis label font for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLabelFont(int index, const char* family, int pointSize, bool bold,
                        bool italic);

  // Description:
  // Sets the axis label color for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLabelColor(int index, double red, double green, double blue);

  // Description:
  // Sets the axis label notation for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLabelNotation(int index, int notation);

  // Description:
  // Sets the axis label precision for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLabelPrecision(int index, int precision);

  // Description:
  // Sets the behavior for the given axis, 0 - best fit, 1 - fixed, 2 - custom.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisBehavior(int index, int behavior);

  // Description:
  // Sets the range for the given axis.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisRange(int index, double minimum, double maximum);

  // Description:
  // Sets whether or not the given axis uses a log10 scale.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisLogScale(int index, bool logScale);

  // Description:
  // Set the chart axis title for the given index.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisTitle(int index, const char* title);

  // Description:
  // Set the chart axis title's font for the given index.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisTitleFont(int index, const char* family, int pointSize,
                        bool bold, bool italic);

  // Description:
  // Set the chart axis title's color for the given index.
  // These methods should not be called directly. They are made public only so
  // that the client-server-stream-interpreter can invoke them. Use the
  // corresponding properties to change these values.
  void SetAxisTitleColor(int index, double red, double green, double blue);

  // Description:
  // Provides access to the bar chart view.
//BTX
  vtkChartXY* GetChartXY();
//ETX

//BTX
protected:
  vtkSMXYChartViewProxy();
  ~vtkSMXYChartViewProxy();

  // Description:
  // Called once in CreateVTKObjects() to create a new chart view.
  virtual vtkContextView* NewChartView();

  // Description:
  // Performs the actual rendering. This method is called by
  // both InteractiveRender() and StillRender().
  // Default implementation is empty.
  virtual void PerformRender();

  // Description:
  // Set the internal title, for managing time replacement in the chart title.
  vtkSetStringMacro(InternalTitle);

  // Description:
  // Store the unreplaced chart title in the case where ${TIME} is used...
  char* InternalTitle;

  // Description:
  // Pointer to the proxy's chart instance.
  vtkChartXY* Chart;

private:
  vtkSMXYChartViewProxy(const vtkSMXYChartViewProxy&); // Not implemented
  void operator=(const vtkSMXYChartViewProxy&); // Not implemented
//ETX
};

#endif
