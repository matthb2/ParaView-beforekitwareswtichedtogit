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
// .NAME 
// .SECTION Description
// 
//
// .SECTION See Also

#ifndef __vtkBezierContourLineInterpolator_h
#define __vtkBezierContourLineInterpolator_h

#include "vtkContourLineInterpolator.h"

class VTK_WIDGETS_EXPORT vtkBezierContourLineInterpolator 
                          : public vtkContourLineInterpolator
{
public:
  
  // Description:
  // Instantiate this class.
  static vtkBezierContourLineInterpolator *New();
  
  // Description:
  // Standard methods for instances of this class.
  vtkTypeRevisionMacro(vtkBezierContourLineInterpolator,vtkContourLineInterpolator);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual int InterpolateLine( vtkRenderer *ren, 
                               vtkContourRepresentation *rep,
                               int idx1, int idx2 );
  
  vtkSetClampMacro(MaximumCurveError, double, 0.0, VTK_DOUBLE_MAX);
  vtkGetMacro(MaximumCurveError, double);

  vtkSetClampMacro(MaximumCurveLineSegments, int, 1, 1000);
  vtkGetMacro(MaximumCurveLineSegments, int);
  
  // Description:
  // Span of the interpolator. ie. the number of control points its supposed
  // to interpolate given a node. 
  //
  // The first argument is the current nodeIndex.
  // ie, you'd be trying to interpolate between nodes "nodeIndex" and 
  // "nodeIndex-1", unless you're closing the contour in which case, you're
  // trying to interpolate "nodeIndex" and "Node=0". The node span is 
  // returned in a vtkIntArray. 
  //
  // The node span is returned in a vtkIntArray. The node span returned by 
  // this interpolator will be a 2-tuple with a span of 4.
  virtual void GetSpan( int nodeIndex, vtkIntArray *nodeIndices, 
                        vtkContourRepresentation *rep );
    
protected:
  vtkBezierContourLineInterpolator();
  ~vtkBezierContourLineInterpolator();
 
  void ComputeMidpoint( double p1[3], double p2[3], double mid[3] )
    {
      mid[0] = (p1[0] + p2[0])/2;
      mid[1] = (p1[1] + p2[1])/2;
      mid[2] = (p1[2] + p2[2])/2;
    }
  
  double MaximumCurveError;
  int    MaximumCurveLineSegments;

private:
  vtkBezierContourLineInterpolator(const vtkBezierContourLineInterpolator&);  //Not implemented
  void operator=(const vtkBezierContourLineInterpolator&);  //Not implemented
};

#endif
