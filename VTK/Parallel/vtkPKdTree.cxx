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
/*----------------------------------------------------------------------------
 Copyright (c) Sandia Corporation
 See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.
----------------------------------------------------------------------------*/

#include "vtkPKdTree.h"
#include "vtkKdNode.h"
#include "vtkDataSet.h"
#include "vtkCellCenters.h"
#include "vtkPoints.h"
#include "vtkUnstructuredGrid.h"
#include "vtkObjectFactory.h"
#include "vtkMultiProcessController.h"
#include "vtkSocketController.h"
#include "vtkTimerLog.h"
#include "vtkCellData.h"
#include "vtkPointData.h"
#include "vtkIntArray.h"
#include "vtkIdList.h"
#include "vtkSubGroup.h"
#include <vtkstd/queue>
#include <vtkstd/algorithm>

// Timing data ---------------------------------------------

#if 0
#define MSGSIZE 60

static char dots[MSGSIZE] = "...........................................................";
static char msg[MSGSIZE];

static char * makeEntry(const char *s)
{
  memcpy(msg, dots, MSGSIZE);
  int len = strlen(s);
  len = (len >= MSGSIZE) ? MSGSIZE-1 : len;

  memcpy(msg, s, len);

  return msg;
}

#define TIMER(s)                        \
  if (this->GetTiming())                \
    {                                   \
    char *s2 = makeEntry(s);            \
    if (this->TimerLog == NULL)            \
      {                                    \
      this->TimerLog = vtkTimerLog::New(); \
      }                                    \
    this->TimerLog->MarkStartEvent(s2); \
    }

#define TIMERDONE(s) \
  if (this->GetTiming())\
    { char *s2 = makeEntry(s); this->TimerLog->MarkEndEvent(s2); }

#else
#define TIMER(s) 
#define TIMERDONE(s)
#endif

// Timing data ---------------------------------------------

vtkCxxRevisionMacro(vtkPKdTree, "$Revision$");
vtkStandardNewMacro(vtkPKdTree);

const int vtkPKdTree::NoRegionAssignment = 0;   // default
const int vtkPKdTree::ContiguousAssignment = 1; // default if RegionAssignmentOn
const int vtkPKdTree::UserDefinedAssignment = 2;
const int vtkPKdTree::RoundRobinAssignment  = 3;

#define FreeList(list)   if (list) {delete [] list; list = NULL;}
#define FreeItem(item)   if (item) {delete item; item = NULL;}
#define FreeObject(item)   if (item) {item->Delete(); item = NULL;}

static char errstr[256];

#define VTKERROR(s) \
{                   \
  sprintf(errstr,"(process %d) %s",this->MyId,s); \
  vtkErrorMacro(<< errstr);                       \
}
#define VTKWARNING(s) \
{                     \
  sprintf(errstr,"(process %d) %s",this->MyId,s); \
  vtkWarningMacro(<< errstr);                       \
}

vtkPKdTree::vtkPKdTree()
{
  this->RegionAssignment = NoRegionAssignment;

  this->Controller = NULL;
  this->SubGroup   = NULL;

  this->NumProcesses = 1;
  this->MyId         = 0;

  this->InitializeRegionAssignmentLists();
  this->InitializeProcessDataLists();
  this->InitializeFieldArrayMinMax();
  this->InitializeGlobalIndexLists();

  this->TotalNumCells = 0;

  this->PtArray = NULL;
  this->PtArray2 = NULL;
  this->CurrentPtArray = NULL;
  this->NextPtArray = NULL;

  this->SelectBuffer = NULL;
}
vtkPKdTree::~vtkPKdTree()
{
  this->SetController(NULL);
  this->FreeSelectBuffer();
  this->FreeDoubleBuffer();

  this->FreeGlobalIndexLists();
  this->FreeRegionAssignmentLists();
  this->FreeProcessDataLists();
  this->FreeFieldArrayMinMax();
}
void vtkPKdTree::SetController(vtkMultiProcessController *c)
{
  if (this->Controller == c) 
    {
    return;
    }

  if ((c == NULL) || (c->GetNumberOfProcesses() == 0))
    {
    this->NumProcesses = 1;
    this->MyId = 0;
    }

  this->Modified();

  if (this->Controller != NULL)
    {
    this->Controller->UnRegister(this);
    this->Controller = NULL;
    }

  if (c == NULL)
    {
    return;
    }

  vtkSocketController *sc = vtkSocketController::SafeDownCast(c);

  if (sc)
    {
    vtkErrorMacro(<< 
      "vtkPKdTree communication will fail with a socket controller");

    return;
    }

  this->NumProcesses = c->GetNumberOfProcesses();

  this->Controller = c;
  this->MyId = c->GetLocalProcessId();
  c->Register(this);
}
//--------------------------------------------------------------------
// Parallel k-d tree build, Floyd and Rivest (1975) select algorithm
// for median finding.
//--------------------------------------------------------------------

int vtkPKdTree::AllCheckForFailure(int rc, const char *where, const char *how)
{
  int vote;
  char errmsg[256];

  if (this->NumProcesses > 1){
    this->SubGroup->ReduceSum(&rc, &vote, 1, 0);
    this->SubGroup->Broadcast(&vote, 1, 0);
  }
  else{
    vote = rc;
  }

  if (vote)
    {
    if (rc)
      {
      sprintf(errmsg,"%s on my node (%s)",how, where);
      }
    else
      {
      sprintf(errmsg,"%s on a remote node (%s)",how, where);
      }
    VTKWARNING(errmsg);

    return 1;
    }
  return 0;
}

void vtkPKdTree::AllCheckParameters()
{
  int param[10];
  int param0[10];

  // All the parameters that determine how k-d tree is built and
  //  what tables get created afterward - there's no point in
  //  trying to build unless these match on all processes.

  param[0] = this->ValidDirections;
  param[1] = this->GetMinCells();
  param[2] = this->GetNumberOfRegionsOrLess();
  param[3] = this->GetNumberOfRegionsOrMore();
  param[4] = this->RegionAssignment;
  param[5] = 0;
  param[6] = 0;
  param[7] = 0;
  param[8] = 0;
  param[9] = 0;

  if (this->MyId == 0)
    {
    this->SubGroup->Broadcast(param, 10, 0);
    return;
    }

  this->SubGroup->Broadcast(param0, 10, 0);

  int diff = 0;

  for (int i=0; i < 10; i++)
    {
    if (param0[i] != param[i])
    {
      diff = 1;
      break;
      }
    }
  if (diff)
    {
    VTKWARNING("Changing my runtime parameters to match process 0");

    this->ValidDirections        = param0[0];
    this->SetMinCells(param0[1]);
    this->SetNumberOfRegionsOrLess(param0[2]);
    this->SetNumberOfRegionsOrMore(param0[3]);
    this->RegionAssignment       = param0[4];
    }
  return;
}

#define BoundsToMinMax(bounds,min,max) \
{                                      \
  min[0] = bounds[0]; min[1] = bounds[2]; min[2] = bounds[4]; \
  max[0] = bounds[1]; max[1] = bounds[3]; max[2] = bounds[5]; \
}
#define MinMaxToBounds(bounds,min,max) \
{                                      \
  bounds[0] = min[0]; bounds[2] = min[1]; bounds[4] = min[2]; \
  bounds[1] = max[0]; bounds[3] = max[1]; bounds[5] = max[2]; \
}
#define BoundsToMinMaxUpdate(bounds,min,max) \
{                                            \
  min[0] = (bounds[0] < min[0] ? bounds[0] : min[0]); \
  min[1] = (bounds[2] < min[1] ? bounds[2] : min[1]); \
  min[2] = (bounds[4] < min[2] ? bounds[4] : min[2]); \
  max[0] = (bounds[1] > max[0] ? bounds[1] : max[0]); \
  max[1] = (bounds[3] > max[1] ? bounds[3] : max[1]); \
  max[2] = (bounds[5] > max[2] ? bounds[5] : max[2]); \
}

double *vtkPKdTree::VolumeBounds()
{
  int i;

  // Get the spatial bounds of the whole volume
  
  double *volBounds = new double [6];
  double localMin[3], localMax[3], globalMin[3], globalMax[3];

  for (i=0; i<this->GetNumberOfDataSets(); i++)
    {
    this->GetDataSet(i)->GetBounds(volBounds);
  
    if (i==0)
      {
      BoundsToMinMax(volBounds, localMin, localMax);
      }
    else
      {
      BoundsToMinMaxUpdate(volBounds, localMin, localMax);
      }
    }

  this->SubGroup->ReduceMin(localMin, globalMin, 3, 0);
  this->SubGroup->Broadcast(globalMin, 3, 0);

  this->SubGroup->ReduceMax(localMax, globalMax, 3, 0);
  this->SubGroup->Broadcast(globalMax, 3, 0);

  MinMaxToBounds(volBounds, globalMin, globalMax);

  // push out a little if flat

  double diff[3], aLittle = 0.0;

  for (i=0; i<3; i++)
    {
     diff[i] = volBounds[2*i+1] - volBounds[2*i];
     aLittle = (diff[i] > aLittle) ? diff[i] : aLittle;
    }
  if ((aLittle /= 100.0) <= 0.0)
    {
     VTKERROR("VolumeBounds - degenerate volume");
     return NULL;
    }

  this->SetFudgeFactor(aLittle * 10e-4);

  for (i=0; i<3; i++)
    {
    if (diff[i] <= 0)
      {
        volBounds[2*i]   -= aLittle;
        volBounds[2*i+1] += aLittle;
      }
    else // need lower bound to be strictly less than any point in decomposition
      {
      volBounds[2*i] -= this->GetFudgeFactor();
      }
    }
  return volBounds;
}

// BuildLocator must be called by all processes in the parallel application

