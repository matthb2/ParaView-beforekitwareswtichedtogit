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
// .NAME vtkTableExtentTranslator - Extent translation through lookup table.
// .SECTION Description
// vtkTableExtentTranslator provides a vtkExtentTranslator that is
// programmed with a specific extent corresponding to each piece
// number.  Readers can provide this to an application to allow the
// pipeline to execute using the same piece breakdown that is provided
// in the input file.

#ifndef __vtkTableExtentTranslator_h
#define __vtkTableExtentTranslator_h

#include "vtkExtentTranslator.h"

class VTK_COMMON_EXPORT vtkTableExtentTranslator : public vtkExtentTranslator
{
public:
  vtkTypeRevisionMacro(vtkTableExtentTranslator,vtkExtentTranslator);
  void PrintSelf(ostream& os, vtkIndent indent);
  
  static vtkTableExtentTranslator* New();
  
  // Description:
  // Set the number of pieces in the extent table.  Once set to a
  // non-zero value, this can only be called to change back to zero,
  // or with the same number of pieces.  This prevents a pipeline from
  // using this extent translator to get any pieces that don't match
  // the table.
  void SetNumberOfPieces(int pieces);
  
  // Description:
  // Called to translate the current piece into an extent.  This is
  // not thread safe.
  int PieceToExtent();  
  
  // Description:
  // Not supported by this subclass of vtkExtentTranslator.
  int PieceToExtentByPoints();
  
  // Description:
  // Not supported by this subclass of vtkExtentTranslator.
  int PieceToExtentThreadSafe(int piece, int numPieces, 
                              int ghostLevel, int *wholeExtent, 
                              int *resultExtent, int splitMode, 
                              int byPoints);
  
  // Description:
  // Set the extent to be used for a piece.  This sets the extent table
  // entry for the piece.
  virtual void SetExtentForPiece(int piece, int* extent);
  
  // Description:  
  // Get the extent table entry for the given piece.  This is only for
  // code that is setting up the table.  Extent translation should
  // always be done through the PieceToExtent method.
  virtual void GetExtentForPiece(int piece, int* extent);
  virtual int* GetExtentForPiece(int piece);
  
  // Description:
  // Set the maximum ghost level that can be requested.  This can be
  // used by a reader to make sure an extent request does not go
  // outside the boundaries of the piece's file.
  vtkSetMacro(MaximumGhostLevel, int);
  vtkGetMacro(MaximumGhostLevel, int);
  
  // Description:
  // Get/Set whether the given piece is available.  Requesting a piece
  // that is not available will produce errors in the pipeline.
  virtual void SetPieceAvailable(int piece, int available);
  virtual int GetPieceAvailable(int piece);
  
protected:
  vtkTableExtentTranslator();
  ~vtkTableExtentTranslator();
  
  // Store the extent table in a single array.  Every 6 values form an extent.
  int* ExtentTable;
  int MaximumGhostLevel;
  
  // Store a flag for the availability of each piece.
  int* PieceAvailable;
  
private:
  vtkTableExtentTranslator(const vtkTableExtentTranslator&);  // Not implemented.
  void operator=(const vtkTableExtentTranslator&);  // Not implemented.
};

#endif
