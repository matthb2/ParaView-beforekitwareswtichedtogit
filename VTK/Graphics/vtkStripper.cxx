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
#include "vtkStripper.h"

#include "vtkObjectFactory.h"
#include "vtkPolyData.h"

vtkCxxRevisionMacro(vtkStripper, "$Revision$");
vtkStandardNewMacro(vtkStripper);

// Construct object with MaximumLength set to 1000.
vtkStripper::vtkStripper()
{
  this->MaximumLength = 1000;
}

void vtkStripper::Execute()
{
  vtkIdType cellId, numCells, i;
  int longestStrip, longestLine, j, numPts;
  vtkIdType numLines, numStrips, nei;
  vtkCellArray *newStrips=NULL, *inStrips, *newLines=NULL, *inLines, *inPolys;
  vtkCellArray *newPolys=0;
  vtkIdType numLinePts = 0;
  vtkIdList *cellIds;
  int foundOne;
  vtkIdType *pts, neighbor=0;
  vtkPolyData *Mesh;
  char *visited;
  vtkIdType numStripPts = 0;
  vtkIdType *stripPts = 0;
  vtkIdType *linePts = 0;
  vtkIdType *triPts;
  vtkIdType numTriPts;
  vtkPolyData *input= this->GetInput();
  vtkPolyData *output= this->GetOutput();
  vtkPointData *pd=input->GetPointData();

  vtkDebugMacro(<<"Executing triangle strip / poly-line filter");

  // build cell structure
  inStrips = input->GetStrips();
  inLines = input->GetLines();
  inPolys = input->GetPolys();

  Mesh = vtkPolyData::New();
  Mesh->SetPoints(input->GetPoints());
  Mesh->SetLines(inLines);
  Mesh->SetPolys(inPolys);
  Mesh->BuildLinks();

  // check input
  if ( (numCells=Mesh->GetNumberOfCells()) < 1  && inStrips->GetNumberOfCells() < 1)
    {
    Mesh->Delete();
    vtkDebugMacro(<<"No data to strip!");
    return;
    }

  pts = new vtkIdType[this->MaximumLength + 2]; //working array
  cellIds = vtkIdList::New();
  cellIds->Allocate(this->MaximumLength + 2);

  // pre-load existing strips
  if ( inStrips->GetNumberOfCells() > 0 || inPolys->GetNumberOfCells() > 0 )
    {
    newStrips = vtkCellArray::New();
    newStrips->Allocate(newStrips->EstimateSize(numCells,6));
    for(inStrips->InitTraversal();
        inStrips->GetNextCell(numStripPts,stripPts); )
      {
      newStrips->InsertNextCell(numStripPts,stripPts);
      }
    // These are for passing through non-triangle polygons
    newPolys = vtkCellArray::New();
    newPolys->Allocate(newStrips->EstimateSize(numCells/2,4));
    }

  // pre-load existing poly-lines
  if ( inLines->GetNumberOfCells() > 0 )
    {
    newLines = vtkCellArray::New();
    newLines->Allocate(newLines->EstimateSize(numCells,6));
    for (inLines->InitTraversal(); inLines->GetNextCell(numLinePts,linePts); )
      {
      if ( numLinePts > 2 )
        {
        newLines->InsertNextCell(numLinePts,linePts);
        }
      }
    }

  // array keeps track of data that's been visited
  visited = new char[numCells];
  for (i=0; i < numCells; i++)
    {
    visited[i] = 0;
    }

  // Loop over all cells and find one that hasn't been visited.
  // Start a triangle strip (or poly-line) and mark as visited, and 
  // then find a neighbor that isn't visited.  Add this to the strip 
  // (or poly-line) and mark as visited (and so on).
  //
  longestStrip = 0; numStrips = 0;
  longestLine = 0; numLines = 0;

  int cellType;
  int abort=0;
  vtkIdType progressInterval=numCells/20 + 1;
  for ( cellId=0; cellId < numCells && !abort; cellId++)
    {
    if ( !(cellId % progressInterval) ) 
      {
      this->UpdateProgress ((float) cellId/numCells);
      abort = this->GetAbortExecute();
      }
    if ( ! visited[cellId] )
      {
      visited[cellId] = 1;
      if ( (cellType=Mesh->GetCellType(cellId)) == VTK_TRIANGLE )
        {
        //  Got a starting point for the strip.  Initialize.  Find a neighbor
        //  to extend strip.
        //
        numStrips++;
        numPts = 3;

        Mesh->GetCellPoints(cellId,numTriPts,triPts);

        for (i=0; i<3; i++) 
          {
          pts[1] = triPts[i];
          pts[2] = triPts[(i+1)%3];

          Mesh->GetCellEdgeNeighbors(cellId, pts[1], pts[2], cellIds);
          if ( cellIds->GetNumberOfIds() > 0 && 
          !visited[neighbor=cellIds->GetId(0)] &&
          Mesh->GetCellType(neighbor) == VTK_TRIANGLE )
            {
            pts[0] = triPts[(i+2)%3];
            break;
            }
          }
        //  If no unvisited neighbor, just create the strip of one triangle.
        //
        if ( i >= 3 ) 
          {
          pts[0] = triPts[0];;
          pts[1] = triPts[1];
          pts[2] = triPts[2];
          newStrips->InsertNextCell(3,pts);
          } 
        else // continue strip 
          { 
          //  Have a neighbor.  March along grabbing new points
          //
          while ( neighbor >= 0 )
            {
            visited[neighbor] = 1;
            Mesh->GetCellPoints(neighbor,numTriPts, triPts);

            for (i=0; i<3; i++)
              {
              if ( triPts[i] != pts[numPts-2] && 
              triPts[i] != pts[numPts-1] )
                {
                break;
                }
              }

            // only add the triangle to the strip if it isn't degenerate.
            if (i < 3)
              {
              pts[numPts] = triPts[i];
              Mesh->GetCellEdgeNeighbors(neighbor, pts[numPts], 
                                         pts[numPts-1], cellIds);
              numPts++;
              }
            
            if ( numPts > longestStrip )
              {
              longestStrip = numPts;
              }
            
            // note: if updates value of neighbor
            // Note2: for a degenerate triangle this test will
            // correctly fail because the visited[neighbor] will
            // now be visited
            if ( cellIds->GetNumberOfIds() <= 0 || 
                 visited[neighbor=cellIds->GetId(0)] ||
                 Mesh->GetCellType(neighbor) != VTK_TRIANGLE ||
                 numPts >= (this->MaximumLength+2) )
              {
              newStrips->InsertNextCell(numPts,pts);
              neighbor = (-1);
              }
            } // while
          } // else continue strip
        } // if triangle
      
      else if ( cellType == VTK_LINE )
        {
        //
        //  Got a starting point for the line.  Initialize.  Find a neighbor
        //  to extend poly-line.
        //
        numLines++;
        numPts = 2;

        Mesh->GetCellPoints(cellId,numLinePts,linePts);

        for ( foundOne=i=0; !foundOne && i<2; i++) 
          {
          pts[0] = linePts[i];
          pts[1] = linePts[(i+1)%2];
          Mesh->GetPointCells(pts[1], cellIds);
          for (j=0; j < cellIds->GetNumberOfIds(); j++ )
            {
            neighbor = cellIds->GetId(j);
            if ( neighbor != cellId && !visited[neighbor] &&
            Mesh->GetCellType(neighbor) == VTK_LINE )
              {
              foundOne = 1;
              break;
              }
            }
          }
        //  If no unvisited neighbor, just create the poly-line from one line.
        //
        if ( !foundOne ) 
          {
          newLines->InsertNextCell(2,linePts);
          } 
        else // continue poly-line
          { 
          //  Have a neighbor.  March along grabbing new points
          //
          while ( neighbor >= 0 )
            {
            visited[neighbor] = 1;
            Mesh->GetCellPoints(neighbor, numLinePts, linePts);

            for (i=0; i<2; i++)
              {
              if ( linePts[i] != pts[numPts-1] )
                {
                break;
                }
              }
            pts[numPts] = linePts[i];
            Mesh->GetPointCells(pts[numPts], cellIds);
            if ( ++numPts > longestLine )
              {
              longestLine = numPts;
              }

            // get new neighbor
            for ( j=0; j < cellIds->GetNumberOfIds(); j++ )
              {
              nei = cellIds->GetId(j);
              if ( nei != neighbor && !visited[nei] &&
              Mesh->GetCellType(nei) == VTK_LINE )
                {
                neighbor = nei;
                break;
                }
              }

            if ( j >= cellIds->GetNumberOfIds() ||
            numPts >= (this->MaximumLength+1) )
              {
              newLines->InsertNextCell(numPts,pts);
              neighbor = (-1);
              }
            } // while
          } // else continue line
        } // if line

      //not line, triangle, or strip must be quad or tpolygon which we pass through
      else if ( cellType == VTK_POLYGON || cellType == VTK_QUAD )
        {
        Mesh->GetCellPoints(cellId,numTriPts,triPts);
        newPolys->InsertNextCell(numTriPts,triPts);
        }

      } // if not visited
    } // for all elements

  // Update output and release memory
  //
  delete [] pts;
  delete [] visited;
  Mesh->Delete();
  Mesh = NULL;

  output->SetPoints(input->GetPoints());
  output->GetPointData()->PassData(pd);

  // output strips
  if ( newStrips )
    {
    newStrips->Squeeze();
    output->SetStrips(newStrips);
    newStrips->Delete();
    vtkDebugMacro (<<"Reduced " << numCells << " cells to " << numStrips 
                  << " triangle strips \n\t(Average " 
                  << (float)numCells/numStrips 
                  << " triangles per strip, longest strip = "
                  << ((longestStrip-2)>0?(longestStrip-2):0) << " triangles)");

    if ( newPolys->GetNumberOfCells() > 0 )
      {
      vtkDebugMacro(<<"Passed " << newPolys->GetNumberOfCells() 
                    << " polygons");
      newPolys->Squeeze();
      output->SetPolys(newPolys);
      }
    newPolys->Delete();
    }

  // output poly-lines
  if ( newLines )
    {
    newLines->Squeeze();
    output->SetLines(newLines);
    newLines->Delete();
    vtkDebugMacro (<<"Reduced " << numCells << " cells to " << numLines 
                   << " poly-lines \n\t(Average " << (float)numCells/numLines 
                   << " lines per poly-line, longest poly-line = "
                   << ((longestLine-1)>0?(longestLine-1):0) << " lines)");

    }

  // pass through verts
  output->SetVerts(input->GetVerts());
  cellIds->Delete();
}

void vtkStripper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Maximum Length: " << this->MaximumLength << "\n";
}