void vtkPKdTree::BuildLocator()
{
  int fail = 0;
  int rebuildLocator = 0;

  if ((this->Top == NULL) || 
      (this->BuildTime < this->GetMTime()) ||
      this->NewGeometry())
    {
    // We don't have a k-d tree, or parameters that affect the
    // build of the tree have changed, or input geometry has changed.

    rebuildLocator = 1;
    } 

  if (this->NumProcesses == 1){
    if (rebuildLocator)
      {
      this->SingleProcessBuildLocator();
      }
    return;
  }

  TIMER("Determine if we need to rebuild");

  this->SubGroup = vtkSubGroup::New();
  this->SubGroup->Initialize(0, this->NumProcesses-1,
             this->MyId, 0x00001000, this->Controller->GetCommunicator());

  int vote;
  this->SubGroup->ReduceSum(&rebuildLocator, &vote, 1, 0);
  this->SubGroup->Broadcast(&vote, 1, 0);

  rebuildLocator = (vote > 0);

  TIMERDONE("Determine if we need to rebuild");

  if (rebuildLocator)
    {
    TIMER("Build k-d tree");

    this->AllCheckParameters();

    fail = this->MultiProcessBuildLocator();

    if (fail) goto doneError;

    TIMERDONE("Build k-d tree");
    }

  // Even if locator is not rebuilt, we should update
  // region assignments since they may have changed.

  this->UpdateRegionAssignment();

  goto done;

doneError:

  this->FreeRegionAssignmentLists();
  this->FreeSearchStructure();

done:

  FreeObject(this->SubGroup);

  this->UpdateBuildTime();
  
  this->SetCalculator(this->Top);

  return;
}
int vtkPKdTree::MultiProcessBuildLocator()
{
  int retVal = 0;
  double *volBounds = NULL;

  vtkDebugMacro( << "Creating Kdtree in parallel" );

  if (this->GetTiming())
    {
    if (this->TimerLog == NULL) this->TimerLog = vtkTimerLog::New();
    }

  this->FreeSearchStructure();

  this->ReleaseTables();   // they're not valid anymore

  // Locally, create a single list of the coordinates of the centers of the 
  //   cells of my data sets 

  TIMER("Compute cell centers");

  this->PtArray = NULL;

  this->PtArray = this->ComputeCellCenters();
  int totalPts = this->GetNumberOfCells();    // total on local node
  this->CurrentPtArray = this->PtArray;

  int fail = (this->PtArray == NULL);

  if (this->AllCheckForFailure(fail, 
          "MultiProcessBuildLocator", "memory allocation"))
    {
    goto doneError6;
    }

  TIMERDONE("Compute cell centers");

  // Get total number of cells across all processes, assign global indices
  //   for select operation

  TIMER("Build index lists");

  fail = this->BuildGlobalIndexLists(totalPts);

  TIMERDONE("Build index lists");

  if (fail)
    {
    goto doneError6;
    }

  // Get the bounds of the entire volume

  TIMER("Compute volume bounds");

  volBounds = this->VolumeBounds();

  TIMERDONE("Compute volume bounds");

  if (volBounds == NULL)
    {
    goto doneError6;
    }

  // In parallel, build the k-d tree structure, partitioning all
  //   the points into spatial regions.  Sub-groups of processors
  //   will form vtkSubGroups to divide sub-regions of space.

  FreeObject(this->SubGroup);

  TIMER("Compute tree");

  fail = this->BreadthFirstDivide(volBounds);

  TIMERDONE("Compute tree");

  this->SubGroup = vtkSubGroup::New();
  this->SubGroup->Initialize(0, this->NumProcesses-1,
             this->MyId, 0x00002000, this->Controller->GetCommunicator());

  if (this->AllCheckForFailure(fail, "BreadthFirstDivide", "memory allocation"))
    {
    goto doneError6;
    }

  FreeObject(this->SubGroup);

  // I only have a partial tree at this point, the regions in which
  //   I participated.  Now collect the entire tree.

  this->SubGroup = vtkSubGroup::New();
  this->SubGroup->Initialize(0, this->NumProcesses-1,
             this->MyId, 0x00003000, this->Controller->GetCommunicator());

  TIMER("Complete tree");

  fail = this->CompleteTree();

  TIMERDONE("Complete tree");

  if (fail)
    {
    goto doneError6;
    }

  goto done6;

doneError6:

  this->FreeSearchStructure();
  retVal = 1;

done6:
  // no longer valid, we overwrote them during k-d tree parallel build
  delete [] this->PtArray;
  this->CurrentPtArray = this->PtArray = NULL;

  FreeObject(this->SubGroup);

  if (volBounds)
    {
    FreeList(volBounds);
    }
  this->FreeGlobalIndexLists();

  return retVal;
}

void vtkPKdTree::SingleProcessBuildLocator()
{
  vtkKdTree::BuildLocator();

  this->TotalNumCells = this->GetNumberOfCells();

  if (this->RegionAssignment != vtkPKdTree::NoRegionAssignment)
    {
    this->UpdateRegionAssignment();
    }

  return;
}
typedef struct _vtkNodeInfo{
  vtkKdNode *kd;
  int L;
  int level;
  int tag;
} *vtkNodeInfo;

#define ENQUEUE(a, b, c, d)  \
{                            \
  vtkNodeInfo rec = new struct _vtkNodeInfo; \
  rec->kd = a; \
  rec->L = b; \
  rec->level = c; \
  rec->tag = d; \
  Queue.push(rec); \
}

int vtkPKdTree::BreadthFirstDivide(double *volBounds)
{
  int returnVal = 0;

  vtkstd::queue <vtkNodeInfo> Queue;

  if (this->AllocateDoubleBuffer())
    {
    VTKERROR("memory allocation for double buffering");  
    return 1;
    }

  if (this->AllocateSelectBuffer())
    {
    this->FreeDoubleBuffer();

    VTKERROR("memory allocation for select buffers");
    return 1;
    }

  vtkKdNode *kd = this->Top = vtkKdNode::New();

  kd->SetBounds(volBounds[0], volBounds[1],
                volBounds[2], volBounds[3],
                volBounds[4], volBounds[5]);

  kd->SetNumberOfPoints(this->TotalNumCells);

  kd->SetDataBounds(volBounds[0], volBounds[1],
                volBounds[2], volBounds[3],
                volBounds[4], volBounds[5]);

  int midpt = this->DivideRegion(kd, 0, 0, 0x00000001);

  if (midpt > 0)
    {
    ENQUEUE(kd->GetLeft(), 0, 1, 0x00000002);
    ENQUEUE(kd->GetRight(), midpt, 1, 0x00000003);
    }
  else if (midpt < 0)
    {
    this->FreeSelectBuffer();
    this->FreeDoubleBuffer();

    return 1;
    }

  while (!Queue.empty())
    {
    vtkNodeInfo info = Queue.front();
    Queue.pop();

    kd = info->kd;
    int L = info->L;
    int level = info->level;
    int tag   = info->tag;

    midpt = this->DivideRegion(kd, L, level, tag);

    if (midpt > 0)
      {
      ENQUEUE(kd->GetLeft(), L, level+1, tag << 1);

      ENQUEUE(kd->GetRight(), midpt, level+1, (tag << 1) | 1);
      }
    else if (midpt < 0)
      {
      returnVal = 1;  // have to keep going, or remote ops may hang
      }
    delete info;
    }

  this->FreeSelectBuffer();

  if (this->CurrentPtArray == this->PtArray2)
    {
    memcpy(this->PtArray, this->PtArray2, this->PtArraySize * sizeof(float));
    }

  this->FreeDoubleBuffer();

  return returnVal;
}
int vtkPKdTree::DivideRegion(vtkKdNode *kd, int L, int level, int tag)
{
  if (!this->DivideTest(kd->GetNumberOfPoints(), level)) return 0;

  int R = L + kd->GetNumberOfPoints() - 1;

  int p1 = this->WhoHas(L);
  int p2 = this->WhoHas(R);

  if ((this->MyId < p1) || (this->MyId > p2)) return 0;

  this->SubGroup = vtkSubGroup::New();
  this->SubGroup->Initialize(p1, p2, this->MyId, tag, 
              this->Controller->GetCommunicator());

  int maxdim = this->SelectCutDirection(kd);

  kd->SetDim(maxdim);

  int midpt = this->Select(maxdim, L, R);

  if (midpt < L + 1)
    {
    // couldn't divide along maxdim - all points we're at same location
    // should probably try a different direction

    kd->SetDim(3);  // indicates region is not divided 
    FreeObject(this->SubGroup);
    return 0;
    }

  float *newDataBounds = this->DataBounds(L, midpt, R);
  vtkKdNode *left = vtkKdNode::New();
  vtkKdNode *right = vtkKdNode::New();

  int fail = ( (newDataBounds == NULL) || (left == NULL) || (right == NULL) );

  if (this->AllCheckForFailure(fail, "Divide Region", "memory allocation"))
    {
    FreeList(newDataBounds);
    left->Delete();
    right->Delete();
    FreeObject(this->SubGroup);
    return -1;
    }

  double coord = (newDataBounds[maxdim*2 + 1] +   // max on left side
                 newDataBounds[6 + maxdim*2] )*   // min on right side
                  0.5;

  kd->AddChildNodes(left, right);

  double bounds[6];
  kd->GetBounds(bounds);

  left->SetBounds(
     bounds[0], ((maxdim == XDIM) ? coord : bounds[1]),
     bounds[2], ((maxdim == YDIM) ? coord : bounds[3]),
     bounds[4], ((maxdim == ZDIM) ? coord : bounds[5]));

  left->SetNumberOfPoints(midpt - L);

  right->SetBounds(
     ((maxdim == XDIM) ? coord : bounds[0]), bounds[1],
     ((maxdim == YDIM) ? coord : bounds[2]), bounds[3],
     ((maxdim == ZDIM) ? coord : bounds[4]), bounds[5]);

  right->SetNumberOfPoints(R - midpt + 1);

  left->SetDataBounds(newDataBounds[0], newDataBounds[1], 
                      newDataBounds[2], newDataBounds[3],
                      newDataBounds[4], newDataBounds[5]);

  right->SetDataBounds(newDataBounds[6], newDataBounds[7], 
                      newDataBounds[8], newDataBounds[9],
                      newDataBounds[10], newDataBounds[11]);

  delete [] newDataBounds;

  FreeObject(this->SubGroup);

  return midpt;
}

void vtkPKdTree::ExchangeVals(int pos1, int pos2)
{
  vtkCommunicator *comm = this->Controller->GetCommunicator();

  float *myval; 
  float otherval[3];

  int player1 = this->WhoHas(pos1);
  int player2 = this->WhoHas(pos2);

  if ((player1 == this->MyId) && (player2 == this->MyId))
    {
    this->ExchangeLocalVals(pos1, pos2);
    }

  else if (player1 == this->MyId)
    {
    myval = this->GetLocalVal(pos1);

    comm->Send(myval, 3, player2, this->SubGroup->tag);

    comm->Receive(otherval, 3, player2, this->SubGroup->tag);

    this->SetLocalVal(pos1, otherval);
    }
  else if (player2 == this->MyId)
    {
    myval = this->GetLocalVal(pos2);

    comm->Receive(otherval, 3, player1, this->SubGroup->tag);

    comm->Send(myval, 3, player1, this->SubGroup->tag);

    this->SetLocalVal(pos2, otherval);
    }
  return;
}

// Given an array X with element indices ranging from L to R, and
// a K such that L <= K <= R, rearrange the elements such that
// X[K] contains the ith sorted element,  where i = K - L + 1, and 
// all the elements X[j], j < k satisfy X[j] <= X[K], and all the
// elements X[j], j > k satisfy X[j] >= X[K].

#define sign(x) ((x<0) ? (-1) : (1))
#ifndef max
#define max(x,y) ((x>y) ? (x) : (y))
#endif
#ifndef min 
#define min(x,y) ((x<y) ? (x) : (y))
#endif

