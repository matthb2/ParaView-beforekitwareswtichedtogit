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
// .NAME vtkXMLStructuredDataWriter - Superclass for VTK XML structured data writers.
// .SECTION Description
// vtkXMLStructuredDataWriter provides VTK XML writing functionality that
// is common among all the structured data formats.

#ifndef __vtkXMLStructuredDataWriter_h
#define __vtkXMLStructuredDataWriter_h

#include "vtkXMLWriter.h"

class vtkDataSet;
class vtkPointData;
class vtkExtentTranslator;
class vtkDataArray;
class vtkDataSetAttributes;

class VTK_IO_EXPORT vtkXMLStructuredDataWriter : public vtkXMLWriter
{
public:
  vtkTypeRevisionMacro(vtkXMLStructuredDataWriter,vtkXMLWriter);
  void PrintSelf(ostream& os, vtkIndent indent);
  
  // Description:
  // Get/Set the number of pieces used to stream the image through the
  // pipeline while writing to the file.
  vtkSetMacro(NumberOfPieces, int);
  vtkGetMacro(NumberOfPieces, int);
  
  // Description:
  // Get/Set the extent of the input that should be treated as the
  // WholeExtent in the output file.  The default is the WholeExtent
  // of the input.
  vtkSetVector6Macro(WriteExtent, int);
  vtkGetVector6Macro(WriteExtent, int);
  
  // Description:
  // Get/Set the extent translator used for streaming.
  virtual void SetExtentTranslator(vtkExtentTranslator*);
  vtkGetObjectMacro(ExtentTranslator, vtkExtentTranslator);
  
protected:
  vtkXMLStructuredDataWriter();
  ~vtkXMLStructuredDataWriter();  
  
  // Writing drivers defined by subclasses.
  virtual void WritePrimaryElementAttributes();
  virtual void WriteAppendedPiece(int index, vtkIndent indent);
  virtual void WriteAppendedPieceData(int index);
  virtual void WriteInlinePiece(int index, vtkIndent indent);
  virtual void GetInputExtent(int* extent)=0;
  
  // The actual writing driver required by vtkXMLWriter.
  int WriteData();
  void SetupExtentTranslator();
  virtual int WriteAppendedMode(vtkIndent indent);
  vtkDataArray* CreateExactExtent(vtkDataArray* array, int* inExtent,
                                  int* outExtent, int isPoint);
  virtual int WriteInlineMode(vtkIndent indent);
  unsigned int GetStartTuple(int* extent, int* increments,
                             int i, int j, int k);
  void CalculatePieceFractions(float* fractions);
  
  // Define utility methods required by vtkXMLWriter.
  vtkDataArray* CreateArrayForPoints(vtkDataArray* inArray);
  vtkDataArray* CreateArrayForCells(vtkDataArray* inArray);
  
  // The extent of the input to write.
  int WriteExtent[6];
  
  // Number of pieces used for streaming.
  int NumberOfPieces;
  
  // Translate piece number to extent.
  vtkExtentTranslator* ExtentTranslator;
  
  // Appended data offsets of point and cell data arrays.
  unsigned long** PointDataOffsets;
  unsigned long** CellDataOffsets;
  
private:
  vtkXMLStructuredDataWriter(const vtkXMLStructuredDataWriter&);  // Not implemented.
  void operator=(const vtkXMLStructuredDataWriter&);  // Not implemented.
};

#endif
