/*=========================================================================

  Program:   Visualization Library
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

This file is part of the Visualization Library. No part of this file or its
contents may be copied, reproduced or altered in any way without the express
written consent of the authors.

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 1993, 1994

=========================================================================*/
#include "TextSrc.hh"
#include "FPoints.hh"
#include "Graymap.hh"

#define vlfont_width 9
#define vlfont_row_width 864
#define vlfont_height 15
static char vlfont_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0xe0,0x00,0x10,0x90,0x00,0x00,0x40,0x88,0x03,0x1c,0x10,0x08,0x00,
 0x00,0x00,0x00,0x00,0x00,0x20,0x1c,0x10,0xf8,0xf8,0x03,0xe2,0x0f,0x8f,0x3f,
 0x3e,0x7c,0x00,0x00,0x00,0x02,0x80,0x00,0x1f,0x3e,0x10,0xfc,0xf0,0xf1,0xe3,
 0xcf,0x1f,0x1f,0x41,0x7c,0xe0,0x09,0x12,0x20,0x48,0x10,0x1f,0x3f,0x7c,0xfc,
 0xf0,0xf1,0x27,0x48,0x90,0x20,0x41,0x82,0xfc,0xe1,0x11,0xc0,0x03,0x02,0x00,
 0x0e,0x00,0x04,0x00,0x00,0x04,0x00,0x0e,0x00,0x01,0x00,0x00,0x08,0xc0,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x80,0x83,0xe0,0x80,0x11,0xe0,0x00,0x10,0x90,0x90,0x80,0xa0,0x44,0x04,0x0c,
 0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x22,0x18,0x04,0x01,0x02,0x23,
 0x80,0x00,0x20,0x41,0x82,0x00,0x00,0x00,0x01,0x00,0x81,0x20,0x41,0x28,0x08,
 0x09,0x22,0x44,0x80,0x80,0x20,0x41,0x10,0x80,0x08,0x11,0x20,0x48,0x90,0x20,
 0x41,0x82,0x04,0x09,0x82,0x20,0x48,0x90,0x20,0x41,0x82,0x00,0x21,0x20,0x00,
 0x02,0x05,0x00,0x0c,0x00,0x04,0x00,0x00,0x04,0x00,0x11,0x00,0x01,0x10,0x80,
 0x08,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x40,0x80,0x00,0x41,0x12,0xe0,0x00,0x10,0x90,0x90,0xe0,0xa3,
 0x44,0x04,0x02,0x08,0x10,0x00,0x40,0x00,0x00,0x00,0x00,0x10,0x41,0x14,0x04,
 0x01,0x81,0x22,0x40,0x00,0x20,0x41,0x82,0x00,0x00,0x80,0x00,0x00,0x82,0x20,
 0x41,0x44,0x08,0x09,0x20,0x44,0x80,0x80,0x00,0x41,0x10,0x80,0x88,0x10,0x60,
 0xcc,0x90,0x20,0x41,0x82,0x04,0x09,0x80,0x20,0x48,0x90,0x20,0x22,0x44,0x80,
 0x20,0x20,0x00,0x82,0x08,0x00,0x10,0x00,0x04,0x00,0x00,0x04,0x00,0x11,0x00,
 0x01,0x00,0x00,0x08,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x80,0x00,0x41,0x0c,0xe0,0x00,0x10,0x00,
 0xf8,0x91,0x40,0x42,0x04,0x00,0x04,0x20,0x88,0x40,0x00,0x00,0x00,0x00,0x08,
 0x41,0x10,0x00,0x81,0x40,0xa2,0x47,0x00,0x10,0x41,0x82,0x20,0x40,0x40,0x00,
 0x00,0x04,0x20,0x79,0x82,0x08,0x09,0x20,0x44,0x80,0x80,0x00,0x41,0x10,0x80,
 0x48,0x10,0xa0,0x4a,0x91,0x20,0x41,0x82,0x04,0x09,0x80,0x20,0x88,0x88,0x20,
 0x14,0x28,0x40,0x20,0x40,0x00,0x42,0x10,0x00,0x00,0x7c,0xf4,0xf0,0xe1,0xc5,
 0x07,0x01,0x2f,0x3d,0x18,0xe0,0x08,0x82,0xe0,0x46,0x0f,0x1f,0x3d,0xbc,0xe4,
 0xf0,0xf1,0x23,0x44,0x90,0x20,0x41,0x42,0xfc,0x81,0x80,0x80,0x00,0x00,0xe0,
 0x00,0x10,0x00,0x90,0x90,0x00,0x81,0x03,0x00,0x04,0x20,0x50,0x40,0x00,0x00,
 0x00,0x00,0x04,0x41,0x10,0x80,0xc0,0x21,0x62,0x48,0x0f,0x08,0x3e,0xc2,0x70,
 0xe0,0x20,0xe0,0x0f,0x08,0x10,0x45,0x82,0xf8,0x08,0x20,0xc4,0x83,0x87,0x00,
 0x7f,0x10,0x80,0x38,0x10,0xa0,0x4a,0x92,0x20,0x3f,0x82,0xfc,0xf0,0x81,0x20,
 0x88,0x88,0x24,0x08,0x10,0x20,0x20,0x80,0x00,0x02,0x00,0x00,0x00,0x80,0x0c,
 0x09,0x12,0x26,0x08,0x81,0x10,0x43,0x10,0x80,0x88,0x81,0x20,0xc9,0x90,0x20,
 0x43,0xc2,0x18,0x09,0x42,0x20,0x44,0x90,0x20,0x22,0x42,0x80,0x60,0x80,0x00,
 0x03,0x00,0xe0,0x00,0x10,0x00,0x90,0xe0,0x03,0x41,0x04,0x00,0x04,0x20,0xfc,
 0xf9,0x03,0xe0,0x0f,0x00,0x04,0x41,0x10,0x40,0x00,0x12,0x02,0xc8,0x10,0x04,
 0x41,0xbc,0x20,0x40,0x20,0x00,0x00,0x08,0x08,0x65,0x82,0x08,0x09,0x20,0x44,
 0x80,0x80,0x38,0x41,0x10,0x80,0x28,0x10,0x20,0x49,0x94,0x20,0x01,0x82,0x24,
 0x00,0x82,0x20,0x88,0x88,0x24,0x08,0x10,0x10,0x20,0x80,0x00,0x02,0x00,0x00,
 0x00,0x80,0x04,0x09,0x10,0x24,0xc8,0x87,0x10,0x41,0x10,0x80,0x68,0x80,0x20,
 0x49,0x90,0x20,0x41,0x82,0x08,0x09,0x40,0x20,0x84,0x88,0x24,0x14,0x42,0x40,
 0x60,0x80,0x00,0x03,0x00,0xe0,0x00,0x10,0x00,0xf8,0x81,0x84,0x44,0x14,0x00,
 0x04,0x20,0x50,0x40,0x00,0x00,0x00,0x00,0x02,0x41,0x10,0x30,0x00,0xf2,0x07,
 0x48,0x10,0x02,0x41,0x80,0x00,0x00,0x40,0x00,0x00,0x04,0x04,0x59,0xfe,0x08,
 0x09,0x20,0x44,0x80,0x80,0x20,0x41,0x10,0x80,0x48,0x10,0x20,0x49,0x98,0x20,
 0x01,0x82,0x44,0x00,0x82,0x20,0x08,0x85,0x24,0x14,0x10,0x08,0x20,0x00,0x01,
 0x02,0x00,0x00,0x00,0xfc,0x04,0x09,0x10,0xe4,0x0f,0x81,0x10,0x41,0x10,0x80,
 0x18,0x80,0x20,0x49,0x90,0x20,0x41,0x82,0x08,0xf0,0x41,0x20,0x84,0x88,0x24,
 0x08,0x42,0x20,0x80,0x80,0x80,0x00,0x00,0xe0,0x00,0x10,0x00,0x90,0x80,0x44,
 0x4a,0x08,0x00,0x08,0x10,0x88,0x40,0x00,0x00,0x00,0x00,0x01,0x41,0x10,0x08,
 0x00,0x02,0x02,0x48,0x10,0x02,0x41,0x80,0x00,0x00,0x80,0xe0,0x0f,0x02,0x04,
 0x01,0x82,0x08,0x09,0x20,0x44,0x80,0x80,0x20,0x41,0x10,0x80,0x88,0x10,0x20,
 0x48,0x90,0x20,0x01,0x92,0x84,0x00,0x82,0x20,0x08,0x85,0x24,0x22,0x10,0x04,
 0x20,0x00,0x02,0x02,0x00,0x00,0x00,0x82,0x04,0x09,0x10,0x24,0x00,0x01,0x0f,
 0x41,0x10,0x80,0x68,0x80,0x20,0x49,0x90,0x20,0x41,0x82,0x08,0x00,0x42,0x20,
 0x04,0x85,0x24,0x14,0x42,0x10,0x40,0x80,0x00,0x01,0x00,0xe0,0x00,0x00,0x00,
 0x90,0xe0,0x43,0x4a,0x0c,0x00,0x08,0x10,0x00,0x40,0xc0,0x01,0x00,0x02,0x01,
 0x22,0x10,0x04,0x08,0x02,0x22,0x48,0x10,0x01,0x41,0x40,0x20,0xe0,0x00,0x01,
 0x00,0x01,0x00,0x01,0x82,0x08,0x09,0x22,0x44,0x80,0x80,0x20,0x41,0x10,0x84,
 0x08,0x11,0x20,0x48,0x90,0x20,0x01,0xa2,0x04,0x09,0x82,0x20,0x08,0x85,0x2a,
 0x41,0x10,0x04,0x20,0x00,0x02,0x02,0x00,0x00,0x00,0xc2,0x0c,0x09,0x12,0x26,
 0x00,0x81,0x00,0x41,0x10,0x80,0x88,0x81,0x20,0x49,0x90,0x20,0x43,0xc2,0x08,
 0x08,0x42,0x24,0x04,0x85,0x2a,0x22,0x62,0x08,0x40,0x80,0x00,0x01,0x00,0xe0,
 0x00,0x10,0x00,0x00,0x80,0x20,0x84,0x13,0x00,0x10,0x08,0x00,0x00,0xc0,0x00,
 0x00,0x87,0x00,0x1c,0x7c,0xfc,0xf1,0x01,0xc2,0x87,0x0f,0x01,0x3e,0x3c,0x70,
 0x60,0x00,0x02,0x80,0x00,0x04,0x3e,0x82,0xfc,0xf0,0xf1,0xe3,0x8f,0x00,0x1f,
 0x41,0x7c,0x78,0x08,0xf2,0x27,0x48,0x10,0x1f,0x01,0x7c,0x04,0xf1,0x81,0xc0,
 0x07,0x02,0x11,0x41,0x10,0xfc,0xe1,0x01,0xc4,0x03,0x00,0x00,0x00,0xbc,0xf4,
 0xf0,0xe1,0xc5,0x07,0x01,0x1f,0x41,0x7c,0x84,0x08,0xe2,0x23,0x48,0x10,0x1f,
 0x3d,0xbc,0x08,0xf0,0x81,0xc3,0x0b,0x02,0x11,0x41,0x5c,0xfc,0x81,0x83,0xe0,
 0x00,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x20,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x3f,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x20,0x00,0x00,0x84,0x00,0x00,0x00,
 0x00,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
 0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x20,0x00,0x00,0x84,
 0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,
 0x00,0x00,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0
 };