void vtkPKdTree::_select(int L, int R, int K, int dim)
{
  int N, I, J, S, SD, LL, RR;
  float Z;

  while (R > L)
    {
    if ( R - L > 600)
      {
      // "Recurse on a sample of size S to get an estimate for the
      // (K-L+1)-th smallest element into X[K], biased slightly so
      // that the (K-L+1)-th element is expected to lie in the
      // smaller set after partitioning"

      N = R - L + 1;
      I = K - L + 1;
      Z = log(float(N)); 
      S = static_cast<int>(.5 * exp(2*Z/3));
      SD = static_cast<int>(.5 * sqrt(Z*S*((N-S)/N)) * sign(1 - N/2));
      LL = max(L, K - (I*(S/N)) + SD);
      RR = min(R, K + (N-1) * (S/N) + SD);
      this->_select(LL, RR, K, dim);
      }

    int p1 = this->WhoHas(L);
    int p2 = this->WhoHas(R);

    // Processes p1 through p2 will rearrange array elements L through R
    // so they are partitioned by the value at K.  The value at K will
    // appear in array element J, all values less than X[K] will appear
    // between L and J-1, all values greater or equal to X[K] will appear
    // between J+1 and R.

    J = this->PartitionSubArray(L, R, K, dim, p1, p2);

    // "now adjust L,R so they surround the subset containing
    // the (K-L+1)-th smallest element"

    if (J <= K) L = J + 1;
    if (K <= J) R = J - 1;
    }
}
int vtkPKdTree::Select(int dim, int L, int R)
{
  int K = ((R + L) / 2) + 1;

  this->_select(L, R, K, dim);

  if (K == L) return K;

  // The global array is now re-ordered, partitioned around X[K]. 
  // (In particular, for all i, i<K, X[i] <= X[K] and for all i,
  // i > K, X[i] >= X[K].)
  // However the value at X[K] may occur more than once, and by
  // construction of the reordered array, there is a J <= K such that
  // for all i < J, X[i] < X[K] and for all J <= i < K X[i] = X[K].
  //
  // We want to roll K back to this value J, so that all points are
  // unambiguously assigned to one region or the other.

  int hasK = this->WhoHas(K);  
  int hasKrank = this->SubGroup->getLocalRank(hasK);

  int hasKleft = this->WhoHas(K-1);
  int hasKleftrank = this->SubGroup->getLocalRank(hasKleft);

  float Kval;
  float Kleftval;
  float *pt;

  if (hasK == this->MyId)
    {
    pt = this->GetLocalVal(K) + dim;
    Kval = *pt;
    }

  this->SubGroup->Broadcast(&Kval, 1, hasKrank);

  if (hasKleft == this->MyId)
    {
    pt = this->GetLocalVal(K-1) + dim;
    Kleftval = *pt;
    }

  this->SubGroup->Broadcast(&Kleftval, 1, hasKleftrank);

  if (Kleftval != Kval) return K;

  int firstKval = this->TotalNumCells;  // greater than any valid index

  if (this->MyId <= hasKleft)
    {
    int start = this->EndVal[this->MyId];
    if (start > K-1) start = K-1;

    pt = this->GetLocalVal(start) + dim;

    if (*pt == Kval)
      {
      firstKval = start;
    
      int finish = this->StartVal[this->MyId];

      for (int idx=start-1; idx >= finish; idx--)
        {
        pt -= 3;
        if (*pt < Kval) break;

        firstKval--;
        }
      }
    }

  int newK;

  this->SubGroup->ReduceMin(&firstKval, &newK, 1, hasKrank);
  this->SubGroup->Broadcast(&newK, 1, hasKrank);

  return newK;
}

int vtkPKdTree::_whoHas(int L, int R, int pos)
{
  if (L == R) return L;

  int M = (L + R) >> 1;

  if ( pos < this->StartVal[M])
    {
    return _whoHas(L, M-1, pos);
    }
  else if (pos < this->StartVal[M+1])
    {
    return M;
    }
  else
    {
    return _whoHas(M+1, R, pos);
    }
}
int vtkPKdTree::WhoHas(int pos)
{
  if ( (pos < 0) || (pos >= this->TotalNumCells))
    {
    return -1;
    }
  return _whoHas(0, this->NumProcesses-1, pos);
}
float *vtkPKdTree::GetLocalVal(int pos)
{
  if ( (pos < this->StartVal[this->MyId]) || (pos > this->EndVal[this->MyId]))
    {
    return NULL;
    }
  int localPos = pos - this->StartVal[this->MyId];

  return this->CurrentPtArray + (3*localPos);
}
float *vtkPKdTree::GetLocalValNext(int pos)
{
  if ( (pos < this->StartVal[this->MyId]) || (pos > this->EndVal[this->MyId]))
    {
    return NULL;
    }
  int localPos = pos - this->StartVal[this->MyId];

  return this->NextPtArray + (3*localPos);
}
void vtkPKdTree::SetLocalVal(int pos, float *val)
{
  if ( (pos < this->StartVal[this->MyId]) || (pos > this->EndVal[this->MyId]))
    {
    VTKERROR("SetLocalVal - bad index");
    return;
    }

  int localOffset = (pos - this->StartVal[this->MyId]) * 3;

  this->CurrentPtArray[localOffset]   = val[0];
  this->CurrentPtArray[localOffset+1] = val[1];
  this->CurrentPtArray[localOffset+2] = val[2];

  return;
}
void vtkPKdTree::ExchangeLocalVals(int pos1, int pos2)
{
  float temp[3];

  float *pt1 = this->GetLocalVal(pos1);
  float *pt2 = this->GetLocalVal(pos2);

  if (!pt1 || !pt2)
    {
    VTKERROR("ExchangeLocalVal - bad index");
    return;
    }

  temp[0] = pt1[0];
  temp[1] = pt1[1];
  temp[2] = pt1[2];

  pt1[0] = pt2[0];
  pt1[1] = pt2[1];
  pt1[2] = pt2[2];

  pt2[0] = temp[0];
  pt2[1] = temp[1];
  pt2[2] = temp[2];

  return;
}

// Global array [L:R] spans the contiguous processes p1 through p2.  In
// parallel, rearrange the array interval [L:R] so that there is a J
// satisfying all elements in [L:J-1] are < T, element J is T, and all
// elements [J+1:R] are >= T.

int vtkPKdTree::PartitionSubArray(int L, int R, int K, int dim, int p1, int p2)
{
  int TLocation = 0;

  int rootrank = this->SubGroup->getLocalRank(p1);

  int me     = this->MyId;

  if ( (me < p1) || (me > p2))
    {
    this->SubGroup->Broadcast(&TLocation, 1, rootrank);

    return TLocation;
    }

  if (p1 == p2)
    {
    TLocation = this->PartitionAboutMyValue(L, R, K, dim);

    this->SubGroup->Broadcast(&TLocation, 1, rootrank);

    return TLocation;
    }

  // Each process will rearrange their subarray into a left region of values
  // less than X[K] and a right region of values greater or equal to X[K].
  // J will be the index of the first value greater or equal to X[K].  If
  // all values are less, J will be the one index past the last element.
  // In the case of the process holding the Kth array value, X[K] will be
  // found at location J.

  int tag = this->SubGroup->tag;

  vtkSubGroup *sg = vtkSubGroup::New();
  sg->Initialize(p1, p2, me, tag, this->Controller->GetCommunicator());

  int hasK   = this->WhoHas(K);

  int Krank    = sg->getLocalRank(hasK);

  int myL = this->StartVal[me];
  int myR = this->EndVal[me];

  if (myL < L) myL = L;
  if (myR > R) myR = R;

  // Get Kth element
    
  float T;

  if (hasK == me)
    {
    T = this->GetLocalVal(K)[dim];
    }

  sg->Broadcast(&T, 1, Krank);

  int J;   // dividing point in rearranged sub array
    
  if (hasK == me)
    {
    J = this->PartitionAboutMyValue(myL, myR, K, dim);
    }
  else
    {
    J = this->PartitionAboutOtherValue(myL, myR, T, dim);
    }


  // Now the ugly part.  The processes redistribute the array so that
  // globally the interval [L:R] is partitioned by the value formerly
  // at X[K].

  int nprocs = p2 - p1 + 1;

  int *buf  = this->SelectBuffer;

  int *left       = buf; buf += nprocs; // global index of my leftmost
  int *right      = buf; buf += nprocs; // global index of my rightmost
  int *Jval       = buf; buf += nprocs; // global index of my first val >= T

  int *leftArray  = buf; buf += nprocs; // number of my vals < T
  int *leftUsed   = buf; buf += nprocs; // how many scheduled to be sent so far

  int *rightArray = buf; buf += nprocs; // number of my vals >= T
  int *rightUsed  = buf; buf += nprocs; // how many scheduled to be sent so far


  rootrank = sg->getLocalRank(p1);

  sg->Gather(&myL, left, 1, rootrank);
  sg->Broadcast(left, nprocs, rootrank);
  
  sg->Gather(&myR, right, 1, rootrank);
  sg->Broadcast(right, nprocs, rootrank); 

  sg->Gather(&J, Jval, 1, rootrank);
  sg->Broadcast(Jval, nprocs, rootrank);

  sg->Delete();;

  int leftRemaining = 0;

  int p, sndr, recvr;

  for (p = 0; p < nprocs; p++)
    {
    leftArray[p]  = Jval[p] - left[p];
    rightArray[p] = right[p] - Jval[p] + 1;

    leftRemaining += leftArray[p];
                      
    leftUsed[p] = 0;
    rightUsed[p] = 0;
    }

  int nextLeftProc = 0;
  int nextRightProc = 0;

  int need, have, take;

  int FirstRightArrayElementLocation = 0;
  int FirstRight = 1;

  if ( (myL > this->StartVal[me]) || (myR < this->EndVal[me]))
    {
    memcpy(this->NextPtArray, this->CurrentPtArray, this->PtArraySize * sizeof(float));
    }

  for (recvr = 0; recvr < nprocs; recvr++)
    {
    need = leftArray[recvr] + rightArray[recvr];
    have = 0;

    if (leftRemaining >= 0)
      {
      for (sndr = nextLeftProc; sndr < nprocs; sndr++)
        {
        take = leftArray[sndr] - leftUsed[sndr];

        if (take == 0) continue;

        take = (take > need) ? need : take;

        this->DoTransfer(sndr + p1, recvr + p1, 
                         left[sndr] + leftUsed[sndr], left[recvr] + have, take);

        have += take;
        need -= take;

        leftUsed[sndr] += take;

        if (need == 0) break;
        }

      if (leftUsed[sndr] == leftArray[sndr])
        {
        nextLeftProc = sndr+1;
        }
      else
        {
        nextLeftProc = sndr;
        }

      leftRemaining -= have;
      }

    if (need == 0) continue;

    for (sndr = nextRightProc; sndr < nprocs; sndr++)
      {
        take = rightArray[sndr] - rightUsed[sndr];

        if (take == 0) continue;
  
        take = (take > need) ? need : take;
  
        if ((sndr == Krank) && (rightUsed[sndr] == 0))
          {
          TLocation = left[recvr] + have;
          }

        if (FirstRight)
          {
          FirstRightArrayElementLocation = left[recvr] + have;
          FirstRight = 0;
          }

        this->DoTransfer(sndr + p1, recvr + p1, 
                         left[sndr] + leftArray[sndr] + rightUsed[sndr], 
                         left[recvr] + have, take);

        have += take; 
        need -= take;
          
        rightUsed[sndr] += take;
          
        if (need == 0) break;
      }   

    if (rightUsed[sndr] == rightArray[sndr])
      {
      nextRightProc = sndr+1;
      }
    else
      {
      nextRightProc = sndr;
      }   
    }   

  this->SwitchDoubleBuffer();


  if (FirstRightArrayElementLocation != TLocation)
    {
    this->ExchangeVals(FirstRightArrayElementLocation, TLocation);

    TLocation = FirstRightArrayElementLocation;
    }

  rootrank = this->SubGroup->getLocalRank(p1);

  this->SubGroup->Broadcast(&TLocation, 1, rootrank);

  return TLocation;
}

void vtkPKdTree::DoTransfer(int from, int to, int fromIndex, int toIndex, int count)
{
float *fromPt, *toPt;

  vtkCommunicator *comm = this->Controller->GetCommunicator();

  int nitems = count * 3;

  int me = this->MyId;

  int tag = this->SubGroup->tag;

  if ( (from==me) && (to==me))
    {
    fromPt = this->GetLocalVal(fromIndex);
    toPt = this->GetLocalValNext(toIndex);

    memcpy(toPt, fromPt, nitems * sizeof(float));
    }
  else if (from == me)
    {
    fromPt = this->GetLocalVal(fromIndex);

    comm->Send(fromPt, nitems, to, tag);
    }
  else if (to == me)
    {
    toPt = this->GetLocalValNext(toIndex);

    comm->Receive(toPt, nitems, from, tag);
    }

  return;
}

