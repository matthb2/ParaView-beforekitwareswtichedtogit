/*=========================================================================

  Program:   Visualization Library
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

This file is part of the Visualization Library. No part of this file
or its contents may be copied, reproduced or altered in any way
without the express written consent of the authors.

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 1993, 1994 

=========================================================================*/
#include "MergePts.hh"

// Description:
// Merge points together if they are exactly coincident. Return a list 
// that maps unmerged point ids into new point ids. User is responsible 
// for freeing list (use delete []).
int *vlMergePoints::MergePoints()
{
  float *bounds;
  int ptId, i, j;
  int numPts;
  int *index;
  int newPtId;
  float *pt, *p;
  int ijk[3];
  int cno;
  vlIdList *ptIds;

  vlDebugMacro(<<"Merging points");

  if ( this->Points == NULL || 
  (numPts=this->Points->GetNumberOfPoints()) < 1 ) return NULL;

  this->SubDivide(); // subdivides if necessary

  bounds = this->Points->GetBounds();

  index = new int[numPts];
  for (i=0; i < numPts; i++) index[i] = -1;

  newPtId = 0; // renumbering points
//
//  Traverse each point, find cell that point is in, check the list of
//  points in that cell for merging.  Also need to search all
//  neighboring cells within the tolerance.  The number and level of
//  neighbors to search depends upon the tolerance and the cell width.
//
  for ( i=0; i < numPts; i++ ) //loop over all points
    {
    // Only try to merge the point if it hasn't yet been merged.
    if ( index[i] == -1 ) 
      {
      p = this->Points->GetPoint(i);
      index[i] = newPtId;

      for (j=0; j<3; j++) 
        ijk[j] = (int) ((float)((p[j] - bounds[2*j])*0.999 / 
                      (bounds[2*j+1] - bounds[2*j])) * this->Divisions[j]);

      cno = ijk[0] + ijk[1]*this->Divisions[0] + 
            ijk[2]*this->Divisions[0]*this->Divisions[1];

      if ( (ptIds = this->HashTable[cno]) != NULL )
        {
        for (j=0; j < ptIds->GetNumberOfIds(); j++) 
          {
          ptId = ptIds->GetId(j);
          pt = this->Points->GetPoint(ptId);

          if ( index[ptId] == -1 && pt[0] == p[0] && pt[1] == p[1]
          && pt[2] == p[2] )
            {
            index[ptId] = newPtId;
            }
          }
        }
      newPtId++;
      } // if point hasn't been merged
    } // for all points

  return index;
}

// Description:
// Incrementally insert a point into search structure, merging the point
// with pre-inserted point (if precisely coincident). If point is merged 
// with pre-inserted point, pre-inserted point id is returned. Otherwise, 
// new point id is returned. Before using this method you must make sure 
// that newPts have been supplied, the bounds has been set properly, and
// that divs are properly set. (See InitPointInsertion()).
int vlMergePoints::InsertPoint(float x[3])
{
  int i, ijk[3];
  int idx;
  vlIdList *cell;
//
//  Locate cell that point is in.
//
  for (i=0; i<3; i++) 
    {
    ijk[i] = (int) ((float) ((x[i] - this->Bounds[2*i])*0.999 / 
             (this->Bounds[2*i+1] - this->Bounds[2*i])) * this->Divisions[i]);
    }
  idx = ijk[0] + ijk[1]*this->Divisions[0] + 
        ijk[2]*this->Divisions[0]*this->Divisions[1];

  cell = this->HashTable[idx];

  if ( ! cell )
    {
    cell = new vlIdList(this->NumberOfPointsInCell/2);
    this->HashTable[idx] = cell;
    }
  else // see whether we've got duplicate point
    {
//
// Check the list of points in that cell.
//
    int ptId;
    float *pt;

    for (i=0; i < cell->GetNumberOfIds(); i++) 
      {
      ptId = cell->GetId(i);
      pt = this->Points->GetPoint(ptId);
      if ( x[0] == pt[0] && x[1] == pt[1] && x[2] == pt[2] ) return ptId;
      }
    }

  cell->InsertNextId(this->InsertionPointId);
  this->Points->InsertPoint(this->InsertionPointId,x);

  return this->InsertionPointId++;
}

