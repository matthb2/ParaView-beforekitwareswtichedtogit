/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

// .NAME vtkAxis - takes care of drawing 2D axes
//
// .SECTION Description
// The vtkAxis is drawn in screen coordinates. It is usually one of the last
// elements of a chart to be drawn. It renders the axis label, tick marks and
// tick labels.

#ifndef __vtkAxis_h
#define __vtkAxis_h

#include "vtkContextItem.h"

class vtkContext2D;
class vtkPen;
class vtkFloatArray;
class vtkStringArray;
class vtkTextProperty;

class VTK_CHARTS_EXPORT vtkAxis : public vtkContextItem
{
public:
  vtkTypeRevisionMacro(vtkAxis, vtkContextItem);
  virtual void PrintSelf(ostream &os, vtkIndent indent);

//BTX
  // Description:
  // Enumeration of the axis locations in a conventional XY chart. Other
  // layouts are possible.
  enum {
    LEFT = 0,
    BOTTOM,
    RIGHT,
    TOP
  };
//ETX

  // Description:
  // Creates a 2D Chart object.
  static vtkAxis *New();

  // Description:
  // Get/set the position of the axis (LEFT, BOTTOM, RIGHT, TOP).
  vtkSetMacro(Position, int);
  vtkGetMacro(Position, int);

  // Description:
  // Set point 1 of the axis (in pixels), this is usually the origin.
  vtkSetVector2Macro(Point1, float);

  // Description:
  // Get point 1 of the axis (in pixels), this is usually the origin.
  vtkGetVector2Macro(Point1, float);

  // Description:
  // Set point 2 of the axis (in pixels), this is usually the terminus.
  vtkSetVector2Macro(Point2, float);

  // Description:
  // Get point 2 of the axis (in pixels), this is usually the terminus.
  vtkGetVector2Macro(Point2, float);

  // Description:
  // Set the number of tick marks for this axis.
  vtkSetMacro(NumberOfTicks, int);

  // Description:
  // Get the number of tick marks for this axis.
  vtkGetMacro(NumberOfTicks, int);

  // Description:
  // Get the vtkTextProperty that governs how the axis lables are displayed.
  // Note that the alignment properties are not used.
  vtkGetObjectMacro(LabelProperties, vtkTextProperty);

  // Description:
  // Set the logical minimum value of the axis, in plot coordinates.
  vtkSetMacro(Minimum, float);

  // Description:
  // Get the logical minimum value of the axis, in plot coordinates.
  vtkGetMacro(Minimum, float);

  // Description:
  // Set the logical maximum value of the axis, in plot coordinates.
  vtkSetMacro(Maximum, float);

  // Description:
  // Get the logical maximum value of the axis, in plot coordinates.
  vtkGetMacro(Maximum, float);

  // Description:
  // Get/set the title text of the axis.
  vtkSetStringMacro(Title);
  vtkGetStringMacro(Title);

  // Description:
  // Get the vtkTextProperty that governs how the axis title is displayed.
  vtkGetObjectMacro(TitleProperties, vtkTextProperty);

  // Description:
  // Get/set whether the axis should use a log scale, default is false.
  vtkSetMacro(LogScale, bool);
  vtkGetMacro(LogScale, bool);

  // Description:
  // Get/set whether the axis grid lines should be drawn, default is true.
  vtkSetMacro(GridVisible, bool);
  vtkGetMacro(GridVisible, bool);

  // Description:
  // Get/set whether the axis labels should be visible.
  vtkSetMacro(LabelsVisible, bool);
  vtkGetMacro(LabelsVisible, bool);

  // Description:
  // Get/set the numerical precision to use, default is 2.
  vtkSetMacro(Precision, int);
  vtkGetMacro(Precision, int);

  // Description:
  // Get/set the numerical notation, standard, scientific or mixed (0, 1, 2).
  vtkSetMacro(Notation, int);
  vtkGetMacro(Notation, int);

  // Description:
  // Get/set the behavior of the axis (auto, fixed, custom). Default is 0 (auto).
  vtkSetMacro(Behavior, int);
  vtkGetMacro(Behavior, int);

  // Description:
  // Get a pointer to the vtkPen object that controls the way this axis is drawn.
  vtkGetObjectMacro(Pen, vtkPen);

  // Description:
  // Get a pointer to the vtkPen object that controls the way this axis is drawn.
  vtkGetObjectMacro(GridPen, vtkPen);

  // Description:
  // Update the geometry of the axis. Takes care of setting up the tick mark
  // locations etc. Should be called by the scene before rendering.
  virtual void Update();

  // Description:
  // Paint event for the axis, called whenever the axis needs to be drawn.
  virtual bool Paint(vtkContext2D *painter);

  // Description:
  // Use this function to autoscale the axes after setting the minimum and
  // maximum values. This will cause the axes to select the nicest numbers
  // that enclose the minimum and maximum values, and to select an appropriate
  // number of tick marks.
  virtual void AutoScale();

  // Description:
  // Recalculate the spacing of the tick marks - typically useful to do after
  // scaling the axis.
  virtual void RecalculateTickSpacing();

  // Description:
  // A float array with the positions of the tick marks along the axis line.
  // The positions are specified in the coordinate system the axis is drawn in
  // (normally screen coordinates).
  vtkFloatArray* GetTickPositions() { return this->TickPositions; }

//BTX
protected:
  vtkAxis();
  ~vtkAxis();

  // Description:
  // Calculate the next "nicest" numbers above and below the current minimum.
  // \return the "nice" spacing of the numbers.
  float CalculateNiceMinMax(float &min, float &max);

  // Description:
  // Return a "nice number", often defined as 1, 2 or 5. If roundUp is true then
  // the nice number will be rounded up, false it is rounded down. The supplied
  // number should be between 0.0 and 9.9.
  float NiceNumber(float number, bool roundUp);

  int Position;        // The position of the axis (LEFT, BOTTOM, RIGHT, TOP)
  float Point1[2];     // The position of point 1 (usually the origin)
  float Point2[2];     // The position of point 2 (usually the terminus)
  float TickInterval;  // Interval between tick marks in plot space
  int NumberOfTicks;   // The number of tick marks to draw
  vtkTextProperty* LabelProperties; // Text properties for the labels.
  int TickLabelSize;   // The point size of the tick labels
  float Minimum;       // Minimum value of the axis
  float Maximum;       // Maximum values of the axis
  char* Title;         // The text label drawn on the axis
  vtkTextProperty* TitleProperties; // Text properties for the axis title
  bool LogScale;       // Should the axis use a log scale
  bool GridVisible;    // Whether the grid for the axis should be drawn
  bool LabelsVisible;  // Should the axis labels be visible
  int Precision;       // Numerical precision to use, defaults to 2.
  int Notation;        // The notation to use (standard, scientific, mixed)
  int Behavior;       // The behaviour of the axis (auto, fixed, custom).

  // Description:
  // This object stores the vtkPen that controls how the axis is drawn.
  vtkPen* Pen;

  // Description:
  // This object stores the vtkPen that controls how the grid lines are drawn.
  vtkPen* GridPen;

  vtkFloatArray* TickPositions; // Position of tick marks in screen coordinates
  vtkStringArray* TickLabels; // The labels for the tick marks

private:
  vtkAxis(const vtkAxis &); // Not implemented.
  void operator=(const vtkAxis &);   // Not implemented.
//ETX
};

#endif //__vtkAxis_h