// Rearrange array elements [L:R] such that there is a J where all elements
// [L:J-1] are < T and all elements [J:R] are >= T.  If all elements are
// < T, let J = R+1.

int vtkPKdTree::PartitionAboutOtherValue(int L, int R, float T, int dim)
{
  float *pt, Lval, Rval;

  pt = this->GetLocalVal(L);
  Lval = pt[dim];

  pt = this->GetLocalVal(R);
  Rval = pt[dim];

  int I = L;
  int J = R;

  if ((Lval >= T) && (Rval >= T))
    {
    pt = this->GetLocalVal(J) + dim;

    while (J > I)
      {
         J--;
         pt -= 3;
         if (*pt < T) break;
      }
    }
  else if ((Lval < T) && (Rval < T))
    {
    pt = this->GetLocalVal(I) + dim;

    while (I < J)
      {
      I++;
      pt += 3;
      if (*pt >= T) break;
      }
    }
  else if ((Lval < T) && (Rval >= T))
    {
    this->ExchangeLocalVals(I, J);
    }
  else if ((Lval >= T) && (Rval < T))
    {
    // first loop will fix this
    }

  while (I < J)
    {
    this->ExchangeLocalVals(I, J);

    pt = this->GetLocalVal(I) + dim;

    while (I < J)
      {
      I++;
      pt += 3;
      if (*pt >= T) break;
      }

    pt = this->GetLocalVal(J) + dim;

    while (I < J)
      {
      J--;
      pt -= 3;
      if (*pt < T) break;
      }
    }

  pt = this->GetLocalVal(R);

  if (pt[dim] < T) J = R + 1;

  return J;
} 

// My local array is [L:R] and L <= K <= R, and element K is T.  
// Rearrange the array so that there is a J satisfying all elements
// [L:J-1] are < T, all elements [J+1:R] >= T, and element J is T.

int vtkPKdTree::PartitionAboutMyValue(int L, int R, int K, int dim)
{ 
  float *pt;
  float T;
  int I, J;
  
  // Set up so after first exchange in the loop, we have either
  //   X[L] = T and X[R] >= T
  // or
  //   X[L] < T and X[R] = T
  //

  pt = this->GetLocalVal(K);
  
  T = pt[dim];
    
  this->ExchangeLocalVals(L, K);
    
  pt = this->GetLocalVal(R);
         
  if (pt[dim] >= T) this->ExchangeLocalVals(R, L);
    
  I = L;
  J = R;

  while (I < J)
    {
    this->ExchangeLocalVals(I, J);

    pt = this->GetLocalVal(--J) + dim;
  
    while (J >= L)
    {
      if (*pt < T) break;
    
      J--;
      pt -= 3;
      }
      
    pt = this->GetLocalVal(++I) + dim;
      
    while (I < J)
    {
      if (*pt >= T) break;

      I++;
      pt += 3;
      }

    } 

  if (J < L)
    {
    return L;    // X[L]=T , X[j] >=T for j > L
    }

  // J is location of the first value < T
      
  pt = this->GetLocalVal(L);
      
  float Lval = pt[dim];

  if (Lval == T)
    {
    this->ExchangeLocalVals(L, J);
    } 
  else
    {
    this->ExchangeLocalVals(++J, R);
    }

  return J;
}

//--------------------------------------------------------------------
// Compute the bounds for the data in a region
//--------------------------------------------------------------------

void vtkPKdTree::GetLocalMinMax(int L, int R, int me, 
                                float *min, float *max)
{
  int i, d;
  int from = this->StartVal[me];
  int to   = this->EndVal[me];

  if (L > from)
    {
    from = L;
    }
  if (R < to)
    {
    to = R;
    }

  if (from <= to)
    {
    from -= this->StartVal[me];
    to   -= this->StartVal[me];

    float *val = this->CurrentPtArray + from*3;

    for (d=0; d<3; d++)
      {
      min[d] = max[d] = val[d];
      }

    for (i= from+1; i<=to; i++)
      {
      val += 3;

      for (d=0; d<3; d++)
        {
        if (val[d] < min[d])
          {
          min[d] = val[d];
          }
        else if (val[d] > max[d])
          {
          max[d] = val[d];
          }
        } 
      }
    } 
  else
    {
    // this guy has none of the data, but still must participate
    //   in ReduceMax and ReduceMin

    double *regionMin = this->Top->GetMinBounds();
    double *regionMax = this->Top->GetMaxBounds();

    for (d=0; d<3; d++)
      {
      min[d] = (float)regionMax[d];
      max[d] = (float)regionMin[d];
      }
    }
}
float *vtkPKdTree::DataBounds(int L, int K, int R) 
{
  float localMinLeft[3];    // Left region is L through K-1
  float localMaxLeft[3];
  float globalMinLeft[3];
  float globalMaxLeft[3];
  float localMinRight[3];   // Right region is K through R
  float localMaxRight[3];
  float globalMinRight[3];
  float globalMaxRight[3];
 
  float *globalBounds = new float [12];

  int fail = (globalBounds == NULL);

  if (this->AllCheckForFailure(fail, "DataBounds", "memory allocation"))
    {
    return NULL;
    }

  this->GetLocalMinMax(L, K-1, this->MyId, localMinLeft, localMaxLeft);

  this->GetLocalMinMax(K, R, this->MyId, localMinRight, localMaxRight);

  this->SubGroup->ReduceMin(localMinLeft, globalMinLeft, 3, 0);
  this->SubGroup->Broadcast(globalMinLeft, 3, 0);

  this->SubGroup->ReduceMax(localMaxLeft, globalMaxLeft, 3, 0);
  this->SubGroup->Broadcast(globalMaxLeft, 3, 0);

  this->SubGroup->ReduceMin(localMinRight, globalMinRight, 3, 0);
  this->SubGroup->Broadcast(globalMinRight, 3, 0);

  this->SubGroup->ReduceMax(localMaxRight, globalMaxRight, 3, 0);
  this->SubGroup->Broadcast(globalMaxRight, 3, 0);

  float *left = globalBounds;
  float *right = globalBounds + 6;

  MinMaxToBounds(left, globalMinLeft, globalMaxLeft);

  MinMaxToBounds(right, globalMinRight, globalMaxRight);

  return globalBounds;
}

//--------------------------------------------------------------------
// Complete the tree - Different nodes of tree were computed by different
//   processors.  Now put it together.
//--------------------------------------------------------------------

int vtkPKdTree::CompleteTree()
{
  // calculate depth of entire tree

  int depth;
  int myDepth = vtkPKdTree::ComputeDepth(this->Top);

  this->SubGroup->ReduceMax(&myDepth, &depth, 1, 0); 
  this->SubGroup->Broadcast(&depth, 1, 0); 

  // fill out nodes of tree

  int fail = vtkPKdTree::FillOutTree(this->Top, depth);

  if (this->AllCheckForFailure(fail, "CompleteTree", "memory allocation"))
    {
    return 1;
    }

  // Processor 0 collects all the nodes of the k-d tree, and then
  //   processes the tree to ensure region boundaries are
  //   consistent.  The completed tree is then broadcast.

  int *buf = new int [this->NumProcesses];

  fail = (buf == NULL);

  if (this->AllCheckForFailure(fail, "CompleteTree", "memory allocation"))
    {
    if (buf) delete [] buf;
    return 1;
    }

#ifdef YIELDS_INCONSISTENT_REGION_BOUNDARIES

  this->RetrieveData(this->Top, buf);

#else

  this->ReduceData(this->Top, buf);

  if (this->MyId == 0)
    {
    CheckFixRegionBoundaries(this->Top);
    }

  this->BroadcastData(this->Top);
#endif

  delete [] buf;

  this->SetActualLevel();

  this->BuildRegionList();

  return 0;
}

void vtkPKdTree::PackData(vtkKdNode *kd, double *data)
{
  int i, v;

  data[0] = (double)kd->GetDim();
  data[1] = (double)kd->GetLeft()->GetNumberOfPoints();
  data[2] = (double)kd->GetRight()->GetNumberOfPoints();

  double *lmin = kd->GetLeft()->GetMinBounds();
  double *lmax = kd->GetLeft()->GetMaxBounds();
  double *lminData = kd->GetLeft()->GetMinDataBounds();
  double *lmaxData = kd->GetLeft()->GetMaxDataBounds();
  double *rmin = kd->GetRight()->GetMinBounds();
  double *rmax = kd->GetRight()->GetMaxBounds();
  double *rminData = kd->GetRight()->GetMinDataBounds();
  double *rmaxData = kd->GetRight()->GetMaxDataBounds();

  v = 3;
  for (i=0; i<3; i++)
    {
    data[v++]  = lmin[i];
    data[v++]  = lmax[i];
    data[v++]  = lminData[i];
    data[v++]  = lmaxData[i];
    data[v++]  = rmin[i];
    data[v++]  = rmax[i];
    data[v++]  = rminData[i];
    data[v++]  = rmaxData[i];
    }
} 
void vtkPKdTree::UnpackData(vtkKdNode *kd, double *data)
{
  int i, v;

  kd->SetDim((int)data[0]);
  kd->GetLeft()->SetNumberOfPoints((int)data[1]);
  kd->GetRight()->SetNumberOfPoints((int)data[2]);

  double lmin[3], rmin[3], lmax[3], rmax[3];
  double lminData[3], rminData[3], lmaxData[3], rmaxData[3];

  v = 3;
  for (i=0; i<3; i++)
    {
    lmin[i] = data[v++];
    lmax[i]  = data[v++];
    lminData[i] = data[v++];
    lmaxData[i] = data[v++];
    rmin[i] = data[v++];
    rmax[i] = data[v++];
    rminData[i] = data[v++];
    rmaxData[i] = data[v++];
    }

  kd->GetLeft()->SetBounds(lmin[0], lmax[0], lmin[1], lmax[1], lmin[2], lmax[2]);
  kd->GetLeft()->SetDataBounds(lminData[0], lmaxData[0], 
                               lminData[1], lmaxData[1], lminData[2], lmaxData[2]);

  kd->GetRight()->SetBounds(rmin[0], rmax[0], rmin[1], rmax[1], rmin[2], rmax[2]);
  kd->GetRight()->SetDataBounds(rminData[0], rmaxData[0], 
                               rminData[1], rmaxData[1], rminData[2], rmaxData[2]);
} 
void vtkPKdTree::ReduceData(vtkKdNode *kd, int *sources)
{
  int i;
  double data[27];
  vtkCommunicator *comm = this->Controller->GetCommunicator();

  if (kd->GetLeft() == NULL) return;

  int ihave = (kd->GetDim() < 3);

  this->SubGroup->Gather(&ihave, sources, 1, 0);
  this->SubGroup->Broadcast(sources, this->NumProcesses, 0);

  // a contiguous group of process IDs built this node, the first
  // in the group sends it to node 0 if node 0 doesn't have it

  if (sources[0] == 0)
    {
    int root = -1;

    for (i=1; i<this->NumProcesses; i++)
      {
      if (sources[i])
        {
        root = i;
        break;
        }
      }
    if (root == -1)
      {

      // Normally BuildLocator will create a complete tree, but
      // it may refuse to divide a region if all the data is at
      // the same point along the axis it wishes to divide.  In
      // that case, this region was not divided, so just return.

      vtkKdTree::DeleteAllDescendants(kd);

      return;
      }

    if (root == this->MyId)
      {
      vtkPKdTree::PackData(kd, data);

      comm->Send(data, 27, 0, 0x1111);
      }
    else if (0 == this->MyId)
      {
      comm->Receive(data, 27, root, 0x1111);

      vtkPKdTree::UnpackData(kd, data);
      }
    }

  this->ReduceData(kd->GetLeft(), sources);

  this->ReduceData(kd->GetRight(), sources);

  return;
}
void vtkPKdTree::BroadcastData(vtkKdNode *kd)
{   
  double data[27];

  if (kd->GetLeft() == NULL) return;

  if (0 == this->MyId)
    {
    vtkPKdTree::PackData(kd, data);
    }

  this->SubGroup->Broadcast(data, 27, 0);

  if (this->MyId > 0)
    {
    vtkPKdTree::UnpackData(kd, data);
    }
      
  this->BroadcastData(kd->GetLeft()); 
      
  this->BroadcastData(kd->GetRight());

  return;
}     
void vtkPKdTree::CheckFixRegionBoundaries(vtkKdNode *tree)
{     
  if (tree->GetLeft() == NULL) return;

  int nextDim = tree->GetDim();

  vtkKdNode *left = tree->GetLeft();
  vtkKdNode *right = tree->GetRight();

  double *min = tree->GetMinBounds();
  double *max = tree->GetMaxBounds();
  double *lmin = left->GetMinBounds();
  double *lmax = left->GetMaxBounds();
  double *rmin = right->GetMinBounds();
  double *rmax = right->GetMaxBounds();
    
  for (int dim=0; dim<3; dim++)
    {
    if ((lmin[dim] - min[dim]) != 0.0)  lmin[dim] = min[dim];
    if ((rmax[dim] - max[dim]) != 0.0) rmax[dim] = max[dim];

    if (dim != nextDim)  // the dimension I did *not* divide along
      {
      if ((lmax[dim] - max[dim]) != 0.0)  lmax[dim] = max[dim];
      if ((rmin[dim] - min[dim]) != 0.0) rmin[dim] = min[dim];
      }
    else
      {
      if ((lmax[dim] - rmin[dim]) != 0.0) lmax[dim] = rmin[dim];
      } 
    }

  CheckFixRegionBoundaries(left);
  CheckFixRegionBoundaries(right);
} 
#ifdef YIELDS_INCONSISTENT_REGION_BOUNDARIES