// Description:
// Construct Text object with no string set and Backing enabled.
vlTextSource::vlTextSource()
{
  this->Text = NULL;
  this->Backing = 1;
}

void vlTextSource::Execute()
{
  int row, col;
  vlFloatPoints *newPoints; 
  vlCellArray *newPolys;
  vlGraymap *newScalars;
  float x[3];
  int pos = 0;
  int pixelPos;
  int pts[5];
  int numPolys = 0;
  int acol;
  int drawingWhite = 0;
  int drawingBlack = 0;
  static unsigned char white[4] = {255, 255, 255, 255};
  static unsigned char black[4] = {0, 0, 0, 255};

  // Set things up; allocate memory
  this->Initialize();
  x[2] = 0;

  newPoints = new vlFloatPoints();
  newPolys = new vlCellArray;
  newScalars = new vlGraymap;

  // Create Text
  while (this->Text[pos])
    {
    if (this->Text[pos] != 32)
      {
      for (col = 0; col < vlfont_width; col++)
	{
	acol = (this->Text[pos] - 32)*vlfont_width + col - 2;
	for (row = 0; row < vlfont_height; row++)
	  {
	  pixelPos = acol + row*vlfont_row_width;
	  if (vlfont_bits[pixelPos/8] & (0x01 << pixelPos%8))
	    {
	    if (drawingBlack)
	      {
	      x[0] = pos*vlfont_width + col + 1; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(black);

	      x[0] = pos*vlfont_width + col; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(black);

	      pts[0] = numPolys*4;
	      pts[1] = numPolys*4 + 1;
	      pts[2] = numPolys*4 + 2;
	      pts[3] = numPolys*4 + 3;
	      newPolys->InsertNextCell(4,pts);
	      numPolys++;
	      drawingBlack = 0;
	      }
	    if (!drawingWhite)
	      {
	      x[0] = pos*vlfont_width + col; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(white);

	      x[0] = pos*vlfont_width + col + 1; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(white);
	      drawingWhite = 1;
	      }
	    }
	  // if the pixel is not set the close up the rectangle
	  else
	    {
	    if (drawingWhite)
	      {
	      x[0] = pos*vlfont_width + col + 1; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(white);

	      x[0] = pos*vlfont_width + col; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(white);

	      pts[0] = numPolys*4;
	      pts[1] = numPolys*4 + 1;
	      pts[2] = numPolys*4 + 2;
	      pts[3] = numPolys*4 + 3;
	      newPolys->InsertNextCell(4,pts);
	      numPolys++;
	      drawingWhite = 0;
	      }
	    if (!drawingBlack && this->Backing)
	      {
	      x[0] = pos*vlfont_width + col; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(black);

	      x[0] = pos*vlfont_width + col + 1; 
	      x[1] = vlfont_height - row;
	      newPoints->InsertNextPoint(x);
	      newScalars->InsertNextColor(black);
	      drawingBlack = 1;
	      }
	    }
	  }
	// if we finished up a row but are still drawing close it up
	if (drawingWhite)
	  {
	  x[0] = pos*vlfont_width + col + 1; 
	  x[1] = 0;
	  newPoints->InsertNextPoint(x);
	  newScalars->InsertNextColor(white);

	  x[0] = pos*vlfont_width + col; 
	  x[1] = 0;
	  newPoints->InsertNextPoint(x);
	  newScalars->InsertNextColor(white);
	  
	  pts[0] = numPolys*4;
	  pts[1] = numPolys*4 + 1;
	  pts[2] = numPolys*4 + 2;
	  pts[3] = numPolys*4 + 3;
	  newPolys->InsertNextCell(4,pts);
	  numPolys++;
	  drawingWhite = 0;
	  }
	if (drawingBlack)
	  {
	  x[0] = pos*vlfont_width + col + 1; 
	  x[1] = 0;
	  newPoints->InsertNextPoint(x);
	  newScalars->InsertNextColor(black);

	  x[0] = pos*vlfont_width + col; 
	  x[1] = 0;
	  newPoints->InsertNextPoint(x);
	  newScalars->InsertNextColor(black);
	  
	  pts[0] = numPolys*4;
	  pts[1] = numPolys*4 + 1;
	  pts[2] = numPolys*4 + 2;
	  pts[3] = numPolys*4 + 3;
	  newPolys->InsertNextCell(4,pts);
	  numPolys++;
	  drawingBlack = 0;
	  }
	}
      }
    else
      {
      // draw a black square for a space
      if (this->Backing)
	{
	x[0] = pos*vlfont_width; 
	x[1] = vlfont_height;
	newPoints->InsertNextPoint(x);
	newScalars->InsertNextColor(black);
      
	x[0] = pos*vlfont_width + vlfont_width; 
	x[1] = vlfont_height;
	newPoints->InsertNextPoint(x);
	newScalars->InsertNextColor(black);

	x[0] = pos*vlfont_width + vlfont_width; 
	x[1] = 0;
	newPoints->InsertNextPoint(x);
	newScalars->InsertNextColor(black);
      
	x[0] = pos*vlfont_width; 
	x[1] = 0;
	newPoints->InsertNextPoint(x);
	newScalars->InsertNextColor(black);
      
	pts[0] = numPolys*4;
	pts[1] = numPolys*4 + 1;
	pts[2] = numPolys*4 + 2;
	pts[3] = numPolys*4 + 3;
	newPolys->InsertNextCell(4,pts);
	numPolys++;
	}
      }
    pos++;
    }

  // Update ourselves
  this->SetPoints(newPoints);
  this->PointData.SetScalars(newScalars);
  this->SetPolys(newPolys);
}

void vlTextSource::PrintSelf(ostream& os, vlIndent indent)
{
  vlPolySource::PrintSelf(os,indent);

  os << indent << "Text: " << (this->Text ? this->Text : "(none)") << "\n";
}
