/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkXMLPVAnimationWriter - Data writer for ParaView
// .SECTION Description
// vtkXMLPVAnimationWriter is used to save all parts of a current
// source to a file with pieces spread across ther server processes.

#ifndef __vtkXMLPVAnimationWriter_h
#define __vtkXMLPVAnimationWriter_h

#include "vtkXMLPVDWriter.h"

class vtkXMLPVAnimationWriterInternals;

class VTK_EXPORT vtkXMLPVAnimationWriter: public vtkXMLPVDWriter
{
public:
  static vtkXMLPVAnimationWriter* New();
  vtkTypeRevisionMacro(vtkXMLPVAnimationWriter,vtkXMLPVDWriter);
  void PrintSelf(ostream& os, vtkIndent indent);  
  
  // Description:
  // Add an input corresponding to a named group.  Multiple inputs
  // with the same group name will be considered multiple parts of the
  // same group.  If no name is given, the empty string is assumed.
  void AddInput(vtkDataSet*, const char* group);
  void AddInput(vtkDataSet*);
  
  // Description:
  // Start a new animation with the current set of inputs.
  void Start();
  
  // Description:
  // Write the current time step.
  void WriteTime(double time);
  
  // Description:
  // Finish an animation by writing the collection file.
  void Finish();
  
protected:
  vtkXMLPVAnimationWriter();
  ~vtkXMLPVAnimationWriter();  
  
  // Override vtkProcessObject's AddInput method to prevent compiler
  // warnings.
  virtual void AddInput(vtkDataObject*);
  
  // Replace vtkXMLWriter's writing driver method.
  virtual int WriteInternal();
  
  // Status safety check for method call ordering.
  int StartCalled;
  int FinishCalled;
  
  // Internal implementation details.
  vtkXMLPVAnimationWriterInternals* Internal;
  
private:
  vtkXMLPVAnimationWriter(const vtkXMLPVAnimationWriter&);  // Not implemented.
  void operator=(const vtkXMLPVAnimationWriter&);  // Not implemented.
};

#endif