void vtkPKdTree::RetrieveData(vtkKdNode *kd, int *sources)
{
  int i;
  double data[27];

  if (kd->GetLeft() == NULL) return;

  int ihave = (kd->GetDim() < 3);

  this->SubGroup->Gather(&ihave, sources, 1, 0);
  this->SubGroup->Broadcast(sources, this->NumProcesses, 0);

  // a contiguous group of process IDs built this node, the first
  // in the group broadcasts the results to everyone else

  int root = -1;

  for (i=0; i<this->NumProcesses; i++)
    {
    if (sources[i])
      {
      root = i;
      break;
      }
    }
  if (root == -1)
    {
    // Normally BuildLocator will create a complete tree, but
    // it may refuse to divide a region if all the data is at
    // the same point along the axis it wishes to divide.  In
    // that case, this region was not divided, so just return.

    vtkKdTree::DeleteAllDescendants(kd);

    return;
    }

  if (root == this->MyId)
    {
    vtkPKdTree::PackData(kd, data);
    }

  this->SubGroup->Broadcast(data, 27, root);

  if (!ihave)
    {
    vtkPKdTree::UnpackData(kd, data);
    }

  this->RetrieveData(kd->GetLeft(), sources);

  this->RetrieveData(kd->GetRight(), sources);

  return; 
}
#endif

int vtkPKdTree::FillOutTree(vtkKdNode *kd, int level)
{
  if (level == 0) return 0;

  if (kd->GetLeft() == NULL)
    {
    vtkKdNode *left = vtkKdNode::New();

    if (!left) goto doneError2;

    left->SetBounds(-1,-1,-1,-1,-1,-1);
    left->SetDataBounds(-1,-1,-1,-1,-1,-1);
    left->SetNumberOfPoints(-1);

    vtkKdNode *right = vtkKdNode::New();

    if (!right) goto doneError2;

    right->SetBounds(-1,-1,-1,-1,-1,-1);
    right->SetDataBounds(-1,-1,-1,-1,-1,-1);
    right->SetNumberOfPoints(-1);

    kd->AddChildNodes(left, right);
    }

  if (vtkPKdTree::FillOutTree(kd->GetLeft(), level-1)) goto doneError2;

  if (vtkPKdTree::FillOutTree(kd->GetRight(), level-1)) goto doneError2;

  return 0;

doneError2:

  return 1;
}

int vtkPKdTree::ComputeDepth(vtkKdNode *kd)
{
  int leftDepth = 0; 
  int rightDepth = 0;

  if ((kd->GetLeft() == NULL) && (kd->GetRight() == NULL)) return 0;

  if (kd->GetLeft())
    {
    leftDepth = vtkPKdTree::ComputeDepth(kd->GetLeft());
    }
  if (kd->GetRight())
    {
    rightDepth = vtkPKdTree::ComputeDepth(kd->GetRight());
    }

  if (leftDepth > rightDepth) return leftDepth + 1;
  else return rightDepth + 1;
}

//--------------------------------------------------------------------
// lists, lists, lists
//--------------------------------------------------------------------

int vtkPKdTree::AllocateDoubleBuffer()
{
  this->FreeDoubleBuffer();

  this->PtArraySize = this->NumCells[this->MyId] * 3;

  this->PtArray2 = new float [this->PtArraySize];

  this->CurrentPtArray = this->PtArray;
  this->NextPtArray    = this->PtArray2;

  return (this->PtArray2 == NULL);
}
void vtkPKdTree::SwitchDoubleBuffer()
{
  float *temp = this->CurrentPtArray;

  this->CurrentPtArray = this->NextPtArray;
  this->NextPtArray = temp;
}
void vtkPKdTree::FreeDoubleBuffer()
{
  FreeList(this->PtArray2);
  this->CurrentPtArray = this->PtArray;
  this->NextPtArray = NULL;
}

int vtkPKdTree::AllocateSelectBuffer()
{
  this->FreeSelectBuffer();

  this->SelectBuffer = new int [this->NumProcesses * 7];

  return (this->SelectBuffer == NULL);
}
void vtkPKdTree::FreeSelectBuffer()
{
  if (this->SelectBuffer)
    {
    delete [] this->SelectBuffer;
    this->SelectBuffer = NULL;
    }
}

#define FreeListOfLists(list, len) \
{                                  \
  int i;                           \
  if (list)                        \
    {                              \
    for (i=0; i<len; i++)          \
      {                            \
      if (list[i]) delete [] list[i]; \
      }                                 \
    delete [] list;                   \
    list = NULL;                      \
    }                                   \
}

#define MakeList(field, type, len) \
  {                                \
   field = new type [len];         \
   if (field) memset(field, 0, (len) * sizeof(type));  \
  }

// global index lists -----------------------------------------------

void vtkPKdTree::InitializeGlobalIndexLists()
{
  this->StartVal = NULL;
  this->EndVal   = NULL;
  this->NumCells = NULL;
}
int vtkPKdTree::AllocateAndZeroGlobalIndexLists()
{
  this->FreeGlobalIndexLists();

  MakeList(this->StartVal, int, this->NumProcesses);
  MakeList(this->EndVal, int, this->NumProcesses);
  MakeList(this->NumCells, int, this->NumProcesses);

  int defined = ((this->StartVal != NULL) && 
                 (this->EndVal != NULL) && 
                 (this->NumCells != NULL));

  if (!defined) this->FreeGlobalIndexLists();

  return !defined;
}
void vtkPKdTree::FreeGlobalIndexLists()
{
  FreeList(StartVal);
  FreeList(EndVal);
  FreeList(NumCells);
}
int vtkPKdTree::BuildGlobalIndexLists(int numMyCells)
{
  int fail = this->AllocateAndZeroGlobalIndexLists();

  if (this->AllCheckForFailure(fail, "BuildGlobalIndexLists", "memory allocation"))
    {
    this->FreeGlobalIndexLists();
    return 1;
    }

  this->SubGroup->Gather(&numMyCells, this->NumCells, 1, 0);

  this->SubGroup->Broadcast(this->NumCells, this->NumProcesses, 0);

  this->StartVal[0] = 0;
  this->EndVal[0] = this->NumCells[0] - 1; 

  this->TotalNumCells = this->NumCells[0]; 

  for (int i=1; i<this->NumProcesses; i++)
    {
    this->StartVal[i] = this->EndVal[i-1] + 1;
    this->EndVal[i]   = this->EndVal[i-1] + this->NumCells[i];

    this->TotalNumCells += this->NumCells[i];
    }

  return 0;
}

// Region assignment lists ---------------------------------------------

void vtkPKdTree::InitializeRegionAssignmentLists()
{
  this->RegionAssignmentMap = NULL;
  this->ProcessAssignmentMap = NULL;
  this->NumRegionsAssigned  = NULL;
}
int vtkPKdTree::AllocateAndZeroRegionAssignmentLists()
{
  this->FreeRegionAssignmentLists();

  this->RegionAssignmentMapLength = this->GetNumberOfRegions();

  MakeList(this->RegionAssignmentMap, int , this->GetNumberOfRegions());
  MakeList(this->NumRegionsAssigned, int, this->NumProcesses);

  MakeList(this->ProcessAssignmentMap, int *, this->NumProcesses);

  int defined = ((this->RegionAssignmentMap != NULL) &&
                 (this->ProcessAssignmentMap != NULL) &&
                 (this->NumRegionsAssigned != NULL) );

  if (!defined) this->FreeRegionAssignmentLists();

  return !defined;
}
void vtkPKdTree::FreeRegionAssignmentLists()
{
  FreeList(this->RegionAssignmentMap);
  FreeList(this->NumRegionsAssigned);
  FreeListOfLists(this->ProcessAssignmentMap, this->NumProcesses);

  this->RegionAssignmentMapLength = 0;
}

// Process data tables ------------------------------------------------

void vtkPKdTree::InitializeProcessDataLists()
{
  this->DataLocationMap = NULL;

  this->NumProcessesInRegion = NULL;
  this->ProcessList = NULL;

  this->NumRegionsInProcess = NULL;
  this->RegionList = NULL;

  this->CellCountList = NULL;
}

int vtkPKdTree::AllocateAndZeroProcessDataLists()
{
  int nRegions = this->GetNumberOfRegions();
  int nProcesses = this->NumProcesses;

  this->FreeProcessDataLists();

  MakeList(this->DataLocationMap, char, nRegions * nProcesses);

  if (this->DataLocationMap == NULL) goto doneError3;

  MakeList(this->NumProcessesInRegion, int ,nRegions);

  if (this->NumProcessesInRegion == NULL) goto doneError3;

  MakeList(this->ProcessList, int * ,nRegions);

  if (this->ProcessList == NULL) goto doneError3;

  MakeList(this->NumRegionsInProcess, int ,nProcesses);

  if (this->NumRegionsInProcess == NULL) goto doneError3;

  MakeList(this->RegionList, int * ,nProcesses);

  if (this->RegionList == NULL) goto doneError3;

  MakeList(this->CellCountList, int * ,nRegions);

  if (this->CellCountList == NULL) goto doneError3;

  return 0;

doneError3:
  this->FreeProcessDataLists();
  return 1;
}
void vtkPKdTree::FreeProcessDataLists()
{
  int nRegions = this->GetNumberOfRegions();
  int nProcesses = this->NumProcesses;

  FreeListOfLists(this->CellCountList, nRegions);

  FreeListOfLists(this->RegionList, nProcesses);

  FreeList(this->NumRegionsInProcess);

  FreeListOfLists(this->ProcessList, nRegions);

  FreeList(this->NumProcessesInRegion);

  FreeList(this->DataLocationMap);
}

// Field array global min and max -----------------------------------

void vtkPKdTree::InitializeFieldArrayMinMax()
{
  this->NumCellArrays = this->NumPointArrays = 0;
  this->CellDataMin = this->CellDataMax = NULL;
  this->PointDataMin = this->PointDataMax = NULL;
  this->CellDataName = NULL;
  this->PointDataName = NULL;
}

int vtkPKdTree::AllocateAndZeroFieldArrayMinMax()
{
  int iNumCellArrays = this->GetDataSet(0)->GetCellData()->GetNumberOfArrays();
  int iNumPointArrays = this->GetDataSet(0)->GetPointData()->GetNumberOfArrays();

  this->FreeFieldArrayMinMax();

  if (iNumCellArrays > 0)
    {
    MakeList(this->CellDataMin, double, iNumCellArrays);
    if (this->CellDataMin == NULL) goto doneError5;

    MakeList(this->CellDataMax, double, iNumCellArrays);
    if (this->CellDataMax == NULL) goto doneError5;

    MakeList(this->CellDataName, char *, iNumCellArrays);
    if (this->CellDataName == NULL) goto doneError5;
    }

  this->NumCellArrays = iNumCellArrays;

  if (iNumPointArrays > 0)
    {
    MakeList(this->PointDataMin, double, iNumPointArrays);
    if (this->PointDataMin == NULL) goto doneError5;

    MakeList(this->PointDataMax, double, iNumPointArrays);
    if (this->PointDataMax == NULL) goto doneError5;

    MakeList(this->PointDataName, char *, iNumPointArrays);
    if (this->PointDataName == NULL) goto doneError5;
    }

  this->NumPointArrays = iNumPointArrays;

  return 0;

doneError5:
  this->FreeFieldArrayMinMax();
  return 1;
}
void vtkPKdTree::FreeFieldArrayMinMax()
{
  FreeList(this->CellDataMin);
  FreeList(this->CellDataMax);
  FreeList(this->PointDataMin);
  FreeList(this->PointDataMax);

  FreeListOfLists(this->CellDataName, this->NumCellArrays);
  FreeListOfLists(this->PointDataName, this->NumPointArrays);

  this->NumCellArrays = this->NumPointArrays = 0;
}

void vtkPKdTree::ReleaseTables()
{
  this->FreeRegionAssignmentLists();
  this->FreeProcessDataLists();
  this->FreeFieldArrayMinMax();
}

//--------------------------------------------------------------------
// Create tables indicating which processes have data for which
//  regions.  
//--------------------------------------------------------------------

int vtkPKdTree::CreateProcessCellCountData()
{
  int proc, reg;
  int retval = 0;
  int *cellCounts = NULL;
  int *tempbuf;
  char *procData, *myData; 

  tempbuf = NULL;
  procData = myData = NULL;

  this->SubGroup = vtkSubGroup::New();
  this->SubGroup->Initialize(0, this->NumProcesses-1,
             this->MyId, 0x0000f000, this->Controller->GetCommunicator());

  int fail = this->AllocateAndZeroProcessDataLists();

  if (!fail && !this->Top) fail = 1;

  if (this->AllCheckForFailure(fail, "BuildRegionProcessTables", "memory allocation"))
    {
    this->FreeProcessDataLists();
    this->SubGroup->Delete();
    this->SubGroup = NULL;
    return 1;
    }

  // Build table indicating which processes have data for which regions

  cellCounts = this->CollectLocalRegionProcessData();

  fail = (cellCounts == NULL);

  if (this->AllCheckForFailure(fail,"BuildRegionProcessTables","error"))
    {
    goto doneError4;
    }

  myData = this->DataLocationMap + (this->MyId * this->GetNumberOfRegions());

  for (reg=0; reg < this->GetNumberOfRegions(); reg++)
    {
    if (cellCounts[reg] > 0) myData[reg] = 1;
    }

  if (this->NumProcesses > 1)
    {
    this->SubGroup->Gather(myData, this->DataLocationMap,
                         this->GetNumberOfRegions(), 0);
    this->SubGroup->Broadcast(this->DataLocationMap, 
                    this->GetNumberOfRegions() * this->NumProcesses, 0);
    }

  // Other helpful tables - not the fastest way to create this
  //   information, but it uses the least memory

  procData = this->DataLocationMap;

  for (proc=0; proc<this->NumProcesses; proc++)
    {
    for (reg=0; reg < this->GetNumberOfRegions(); reg++)
      {
      if (*procData) 
        {
        this->NumProcessesInRegion[reg]++;
        this->NumRegionsInProcess[proc]++;
        }
      procData++;
      }
    }
  for (reg=0; reg < this->GetNumberOfRegions(); reg++)
    {
    int nprocs = this->NumProcessesInRegion[reg];

    if (nprocs > 0)
      {
      this->ProcessList[reg] = new int [nprocs];
      this->ProcessList[reg][0] = -1;
      this->CellCountList[reg] = new int [nprocs];
      this->CellCountList[reg][0] = -1;
      }
    }
  for (proc=0; proc < this->NumProcesses; proc++)
    {
    int nregs = this->NumRegionsInProcess[proc];

    if (nregs > 0)
      {
      this->RegionList[proc] = new int [nregs];
      this->RegionList[proc][0] = -1;
      }
    }

  procData = this->DataLocationMap;

  for (proc=0; proc<this->NumProcesses; proc++)
    {

    for (reg=0; reg < this->GetNumberOfRegions(); reg++)
    {
      if (*procData) 
        {
        this->AddEntry(this->ProcessList[reg], 
                       this->NumProcessesInRegion[reg], proc);

        this->AddEntry(this->RegionList[proc], 
                       this->NumRegionsInProcess[proc], reg);
        }
      procData++;
      }
    }

  // Cell counts per process per region

  if (this->NumProcesses > 1)
    {
    tempbuf = new int [this->GetNumberOfRegions() * this->NumProcesses];

    fail = (tempbuf == NULL);

    if (this->AllCheckForFailure(fail,"BuildRegionProcessTables","memory allocation"))
      {
      goto doneError4;
      }

    this->SubGroup->Gather(cellCounts, tempbuf, this->GetNumberOfRegions(), 0);
    this->SubGroup->Broadcast(tempbuf, this->NumProcesses*this->GetNumberOfRegions(), 0);
    }
  else
    {
    tempbuf = cellCounts;
    }

  for (proc=0; proc<this->NumProcesses; proc++)
    {
    int *procCount = tempbuf + (proc * this->GetNumberOfRegions());

    for (reg=0; reg < this->GetNumberOfRegions(); reg++)
      {
      if (procCount[reg]> 0)
        {
        this->AddEntry(this->CellCountList[reg], 
                          this->NumProcessesInRegion[reg],
                          procCount[reg]);
        }
      }
    }

  goto done4;

doneError4:
  
  this->FreeProcessDataLists();
  retval = 1;

done4:

  if (tempbuf != cellCounts)
    {
    FreeList(tempbuf);
    }

  FreeList(cellCounts);
  this->SubGroup->Delete();
  this->SubGroup = NULL;
  
  return retval;
}

//--------------------------------------------------------------------
// Create list of global min and max for cell and point field arrays
//--------------------------------------------------------------------

int vtkPKdTree::CreateGlobalDataArrayBounds()
{
  this->SubGroup = NULL;

  if (this->NumProcesses > 1)
    {
    this->SubGroup = vtkSubGroup::New();
    this->SubGroup->Initialize(0, this->NumProcesses-1,
               this->MyId, 0x0000f000, this->Controller->GetCommunicator());
    }

  int fail = this->AllocateAndZeroFieldArrayMinMax();
  
  if (this->AllCheckForFailure(fail, "BuildFieldArrayMinMax", "memory allocation"))
    {
    this->FreeFieldArrayMinMax();
    FreeObject(this->SubGroup);
    return 1;
    }

  TIMER("Get global ranges");
  
  int ar;
  double range[2];
  int ncells = this->NumCellArrays;
  int npoints = this->NumPointArrays;

  if (ncells > 0)
    {
    for (ar=0; ar<ncells; ar++)
      {
      vtkDataArray *array = this->GetDataSet(0)->GetCellData()->GetArray(ar);
      
      array->GetRange(range);

      this->CellDataMin[ar] = range[0];
      this->CellDataMax[ar] = range[1];

      this->CellDataName[ar] = vtkPKdTree::StrDupWithNew(array->GetName());
      }

    if (this->NumProcesses > 1)
      {
      this->SubGroup->ReduceMin(this->CellDataMin, this->CellDataMin, ncells, 0);
      this->SubGroup->Broadcast(this->CellDataMin, ncells, 0);
  
      this->SubGroup->ReduceMax(this->CellDataMax, this->CellDataMax, ncells, 0);
      this->SubGroup->Broadcast(this->CellDataMax, ncells, 0);
      }
    }

  if (npoints > 0)
    {
    for (ar=0; ar<npoints; ar++)
      {
      vtkDataArray *array = this->GetDataSet(0)->GetPointData()->GetArray(ar);

      array->GetRange(range);

      this->PointDataMin[ar] = range[0];
      this->PointDataMax[ar] = range[1];

      this->PointDataName[ar] = StrDupWithNew(array->GetName());
      }

    if (this->NumProcesses > 1)
      {
      this->SubGroup->ReduceMin(this->PointDataMin, this->PointDataMin, npoints, 0);
      this->SubGroup->Broadcast(this->PointDataMin, npoints, 0);

      this->SubGroup->ReduceMax(this->PointDataMax, this->PointDataMax, npoints, 0);
      this->SubGroup->Broadcast(this->PointDataMax, npoints, 0);
      }
    }

  TIMERDONE("Get global ranges");

  FreeObject(this->SubGroup);

  return 0;
}
int *vtkPKdTree::CollectLocalRegionProcessData()
{
  int *cellCounts;

  int numRegions = this->GetNumberOfRegions();

  MakeList(cellCounts, int, numRegions);

  if (!cellCounts)
    {
     VTKERROR("CollectLocalRegionProcessData - memory allocation");
     return NULL;
    }

  TIMER("Get cell regions");

  int *IDs = this->AllGetRegionContainingCell();

  TIMERDONE("Get cell regions");

  for (int set=0; set < this->GetNumberOfDataSets(); set++)
    {
    int ncells = this->GetDataSet(set)->GetNumberOfCells();

    TIMER("Increment cell counts");

    for (int i=0; i<ncells; i++)
      {
      int regionId = IDs[i];

      if ( (regionId < 0) || (regionId >= numRegions))
        {
        VTKERROR("CollectLocalRegionProcessData - corrupt data");
        return NULL;
        }
      cellCounts[regionId]++;
      }

    IDs += ncells;

    TIMERDONE("Increment cell counts");
    }

  return cellCounts;
}
void vtkPKdTree::AddEntry(int *list, int len, int id)
{
  int i=0;

  while ((i < len) && (list[i] != -1)) i++;

  if (i == len) return;  // error

  list[i++] = id;

  if (i < len) list[i] = -1;
}
int vtkPKdTree::BinarySearch(int *list, int len, int which)
{
int mid, left, right;

  mid = -1;

  if (len <= 3)
    {
    for (int i=0; i<len; i++)
      {
      if (list[i] == which)
        {
        mid=i;
        break;
        }
      }
    }
  else
    {
    mid = len >> 1;
    left = 0;
    right = len-1;

    while (list[mid] != which)
      {
      if (list[mid] < which)
        {
        left = mid+1;
        }
      else
        {
        right = mid-1;
        }

      if (right > left+1)
        {
        mid = (left + right) >> 1;
        }
      else
        {
        if (list[left] == which) mid = left;
        else if (list[right] == which) mid = right;
        else mid = -1;
        break;
        }
      }
    }
  return mid;
}
//--------------------------------------------------------------------
// Assign responsibility for each spatial region to one process
//--------------------------------------------------------------------

int vtkPKdTree::UpdateRegionAssignment()
{
  int returnVal = 0;

  if (this->RegionAssignment== ContiguousAssignment)
    {
    returnVal = this->AssignRegionsContiguous();
    }
  else if (this->RegionAssignment== RoundRobinAssignment)
    {
    returnVal = this->AssignRegionsRoundRobin();
    }

  return returnVal;
}
int vtkPKdTree::AssignRegionsRoundRobin()
{
  this->RegionAssignment = RoundRobinAssignment;

  if (this->Top == NULL)
    {
    return 0;
    }

  int nProcesses = this->NumProcesses;
  int nRegions = this->GetNumberOfRegions();

  int fail = this->AllocateAndZeroRegionAssignmentLists();

  if (fail)
    {
    return 1;
    }

  for (int i=0, procID=0; i<nRegions; i++)
    {
    this->RegionAssignmentMap[i] = procID;
    this->NumRegionsAssigned[procID]++;

    procID = ( (procID == nProcesses-1) ? 0 : procID+1);
    }
  this->BuildRegionListsForProcesses();

  return 0;
}
int vtkPKdTree::AssignRegions(int *map, int len)
{
  int fail = this->AllocateAndZeroRegionAssignmentLists();

  if (fail)
    {
    return 1;
    }

  this->RegionAssignmentMapLength = len;

  this->RegionAssignment = UserDefinedAssignment;

  for (int i=0; i<len; i++)
    {
    if ( (map[i] < 0) || (map[i] >= this->NumProcesses))
      {
      this->FreeRegionAssignmentLists();
      VTKERROR("AssignRegions - invalid process id in map");
      return 1;
      }

    this->RegionAssignmentMap[i] = map[i];
    this->NumRegionsAssigned[map[i]]++;
    }
  
  this->BuildRegionListsForProcesses();

  return 0;
}
void vtkPKdTree::AddProcessRegions(int procId, vtkKdNode *kd)
{
  vtkIntArray *leafNodeIds = vtkIntArray::New();

  vtkKdTree::GetLeafNodeIds(kd, leafNodeIds);

  int nLeafNodes = leafNodeIds->GetNumberOfTuples();

  for (int n=0; n<nLeafNodes; n++)
    {
    this->RegionAssignmentMap[leafNodeIds->GetValue(n)] = procId;
    this->NumRegionsAssigned[procId]++;
    }

  leafNodeIds->Delete();
}

int vtkPKdTree::AssignRegionsContiguous()
{
  int p;

  this->RegionAssignment = ContiguousAssignment;

  if (this->Top == NULL)
    {
    return 0;
    }

  int nProcesses = this->NumProcesses;
  int nRegions = this->GetNumberOfRegions();

  if (nRegions <= nProcesses)
    {
    this->AssignRegionsRoundRobin();
    return 0;
    }

  int fail = this->AllocateAndZeroRegionAssignmentLists();

  if (fail)
    {
    return 1;
    }

  int floorLogP, ceilLogP;

  for (floorLogP = 0; (nProcesses >> floorLogP) > 0; floorLogP++);
  floorLogP--;

  int P = 1 << floorLogP;

  if (nProcesses == P) ceilLogP = floorLogP;
  else                 ceilLogP = floorLogP + 1;

  vtkKdNode **nodes = new vtkKdNode* [P];

  this->GetRegionsAtLevel(floorLogP, nodes);

  if (floorLogP == ceilLogP)
    {
    for (p=0; p<nProcesses; p++)
      {
      this->AddProcessRegions(p, nodes[p]);
      }
    }
  else
    {
    int nodesLeft = 1 << ceilLogP;
    int procsLeft = nProcesses;
    int procId = 0;

    for (int i=0; i<P; i++)
      {
      if (nodesLeft > procsLeft)
        {
        this->AddProcessRegions(procId, nodes[i]);

        procsLeft -= 1;
        procId    += 1;
        }
      else
        {
        this->AddProcessRegions(procId, nodes[i]->GetLeft());
        this->AddProcessRegions(procId+1, nodes[i]->GetRight());

        procsLeft -= 2;
        procId    += 2;
        }
      nodesLeft -= 2;
      }
    }

  delete [] nodes;

  this->BuildRegionListsForProcesses();

  return 0;
}
void vtkPKdTree::BuildRegionListsForProcesses()
{ 
  int *count = new int [this->NumProcesses];

  for (int p=0; p<this->NumProcesses; p++)
    {
    int nregions = this->NumRegionsAssigned[p];
  
    if (nregions > 0)
      {
      this->ProcessAssignmentMap[p] = new int [nregions];
      }
    else
      {
      this->ProcessAssignmentMap[p] = NULL;
      }

    count[p] = 0;
    }   
    
  for (int r=0; r<this->RegionAssignmentMapLength; r++)
    {
    int proc = this->RegionAssignmentMap[r];
    int next = count[proc];
    
    this->ProcessAssignmentMap[proc][next] = r;

    count[proc] = next + 1;
    }
      
  delete [] count;
}       

//--------------------------------------------------------------------
// Queries
//--------------------------------------------------------------------
int vtkPKdTree::FindLocalArrayIndex(const char *n, const char **names, int len)
{
  int index = -1;
  int nsize = strlen(n);

  // normally a very small list, maybe 1 to 5 names

  for (int i=0; i<len; i++)
    {
    if (!strncmp(n, names[i], nsize))
      {
      index = i;
      break;
      }
    }
  return index;
}
int vtkPKdTree::GetCellArrayGlobalRange(const char *n, float range[2])
{
  int index = 
    vtkPKdTree::FindLocalArrayIndex(n, (const char **)this->CellDataName,
                                    this->NumCellArrays);

  return this->GetCellArrayGlobalRange(index, range);
}
int vtkPKdTree::GetCellArrayGlobalRange(const char *n, double range[2])
{
  int index = 
    vtkPKdTree::FindLocalArrayIndex(n, (const char **)this->CellDataName,
                                    this->NumCellArrays);

  return this->GetCellArrayGlobalRange(index, range);
}
int vtkPKdTree::GetPointArrayGlobalRange(const char *n, float range[2])
{
  int index = 
    vtkPKdTree::FindLocalArrayIndex(n, (const char **)this->PointDataName,
                                    this->NumPointArrays);

  return this->GetPointArrayGlobalRange(index, range);
}
int vtkPKdTree::GetPointArrayGlobalRange(const char *n, double range[2])
{
  int index = 
    vtkPKdTree::FindLocalArrayIndex(n, (const char **)this->PointDataName,
                                    this->NumPointArrays);

  return this->GetPointArrayGlobalRange(index, range);
}

int vtkPKdTree::GetCellArrayGlobalRange(int arrayIndex, float range[2])
{
  double drange[2];
  int val = this->GetCellArrayGlobalRange(arrayIndex, drange);

  if (val == 0)
    {
    range[0] = (float)drange[0];
    range[1] = (float)drange[1];
    }

  return val; 
}
int vtkPKdTree::GetCellArrayGlobalRange(int arrayIndex, double range[2])
{
  if ((arrayIndex < 0) || (arrayIndex >= this->NumCellArrays))
    {
    return 1;
    }
  if (this->CellDataMin == NULL) return 1;

  range[0] = this->CellDataMin[arrayIndex];
  range[1] = this->CellDataMax[arrayIndex];

  return 0;
}
int vtkPKdTree::GetPointArrayGlobalRange(int arrayIndex, float range[2])
{
  double drange[2];
  int val = this->GetPointArrayGlobalRange(arrayIndex, drange);

  if (val == 0)
    {
    range[0] = (float)drange[0];
    range[1] = (float)drange[1];
    }

  return val; 
}
int vtkPKdTree::GetPointArrayGlobalRange(int arrayIndex, double range[2])
{
  if ((arrayIndex < 0) || (arrayIndex >= this->NumPointArrays))
    {
    return 1;
    }
  if (this->PointDataMin == NULL) return 1;

  range[0] = this->PointDataMin[arrayIndex];
  range[1] = this->PointDataMax[arrayIndex];

  return 0;
}
int vtkPKdTree::DepthOrderAllProcesses(vtkCamera *camera, vtkIntArray *orderedList)
{ 
  vtkIntArray *regionList = vtkIntArray::New();
  
  this->DepthOrderAllRegions(camera, regionList);
  
  orderedList->SetNumberOfValues(this->NumProcesses);
  
  int nextId = 0;
  
  // if regions were not assigned contiguously, this
  // produces the wrong result
  
  for (int r=0; r<this->GetNumberOfRegions(); )
    {
    int procId = this->RegionAssignmentMap[regionList->GetValue(r)];
    
    orderedList->SetValue(nextId++, procId);
    
    int nregions = this->NumRegionsAssigned[procId];
    
    r += nregions;
    }
  
  regionList->Delete();
  
  return this->NumProcesses;
}

int vtkPKdTree::GetRegionAssignmentList(int procId, vtkIntArray *list)
{
  if ( (procId < 0) || (procId >= this->NumProcesses))
    {
    VTKERROR("GetRegionAssignmentList - invalid process id");
    return 0;
    }

  if (!this->RegionAssignmentMap)
    {
    this->UpdateRegionAssignment();

    if (!this->RegionAssignmentMap)
      {
      return 0;
      }
    }

  int nregions = this->NumRegionsAssigned[procId];
  int *regionIds = this->ProcessAssignmentMap[procId];

  list->Initialize();
  list->SetNumberOfValues(nregions);

  for (int i=0; i<nregions; i++)
    {
    list->SetValue(i, regionIds[i]);
    }

  return nregions;
}
void vtkPKdTree::GetAllProcessesBorderingOnPoint(float x, float y, float z, 
                          vtkIntArray *list)
{
  vtkIntArray *regions = vtkIntArray::New();
  double *subRegionBounds;
  list->Initialize();

  for (int procId = 0; procId < this->NumProcesses; procId++)
    {
    this->GetRegionAssignmentList(procId, regions);

    int nSubRegions = this->MinimalNumberOfConvexSubRegions(
          regions, &subRegionBounds);

    double *b = subRegionBounds;

    for (int r=0; r<nSubRegions; r++)
      {
      if ((((x == b[0]) || (x == b[1])) &&
           ((y >= b[2]) && (y <= b[3]) && (z >= b[4]) && (z <= b[5]))) ||
          (((y == b[2]) || (y == b[3])) &&
           ((x >= b[0]) && (x <= b[1]) && (z >= b[4]) && (z <= b[5]))) ||
          (((z == b[4]) || (z == b[5])) &&
           ((x >= b[0]) && (x <= b[1]) && (y >= b[2]) && (y <= b[3]))) )
        {
        list->InsertNextValue(procId);
        break;
        }

      b += 6;
      }
    }

  regions->Delete();
}

int vtkPKdTree::GetProcessAssignedToRegion(int regionId)
{
  if ( !this->RegionAssignmentMap ||
       (regionId < 0) || (regionId >= this->GetNumberOfRegions()))
    {
    return -1;
    }

  return this->RegionAssignmentMap[regionId];
}
int vtkPKdTree::HasData(int processId, int regionId)
{
  if ((!this->DataLocationMap) ||
      (processId < 0) || (processId >= this->NumProcesses) ||
      (regionId < 0) || (regionId >= this->GetNumberOfRegions())   ) 
    {
    VTKERROR("HasData - invalid request");
    return 0;
    }

  int where = this->GetNumberOfRegions() * processId + regionId;

  return this->DataLocationMap[where];
}

int vtkPKdTree::GetTotalProcessesInRegion(int regionId)
{
  if (!this->NumProcessesInRegion ||
      (regionId < 0) || (regionId >= this->GetNumberOfRegions())   ) 
    {
    VTKERROR("GetTotalProcessesInRegion - invalid request");
    return 0;
    }

  return this->NumProcessesInRegion[regionId];
}

int vtkPKdTree::GetProcessListForRegion(int regionId, vtkIntArray *processes)
{
  if (!this->ProcessList ||
      (regionId < 0) || (regionId >= this->GetNumberOfRegions())   ) 
    {
    VTKERROR("GetProcessListForRegion - invalid request");
    return 0;
    }

  int nProcesses = this->NumProcessesInRegion[regionId];

  for (int i=0; i<nProcesses; i++)
    {
    processes->InsertNextValue(this->ProcessList[regionId][i]);
    }

  return nProcesses;
}

int vtkPKdTree::GetProcessesCellCountForRegion(int regionId, int *count, int len)
{
  if (!this->CellCountList ||
      (regionId < 0) || (regionId >= this->GetNumberOfRegions())   ) 
    {
    VTKERROR("GetProcessesCellCountForRegion - invalid request");
    return 0;
    }

  int nProcesses = this->NumProcessesInRegion[regionId];

  nProcesses = (len < nProcesses) ? len : nProcesses;

  for (int i=0; i<nProcesses; i++)
    {
    count[i] = this->CellCountList[regionId][i];
    }

  return nProcesses;
}

int vtkPKdTree::GetProcessCellCountForRegion(int processId, int regionId)
{
  int nCells;

  if (!this->CellCountList ||
      (regionId < 0) || (regionId >= this->GetNumberOfRegions()) ||
      (processId < 0) || (processId >= this->NumProcesses))
    { 
    VTKERROR("GetProcessCellCountForRegion - invalid request");
    return 0;
    }

  int nProcesses = this->NumProcessesInRegion[regionId];

  int which = -1;

  for (int i=0; i<nProcesses; i++)
    {
    if (this->ProcessList[regionId][i] == processId)
      {
      which = i;
      break;
      }
    }

  if (which == -1) nCells = 0;
  else             nCells = this->CellCountList[regionId][which];

  return nCells;
}

int vtkPKdTree::GetTotalRegionsForProcess(int processId)
{
  if ((!this->NumRegionsInProcess) ||
      (processId < 0) || (processId >= this->NumProcesses))
    { 
    VTKERROR("GetTotalRegionsForProcess - invalid request");
    return 0;
    }

  return this->NumRegionsInProcess[processId];
}

int vtkPKdTree::GetRegionListForProcess(int processId, vtkIntArray *regions)
{
  if (!this->RegionList ||
      (processId < 0) || (processId >= this->NumProcesses))
    { 
    VTKERROR("GetRegionListForProcess - invalid request");
    return 0;
    }

  int nRegions = this->NumRegionsInProcess[processId];

  for (int i=0; i<nRegions; i++)
    {
    regions->InsertNextValue(this->RegionList[processId][i]);
    }

  return nRegions;
}
int vtkPKdTree::GetRegionsCellCountForProcess(int processId, int *count, int len)
{
  if (!this->CellCountList ||
      (processId < 0) || (processId >= this->NumProcesses))
    { 
    VTKERROR("GetRegionsCellCountForProcess - invalid request");
    return 0;
    }

  int nRegions = this->NumRegionsInProcess[processId];

  nRegions = (len < nRegions) ? len : nRegions;

  for (int i=0; i<nRegions; i++)
    {
    int regionId = this->RegionList[processId][i];
    int iam;

    for (iam = 0; iam < this->NumProcessesInRegion[regionId]; iam++)
      {
      if (this->ProcessList[regionId][iam] == processId) break;
      }

    count[i] = this->CellCountList[regionId][iam];
    }
  return nRegions;
}
vtkIdType vtkPKdTree::GetCellListsForProcessRegions(int processId,
          int set, vtkIdList *inRegionCells, vtkIdList *onBoundaryCells)
{
  if ( (set < 0) || (set >= this->GetNumberOfDataSets()))
    {
    vtkErrorMacro(<<"vtkPKdTree::GetCellListsForProcessRegions no such data set");
    return 0;
    }
  return this->GetCellListsForProcessRegions(processId,
            this->GetDataSet(set), inRegionCells, onBoundaryCells);
}
vtkIdType vtkPKdTree::GetCellListsForProcessRegions(int processId,
          vtkIdList *inRegionCells, vtkIdList *onBoundaryCells)
{
  return this->GetCellListsForProcessRegions(processId,
            this->GetDataSet(0), inRegionCells, onBoundaryCells);
}
vtkIdType vtkPKdTree::GetCellListsForProcessRegions(int processId,
          vtkDataSet *set,
          vtkIdList *inRegionCells, vtkIdList *onBoundaryCells)
{
  vtkIdType totalCells = 0;

  if ( (inRegionCells == NULL) && (onBoundaryCells == NULL))
    {
    return totalCells;
    }

  // Get the list of regions owned by this process

  vtkIntArray *regions = vtkIntArray::New();
    
  int nregions = this->GetRegionAssignmentList(processId, regions);
    
  if (nregions == 0){
    if (inRegionCells)
      {
      inRegionCells->Initialize();
      }
    if (onBoundaryCells)
      {
      onBoundaryCells->Initialize();
      }
    regions->Delete();
    return totalCells;
  }

  totalCells = this->GetCellLists(regions, set, inRegionCells, onBoundaryCells);

  regions->Delete();

  return totalCells;
}
void vtkPKdTree::PrintTiming(ostream& os, vtkIndent indent)
{
  os << indent << "Total cells in distributed data: " << this->TotalNumCells << endl;

  if (this->NumProcesses)
    {
    os << indent << "Average cells per processor: " ;
    os << this->TotalNumCells / this->NumProcesses << endl;
    }
  vtkTimerLog::DumpLogWithIndents(&os, (float)0.0);
}
void vtkPKdTree::PrintTables(ostream & os, vtkIndent indent)
{
  int nregions = this->GetNumberOfRegions();
  int nprocs =this->NumProcesses;
  int r,p,n;

  if (this->RegionAssignmentMap)
    {
    int *map = this->RegionAssignmentMap; 
    int *num = this->NumRegionsAssigned;
    int halfr = this->RegionAssignmentMapLength/2;
    int halfp = nprocs/2;

    os << indent << "Region assignments:" << endl;
    for (r=0; r < halfr; r++)
      {
      os << indent << "  region " << r << " to process " << map[r] ;
      os << "    region " << r+halfr << " to process " << map[r+halfr] ;
      os << endl;
      }
    for (p=0; p < halfp; p++)
      {
      os << indent << "  " << num[p] << " regions to process " << p ;
      os << "    " << num[p+halfp] << " regions to process " << p+ halfp ;
      os << endl;
      }
    if (nprocs > halfp*2)
      {
      os << indent << "  " << num[nprocs-1];
      os << " regions to process " << nprocs-1 << endl ;
      }
    }

  if (this->ProcessList)
    {
    os << indent << "Processes holding data for each region:" << endl;
    for (r=0; r<nregions; r++)
      {
      n = this->NumProcessesInRegion[r];

      os << indent << " region " << r << " (" << n << " processes): ";

      for (p=0; p<n; p++)
        {
        if (p && (p%10==0)) os << endl << indent << "   "; 
        os << this->ProcessList[r][p] << " " ;
        }
      os << endl;
      }
    }
  if (this->RegionList)
    {
    os << indent << "Regions held by each process:" << endl;
    for (p=0; p<nprocs; p++)
      {
      n = this->NumRegionsInProcess[p];

      os << indent << " process " << p << " (" << n << " regions): ";

      for (r=0; r<n; r++)
        {
        if (r && (r%10==0)) os << endl << indent << "   "; 
        os << this->RegionList[p][r] << " " ;
        }
      os << endl;
      }
    }
  if (this->CellCountList)
    {
    os << indent << "Number of cells per process per region:" << endl;
    for (r=0; r<nregions; r++)
      {
      n = this->NumProcessesInRegion[r];

      os << indent << " region: " << r << "  ";
      for (p=0; p<n; p++)
        {
        if (p && (p%5==0)) os << endl << indent << "   "; 
        os << this->ProcessList[r][p] << " - ";
        os << this->CellCountList[r][p] << " cells, ";
        }
      os << endl;
      }
    }
}
char *vtkPKdTree::StrDupWithNew(const char *s)
{   
  char *newstr = NULL;
    
  if (s)
    {
    int len = strlen(s);
    if (len == 0)
      {
      newstr = new char [1];
      newstr[0] = '\0';
      }
    else
      {                            
      newstr = new char [len + 1];
      strcpy(newstr, s);
      }
    } 

  return newstr;
}

void vtkPKdTree::PrintSelf(ostream& os, vtkIndent indent)
{  
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RegionAssignment: " << this->RegionAssignment << endl;

  os << indent << "Controller: " << this->Controller << endl;
  os << indent << "SubGroup: " << this->SubGroup<< endl;
  os << indent << "NumProcesses: " << this->NumProcesses << endl;
  os << indent << "MyId: " << this->MyId << endl;

  os << indent << "RegionAssignmentMap: " << this->RegionAssignmentMap << endl;
  os << indent << "NumRegionsAssigned: " << this->NumRegionsAssigned << endl;
  os << indent << "NumProcessesInRegion: " << this->NumProcessesInRegion << endl;
  os << indent << "ProcessList: " << this->ProcessList << endl;
  os << indent << "NumRegionsInProcess: " << this->NumRegionsInProcess << endl;
  os << indent << "RegionList: " << this->RegionList << endl;
  os << indent << "CellCountList: " << this->CellCountList << endl;

  os << indent << "StartVal: " << this->StartVal << endl;
  os << indent << "EndVal: " << this->EndVal << endl;
  os << indent << "NumCells: " << this->NumCells << endl;
  os << indent << "TotalNumCells: " << this->TotalNumCells << endl;

  os << indent << "PtArray: " << this->PtArray << endl;
  os << indent << "PtArray2: " << this->PtArray2 << endl;
  os << indent << "CurrentPtArray: " << this->CurrentPtArray << endl;
  os << indent << "NextPtArray: " << this->NextPtArray << endl;
  os << indent << "SelectBuffer: " << this->SelectBuffer << endl;
}
