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
/*----------------------------------------------------------------------------
 Copyright (c) Sandia Corporation
 See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.
----------------------------------------------------------------------------*/

// .NAME vtkDistributedDataFilter
//
// .SECTION Description
//
// .SECTION See Also

#include "vtkToolkits.h"
#include "vtkDistributedDataFilter.h"
#include "vtkExtractCells.h"
#include "vtkMergeCells.h"
#include "vtkObjectFactory.h"
#include "vtkPKdTree.h"
#include "vtkUnstructuredGrid.h"
#include "vtkDataSetAttributes.h"
#include "vtkExtractUserDefinedPiece.h"
#include "vtkCellData.h"
#include "vtkCellArray.h"
#include "vtkPointData.h"
#include "vtkIntArray.h"
#include "vtkFloatArray.h"
#include "vtkShortArray.h"
#include "vtkLongArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkMultiProcessController.h"
#include "vtkSocketController.h"
#include "vtkDataSetWriter.h"
#include "vtkDataSetReader.h"
#include "vtkCharArray.h"
#include "vtkBoxClipDataSet.h"
#include "vtkClipDataSet.h"
#include "vtkBox.h"
#include "vtkPlanes.h"
#include "vtkIdList.h"
#include "vtkPointLocator.h"
#include "vtkPlane.h"
#include <vtkstd/set>
#include <vtkstd/map>

#ifdef VTK_USE_MPI
#include <vtkMPIController.h>
#endif

// Timing data ---------------------------------------------

#include <vtkTimerLog.h>

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

#define TIMER(s)                    \
  if (this->Timing)                 \
    {                               \
    char *s2 = makeEntry(s);        \
    if (this->TimerLog == NULL)            \
      {                                    \
      this->TimerLog = vtkTimerLog::New(); \
      }                                    \
    this->TimerLog->MarkStartEvent(s2); \
    }

#define TIMERDONE(s) \
  if (this->Timing){ char *s2 = makeEntry(s); this->TimerLog->MarkEndEvent(s2); }

// Timing data ---------------------------------------------

vtkCxxRevisionMacro(vtkDistributedDataFilter, "$Revision$")

vtkStandardNewMacro(vtkDistributedDataFilter)

#define TEMP_CELL_ID_NAME      "___D3___GlobalCellIds"
#define TEMP_INSIDE_BOX_FLAG   "___D3___WHERE"
#define TEMP_NODE_ID_NAME      "___D3___GlobalNodeIds"

vtkDistributedDataFilter::vtkDistributedDataFilter()
{
  this->Kdtree = NULL;

  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());

  this->Target = NULL;
  this->Source = NULL;

  this->NumConvexSubRegions = 0;
  this->ConvexSubRegionBounds = NULL;

  this->GhostLevel = 0;

  this->GlobalIdArrayName = NULL;

  this->RetainKdtree = 0;
  this->IncludeAllIntersectingCells = 0;
  this->ClipCells = 0;

  this->Timing = 0;
  this->TimerLog = NULL;

  this->UseMinimalMemory = 0;
}

vtkDistributedDataFilter::~vtkDistributedDataFilter()
{ 
  if (this->Kdtree)
    {
    this->Kdtree->Delete();
    this->Kdtree = NULL;
    }
  
  this->SetController(NULL);

  if (this->Target)
    {
    delete [] this->Target;
    this->Target= NULL;
    } 

  if (this->Source)
    {
    delete [] this->Source;
    this->Source= NULL;
    }

  if (this->ConvexSubRegionBounds)
    {
    delete [] this->ConvexSubRegionBounds;
    this->ConvexSubRegionBounds = NULL;
    } 
  
  if (this->GlobalIdArrayName) 
    {
    delete [] this->GlobalIdArrayName;
    }
  
  if (this->TimerLog)
    {
    this->TimerLog->Delete();
    this->TimerLog = 0;
    }
}

//-------------------------------------------------------------------------
// Help with the complex business of the global node ID array.  It may
// have been given to us by the user, we may have found it by looking 
// up common array names, or we may have created it ourselves.  We don't
// necessarily know the data type unless we created it ourselves.
//-------------------------------------------------------------------------

const char *vtkDistributedDataFilter::GetGlobalNodeIdArray(vtkDataSet *set)
{
  //------------------------------------------------
  // list common names for global node id arrays here
  //
  int nnames = 1;
  const char *arrayNames[1] = {
     "GlobalNodeId"  // vtkExodusReader name
     };
  //------------------------------------------------

  // ParaView does this... we need to fix it.
  if (this->GlobalIdArrayName && (!this->GlobalIdArrayName[0]))
    {
    delete [] this->GlobalIdArrayName;
    this->GlobalIdArrayName = NULL;
    }

  const char *gidArrayName = NULL;

  if (this->GlobalIdArrayName)
    {
    vtkDataArray *da = set->GetPointData()->GetArray(this->GlobalIdArrayName);

    if (da)
      {
      // The user gave us the name of the field containing global
      // node ids.

      gidArrayName = this->GlobalIdArrayName;
      }
    }

  if (!gidArrayName)
    {
    vtkDataArray *da = set->GetPointData()->GetArray(TEMP_NODE_ID_NAME);

    if (da)
      {
      // We created in parallel a field of global node ids.
      //

      gidArrayName = TEMP_NODE_ID_NAME;
      }
    }

  if (!gidArrayName)
    {
    // Maybe we can find a global node ID array

    for (int nameId=0; nameId < nnames; nameId++)
      {
      vtkDataArray *da = set->GetPointData()->GetArray(arrayNames[nameId]);

      if (da)
        {
        this->SetGlobalIdArrayName(arrayNames[nameId]);
        gidArrayName = arrayNames[nameId];
        break;
        }
      }
    }

  return gidArrayName;
}

int vtkDistributedDataFilter::GlobalNodeIdAccessGetId(int idx)
{
  if (this->GlobalIdArrayIdType)
    return (int)this->GlobalIdArrayIdType[idx];
  else if (this->GlobalIdArrayLong)
    return (int)this->GlobalIdArrayLong[idx];
  else if (this->GlobalIdArrayInt)
    return (int)this->GlobalIdArrayInt[idx];
  else if (this->GlobalIdArrayShort)
    return (int)this->GlobalIdArrayShort[idx];
  else if (this->GlobalIdArrayChar)
    return (int)this->GlobalIdArrayChar[idx];
  else
    return 0;
}
int vtkDistributedDataFilter::GlobalNodeIdAccessStart(vtkDataSet *set)
{
  this->GlobalIdArrayChar = NULL;
  this->GlobalIdArrayShort = NULL;
  this->GlobalIdArrayInt = NULL;
  this->GlobalIdArrayLong = NULL;
  this->GlobalIdArrayIdType = NULL;

  const char *arrayName = this->GetGlobalNodeIdArray(set);

  if (!arrayName)
    {
    return 0;
    }

  vtkDataArray *da = set->GetPointData()->GetArray(arrayName);

  int type = da->GetDataType();

  switch (type)
  {
    case VTK_ID_TYPE:

      this->GlobalIdArrayIdType = ((vtkIdTypeArray *)da)->GetPointer(0);
      break;

    case VTK_CHAR:
    case VTK_UNSIGNED_CHAR:

      this->GlobalIdArrayChar = ((vtkCharArray *)da)->GetPointer(0);
      break;

    case VTK_SHORT:
    case VTK_UNSIGNED_SHORT: 

      this->GlobalIdArrayShort = ((vtkShortArray *)da)->GetPointer(0);
      break;

    case VTK_INT: 
    case VTK_UNSIGNED_INT:

      this->GlobalIdArrayInt = ((vtkIntArray *)da)->GetPointer(0);
      break;

    case VTK_LONG:
    case VTK_UNSIGNED_LONG:

      this->GlobalIdArrayLong = ((vtkLongArray *)da)->GetPointer(0);
      break;

    default:
      return 0;
  }

  return 1;
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
vtkPKdTree *vtkDistributedDataFilter::GetKdtree()
{
  if (this->Kdtree == NULL)
    {
    this->Kdtree = vtkPKdTree::New();
    }

  return this->Kdtree;
}
unsigned long vtkDistributedDataFilter::GetMTime()
{
  unsigned long t1, t2;

  t1 = this->Superclass::GetMTime();
  if (this->Kdtree == NULL)
    {
    return t1;
    }
  t2 = this->Kdtree->GetMTime();
  if (t1 > t2)
    {
    return t1;
    }
  return t2;
}

void vtkDistributedDataFilter::SetController(vtkMultiProcessController *c)
{
  if (this->Kdtree)
    {
    this->Kdtree->SetController(c);
    }

  if ((c == NULL) || (c->GetNumberOfProcesses() == 0))
    {
    this->NumProcesses = 1;    
    this->MyId = 0;
    }

  if (this->Controller == c)
    {
    return;
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

  this->Controller = c;

  c->Register(this);
  this->NumProcesses = c->GetNumberOfProcesses();
  this->MyId    = c->GetLocalProcessId();
}

void vtkDistributedDataFilter::SetBoundaryMode(int mode)
{
  switch (mode)
    {
    case vtkDistributedDataFilter::ASSIGN_TO_ONE_REGION:
      this->AssignBoundaryCellsToOneRegionOn();
      break;
    case vtkDistributedDataFilter::ASSIGN_TO_ALL_INTERSECTING_REGIONS:
      this->AssignBoundaryCellsToAllIntersectingRegionsOn();
      break;
    case vtkDistributedDataFilter::SPLIT_BOUNDARY_CELLS:
      this->DivideBoundaryCellsOn();
      break;
    }
}

int vtkDistributedDataFilter::GetBoundaryMode()
{
  if (!this->IncludeAllIntersectingCells && !this->ClipCells)
    {
    return vtkDistributedDataFilter::ASSIGN_TO_ONE_REGION;
    }
  if (this->IncludeAllIntersectingCells && !this->ClipCells)
    {
    return vtkDistributedDataFilter::ASSIGN_TO_ALL_INTERSECTING_REGIONS;
    }
  if (this->IncludeAllIntersectingCells && this->ClipCells)
    {
    return vtkDistributedDataFilter::SPLIT_BOUNDARY_CELLS;
    }
  
  return -1;
}

void vtkDistributedDataFilter::AssignBoundaryCellsToOneRegionOn()
{
  this->SetAssignBoundaryCellsToOneRegion(1);
}
void vtkDistributedDataFilter::AssignBoundaryCellsToOneRegionOff()
{
  this->SetAssignBoundaryCellsToOneRegion(0);
}
void vtkDistributedDataFilter::SetAssignBoundaryCellsToOneRegion(int val)
{
  if (val)
    {
    this->IncludeAllIntersectingCells = 0;
    this->ClipCells = 0;
    }
}
void 
vtkDistributedDataFilter::AssignBoundaryCellsToAllIntersectingRegionsOn()
{
  this->SetAssignBoundaryCellsToAllIntersectingRegions(1);
}
void 
vtkDistributedDataFilter::AssignBoundaryCellsToAllIntersectingRegionsOff()
{
  this->SetAssignBoundaryCellsToAllIntersectingRegions(0);
}
void 
vtkDistributedDataFilter::SetAssignBoundaryCellsToAllIntersectingRegions(int val)
{
  if (val)
    {
    this->IncludeAllIntersectingCells = 1;
    this->ClipCells = 0;
    }
}
void vtkDistributedDataFilter::DivideBoundaryCellsOn()
{
  this->SetDivideBoundaryCells(1);
}
void vtkDistributedDataFilter::DivideBoundaryCellsOff()
{
  this->SetDivideBoundaryCells(0);
}
void vtkDistributedDataFilter::SetDivideBoundaryCells(int val)
{
  if (val)
    {
    this->IncludeAllIntersectingCells = 1;
    this->ClipCells = 1;
    }
}
//-------------------------------------------------------------------------
// Execute
//-------------------------------------------------------------------------

void vtkDistributedDataFilter::ComputeInputUpdateExtents( vtkDataObject *o)
{
  vtkDataSetToUnstructuredGridFilter::ComputeInputUpdateExtents(o);

  // Since this filter redistibutes data, ghost cells computed upstream
  // will not be valid.

  this->GetInput()->SetUpdateGhostLevel( 0);
}

void vtkDistributedDataFilter::ExecuteInformation()
{
  vtkDataSet* input = this->GetInput();
  vtkUnstructuredGrid* output = this->GetOutput();

  if (input && output)
    {
    output->CopyInformation(input);
    output->SetMaximumNumberOfPieces(-1);
    }
}

void vtkDistributedDataFilter::Execute()
{
  vtkDataSet *input  = this->GetInput();
  vtkUnstructuredGrid *output  = this->GetOutput();
  vtkDataSet* inputPlus = input;

  vtkDebugMacro(<< "vtkDistributedDataFilter::Execute()");

  if (this->NumProcesses > 1)
    {
    int aok = 0;

#ifdef VTK_USE_MPI

    // Class compiles without MPI, but multiprocess execution requires MPI.
  
    if (vtkMPIController::SafeDownCast(this->Controller)) aok = 1;
#endif
  
    if (!aok)
      {
      vtkErrorMacro(<< "vtkDistributedDataFilter multiprocess requires MPI");
      return;
      }
  
    vtkDataSet *shareInput = this->TestFixTooFewInputFiles();

    if (shareInput == NULL)
      {
      return;    // Fewer cells than processes - can't divide input
      }
    else if (shareInput != input)
      {
      inputPlus = shareInput;  // Fewer files than processes, we divided them
      }
    }
  else if (input->GetNumberOfCells() < 1)
    {
    vtkErrorMacro("Empty input");
    return;
    }

  this->GhostLevel = output->GetUpdateGhostLevel();

  if ( (this->NumProcesses == 1) && !this->RetainKdtree)
    {
    // Output is a new grid.  It is the input grid, with
    // duplicate points removed.  Duplicate points arise
    // when the input was read from a data set distributed
    // across more than one file.  

    this->SingleProcessExecute();

    return;
    }

  if (this->NumProcesses > 1)
    {
    int HasIdTypeArrays = this->CheckFieldArrayTypes(inputPlus);
  
    if (HasIdTypeArrays)
      {
      // We use vtkDataWriter to marshall portions of data sets to send to other
      // processes.  Unfortunately, this writer writes out vtkIdType arrays as
      // integer arrays.  Trying to combine that on receipt with your vtkIdType
      // array won't work.  We need to exit with an error while we think of the
      // best way to handle this situation.  In practice I would think data sets
      // read in from disk files would never have such a field, only data sets created
      // in VTK at run time would have such a field.
  
      vtkErrorMacro(<<
        "vtkDistributedDataFilter::Execute - Can't distribute vtkIdTypeArrays, sorry");
      if (inputPlus != input);
        {
        inputPlus->Delete();
        }
      return;
      }
    }

  if (this->Kdtree == NULL)
    {
    this->Kdtree = vtkPKdTree::New();
    }

  this->Kdtree->SetController(this->Controller);
  this->Kdtree->SetNumRegionsOrMore(this->NumProcesses);
  this->Kdtree->SetMinCells(2);

  // Stage (1) - use vtkPKdTree to...
  //   Create a load balanced spatial decomposition in parallel.
  //   Create tables telling us how many cells each process has for
  //    each spatial region.
  //   Create a table assigning regions to processes.
  //
  // Note k-d tree will only be re-built if input or parameters
  // have changed on any of the processing nodes.

  int regionAssignmentScheme = this->Kdtree->GetRegionAssignment();

  if (regionAssignmentScheme == vtkPKdTree::NoRegionAssignment)
    {
    this->Kdtree->AssignRegionsContiguous();
    }

  this->Kdtree->SetDataSet(inputPlus);

  this->Kdtree->ProcessCellCountDataOn();

  TIMER("Build K-d tree in parallel");

  this->Kdtree->BuildLocator();

  TIMERDONE("Build K-d tree in parallel");

  if (this->Kdtree->GetNumberOfRegions() == 0)
    {
    vtkErrorMacro("Unable to build k-d tree structure");
    if (inputPlus != input);
      {
      inputPlus->Delete();
      }
    this->Kdtree->Delete();
    this->Kdtree = NULL;
    return;
    }

  if (this->NumProcesses == 1)
    {
    this->SingleProcessExecute();
    return;
    }

  // Stage (2) - Redistribute data, so that each process gets a ugrid
  //   containing the cells in it's assigned spatial regions.  (Note
  //   that a side effect of merging the grids received from different
  //   processes is that the final grid has no duplicate points.)

  if (this->GhostLevel > 0)
    {
    // We'll need some temporary global cell IDs later on 
    // when we add ghost cells to our data set.  vtkPKdTree 
    // knows how many cells each process read in, so we'll 
    // use that to create cell IDs.

    TIMER("Create temporary global cell IDs");

    vtkIntArray *ids = this->AssignGlobalCellIds(inputPlus); 

    if (inputPlus == input)
      {
      inputPlus = input->NewInstance();
      inputPlus->ShallowCopy(input);
      }

    inputPlus->GetCellData()->AddArray(ids);

    ids->Delete();

    TIMERDONE("Create temporary global cell IDs");
    }


  TIMER("Redistribute data among processors");

  vtkUnstructuredGrid *finalGrid = this->MPIRedistribute(inputPlus);

  TIMERDONE("Redistribute data among processors");

  if (finalGrid == NULL)
    {
    vtkErrorMacro("Unable to redistribute data");
    return;
    }

  const char *nodeIdArrayName = this->GetGlobalNodeIdArray(finalGrid);

  if (!nodeIdArrayName &&            // we don't have global point IDs
      ((this->GhostLevel > 0) ||     // we need ids for ghost cell computation
        this->GlobalIdArrayName))    // user requested that we compute ids
    {
    // Create unique global point IDs across processes

    TIMER("Create global node ID array");
    int rc = this->AssignGlobalNodeIds(finalGrid);

    if (rc)
      {
      finalGrid->Delete();
      this->Kdtree->Delete();
      this->Kdtree = NULL;
      return;
      }
    TIMERDONE("Create global node ID array");
    }

  if (this->GhostLevel > 0)
    {
    // If ghost cells were requested, then obtain enough cells from
    // neighbors so we will have the required levels of ghost cells.

    // Create a search structure mapping global point IDs to local point IDs
  
    TIMER("Build id search structure");
  
    this->GlobalNodeIdAccessStart(finalGrid);
  
    vtkstd::map<int, int> globalToLocalMap;
    vtkIdType numPoints = finalGrid->GetNumberOfPoints();
  
    for (int localPtId = 0; localPtId < numPoints; localPtId++)
      {
      int gid = this->GlobalNodeIdAccessGetId(localPtId);
      globalToLocalMap.insert(vtkstd::pair<int, int>(gid, localPtId));
      }
  
    TIMERDONE("Build id search structure");

    TIMER("Add ghost cells");

    vtkUnstructuredGrid *expandedGrid= NULL;

    if (this->IncludeAllIntersectingCells)
      {
      expandedGrid = 
        this->AddGhostCellsDuplicateCellAssignment(finalGrid, &globalToLocalMap);
      }
    else
      {
      expandedGrid = 
        this->AddGhostCellsUniqueCellAssignment(finalGrid, &globalToLocalMap);
      }

    TIMERDONE("Add ghost cells");

    expandedGrid->GetCellData()->RemoveArray(TEMP_CELL_ID_NAME);

    if (this->GlobalIdArrayName == NULL)
      {
      expandedGrid->GetPointData()->RemoveArray(TEMP_NODE_ID_NAME);
      }

    finalGrid = expandedGrid;
    }

  // Possible Stage (3) - Clip cells to the spatial region boundaries

  if (this->ClipCells)
    {
    // The clipping process introduces new points, and interpolates
    // the point arrays.  Interpolating the global point id is
    // meaningless, so we remove that array.

    if (this->GlobalIdArrayName) 
      {
      finalGrid->GetPointData()->RemoveArray(this->GlobalIdArrayName);
      this->GlobalIdArrayName = NULL;
      }

    TIMER("Clip boundary cells to region");
    
    this->ClipCellsToSpatialRegion(finalGrid);

    TIMERDONE("Clip boundary cells to region");
    }

  this->GetOutput()->ShallowCopy(finalGrid);

  finalGrid->Delete();

  if (!this->RetainKdtree)
    {
    this->Kdtree->Delete();
    this->Kdtree = NULL;
    }
}
void vtkDistributedDataFilter::SingleProcessExecute()
{
  vtkDataSet *input               = this->GetInput();
  vtkUnstructuredGrid *output     = this->GetOutput();

  vtkDebugMacro(<< "vtkDistributedDataFilter::SingleProcessExecute()");

  // we run the input through vtkMergeCells which will remove
  // duplicate points

  vtkDataSet* tmp = input->NewInstance();
  tmp->ShallowCopy(input);

  float tolerance;

  if (this->Kdtree)
    {
    tolerance = (float)this->Kdtree->GetFudgeFactor();
    }
  else
    {
    tolerance = 0.0;
    }

  vtkUnstructuredGrid *clean = 
    vtkDistributedDataFilter::MergeGrids(&tmp, 1, DeleteYes,
        this->GetGlobalNodeIdArray(input), tolerance, NULL);

  output->ShallowCopy(clean);
  clean->Delete();

  if (this->GhostLevel > 0)
    {
    // Add the vtkGhostLevels arrays.  We have the whole
    // data set, so all cells are level 0.

    vtkDistributedDataFilter::AddConstantUnsignedCharPointArray(
                              output, "vtkGhostLevels", 0);
    vtkDistributedDataFilter::AddConstantUnsignedCharCellArray(
                              output, "vtkGhostLevels", 0);
    }
}
void vtkDistributedDataFilter::ComputeMyRegionBounds()
{
    vtkIntArray *myRegions = vtkIntArray::New();

    this->Kdtree->GetRegionAssignmentList(this->MyId, myRegions);

    this->NumConvexSubRegions =
      this->Kdtree->MinimalNumberOfConvexSubRegions(
        myRegions, &this->ConvexSubRegionBounds);

    myRegions->Delete();
}
int vtkDistributedDataFilter::CheckFieldArrayTypes(vtkDataSet *set)
{
  int i;

  // problem - vtkIdType arrays are written out as int arrays
  // when marshalled with vtkDataWriter.  This is a problem
  // when receive the array and try to merge it with our own,
  // which is a vtkIdType

  vtkPointData *pd = set->GetPointData();
  vtkCellData *cd = set->GetCellData();

  int npointArrays = pd->GetNumberOfArrays();

  for (i=0; i<npointArrays; i++)
    {
    int arrayType = pd->GetArray(i)->GetDataType();

    if (arrayType == VTK_ID_TYPE)
      {
      return 1;
      }
    }

  int ncellArrays = cd->GetNumberOfArrays();

  for (i=0; i<ncellArrays; i++)
    {
    int arrayType = cd->GetArray(i)->GetDataType();

    if (arrayType == VTK_ID_TYPE)
      {
      return 1;
      }
    }

  return 0;
}
//-------------------------------------------------------------------------
// Quickly spread input data around if there are more processes than
// input data sets.
//-------------------------------------------------------------------------
extern "C"
{
  int vtkDistributedDataFilterSortSize(const void *s1, const void *s2)
  {
  struct _procInfo{ int had; int procId; int has; } *a, *b;

    a = (struct _procInfo *)s1;
    b = (struct _procInfo *)s2;

    if (a->has < b->has)       return 1;
    else if (a->has == b->has) return 0;
    else                   return -1;
  }
}
vtkDataSet *vtkDistributedDataFilter::TestFixTooFewInputFiles()
{
  int i, proc;
  int me = this->MyId;
  int nprocs = this->NumProcesses;
  vtkDataSet *input = this->GetInput();

  int numMyCells = input->GetNumberOfCells();

  // Find out how many input cells each process has.

  vtkIntArray *inputSize = this->ExchangeCounts(numMyCells, 0x0001);
  int *sizes = inputSize->GetPointer(0);

  int *nodeType = new int [nprocs];
  const int Producer = 1;
  const int Consumer = 2;
  int numConsumers = 0;
  int numTotalCells = 0;

  for (proc = 0; proc < nprocs ; proc++)
    {
    numTotalCells += sizes[proc];
    if (sizes[proc] == 0)
      {
      numConsumers++;
      nodeType[proc] = Consumer;
      }
    else
      {
      nodeType[proc] = Producer;
      }
    }

  if (numConsumers == 0)
    {
    // Nothing to do.  Every process has input data.

    delete [] nodeType;
    inputSize->Delete();
    return input;
    }

  if (numTotalCells < nprocs)
    {
    vtkErrorMacro(<< "D3 - fewer cells than processes");
    delete [] nodeType;
    inputSize->Delete();
    return NULL;
    }    

  int cellsPerNode = numTotalCells / nprocs;

  vtkIdList **sendCells = new vtkIdList * [ nprocs ];
  memset(sendCells, 0, sizeof(vtkIdList *) * nprocs);

  if (numConsumers == nprocs - 1)
    {
    // Simple and common case.  
    // Only one process has data and divides it among the rest.

    inputSize->Delete();

    if (nodeType[me] == Producer)
      {
      int sizeLast = numTotalCells - ((nprocs-1) * cellsPerNode);
      vtkIdType cellId = 0;
  
      for (proc=0; proc<nprocs; proc++)
        {
        int ncells = ((proc == nprocs - 1) ? sizeLast :cellsPerNode);
        
        sendCells[proc] = vtkIdList::New();
        sendCells[proc]->SetNumberOfIds(ncells);
  
        for (i=0; i<ncells; i++)
          {
          sendCells[proc]->SetId(i, cellId++);
          }
        }
      }
    }
  else
    {
    // The processes with data send it to processes without data.
    // This is not the most balanced decomposition, and it is not the
    // fastest.  It is somewhere inbetween.

    int minCells = (int)(.8 * cellsPerNode);
  
    struct _procInfo{ int had; int procId; int has; };
  
    struct _procInfo *procInfo = new struct _procInfo [nprocs];
  
    for (proc = 0; proc < nprocs ; proc++)
      {
      procInfo[proc].had   = inputSize->GetValue(proc);
      procInfo[proc].procId = proc;
      procInfo[proc].has   = inputSize->GetValue(proc);
      }

    inputSize->Delete();
  
    qsort(procInfo, nprocs, sizeof(struct _procInfo), 
          vtkDistributedDataFilterSortSize);

    struct _procInfo *nextProducer = procInfo;
    struct _procInfo *nextConsumer = procInfo + (nprocs - 1);

    int numTransferCells = 0;

    int sanityCheck=0;
  
    while (sanityCheck++ < nprocs)
      {
      int c = nextConsumer->procId;

      if (nodeType[c] == Producer) break;
  
      int cGetMin = minCells - nextConsumer->has;
  
      if (cGetMin < 1)
        {
        nextConsumer--;
        continue;
        }
      int cGetMax = cellsPerNode - nextConsumer->has;
  
      int p = nextProducer->procId;

      int pSendMax = nextProducer->has - minCells;
  
      if (pSendMax < 1)
        {
        nextProducer++;
        continue;
        }
  
      int transferSize = (pSendMax < cGetMax) ? pSendMax : cGetMax;

      if (me == p)
        {
        vtkIdType startCellId = nextProducer->had - nextProducer->has;
        sendCells[c] = vtkIdList::New();
        sendCells[c]->SetNumberOfIds(transferSize);
        for (i=0; i<transferSize; i++)
          {
          sendCells[c]->SetId(i, startCellId++);
          }

        numTransferCells += transferSize;
        }
  
      nextProducer->has -= transferSize;
      nextConsumer->has += transferSize;
  
      continue;
      }

    delete [] procInfo;

    if (sanityCheck > nprocs)
      {
      vtkErrorMacro(<< "TestFixTooFewInputFiles error");
      for (i=0; i<nprocs; i++)
        {
        if (sendCells[i]) sendCells[i]->Delete();
        } 
      delete [] sendCells;
      delete [] nodeType;
      }

    if (nodeType[me] == Producer)
      {
      int keepCells = numMyCells - numTransferCells;
      vtkIdType startCellId = (vtkIdType)numTransferCells;
      sendCells[me] = vtkIdList::New();
      sendCells[me]->SetNumberOfIds(keepCells);
      for (i=0; i<keepCells; i++)
        {
        sendCells[me]->SetId(i, startCellId++);
        }
      }
    }


  vtkUnstructuredGrid *newGrid = this->ExchangeMergeSubGrids(
           sendCells, DeleteYes, input, DeleteNo, 
           DuplicateCellsNo, 0x0011);

  delete [] sendCells;
  delete [] nodeType;

  return newGrid;
}
//-------------------------------------------------------------------------
// Communication routines - two versions:
//   *Lean version use minimal memory
//   *Fast versions use more memory, but are much faster
//-------------------------------------------------------------------------

void vtkDistributedDataFilter::SetUpPairWiseExchange()
{
  int iam = this->MyId;
  int nprocs = this->NumProcesses;

  if (this->Target)
    {
    delete [] this->Target;
    this->Target = NULL;
    }

  if (this->Source)
    {
    delete [] this->Source;
    this->Source = NULL;
    }

  if (nprocs == 1) return;

  this->Target = new int [nprocs - 1];
  this->Source = new int [nprocs - 1];

  for (int i=1; i< nprocs; i++)
    {
    this->Target[i-1] = (iam + i) % nprocs;
    this->Source[i-1] = (iam + nprocs - i) % nprocs;
    }
}
void vtkDistributedDataFilter::FreeIntArrays(vtkIntArray **ar)
{
  for (int i=0; i<this->NumProcesses; i++)
    {
    if (ar[i]) ar[i]->Delete();
    }

  delete [] ar;
}
void vtkDistributedDataFilter::FreeIdLists(vtkIdList**lists, int nlists)
{
  for (int i=0; i<nlists; i++)
    {
    if (lists[i])
      {
       lists[i]->Delete();
       lists[i] = NULL; 
      }
    }
}
int vtkDistributedDataFilter::GetIdListSize(vtkIdList**lists, int nlists)
{
  int numCells = 0;

  for (int i=0; i<nlists; i++)
    {
    if (lists[i])
      {
      numCells += lists[i]->GetNumberOfIds();
      }
    }

  return numCells;
}
vtkUnstructuredGrid *
  vtkDistributedDataFilter::ExchangeMergeSubGrids(
               vtkIdList **cellIds, int deleteCellIds,
               vtkDataSet *myGrid, int deleteMyGrid, 
               int filterOutDuplicateCells, 
               int tag)
{
  int nprocs = this->NumProcesses;

  int *numLists = new int [nprocs];

  vtkIdList ***listOfLists = new vtkIdList ** [nprocs];

  for (int i=0; i<nprocs; i++)
    {
    if (cellIds[i] == NULL) numLists[i] = 0;
    else                    numLists[i] = 1;

    listOfLists[i] = &cellIds[i];
    }

  vtkUnstructuredGrid *grid = NULL; 

  if (this->UseMinimalMemory)
    {
    grid = ExchangeMergeSubGridsLean(listOfLists, numLists, deleteCellIds,
                          myGrid, deleteMyGrid, filterOutDuplicateCells, tag);
    }
  else
    {
    grid = ExchangeMergeSubGridsFast(listOfLists, numLists, deleteCellIds,
                          myGrid, deleteMyGrid, filterOutDuplicateCells, tag);
    }
 
  delete [] numLists;
  delete [] listOfLists;

  return grid;
}
vtkUnstructuredGrid *
  vtkDistributedDataFilter::ExchangeMergeSubGrids(
               vtkIdList ***cellIds, int *numLists, int deleteCellIds,
               vtkDataSet *myGrid, int deleteMyGrid, 
               int filterOutDuplicateCells, 
               int tag)
{
  vtkUnstructuredGrid *grid = NULL; 

  if (this->UseMinimalMemory)
    {
    grid = ExchangeMergeSubGridsLean(cellIds, numLists, deleteCellIds,
                          myGrid, deleteMyGrid, filterOutDuplicateCells, tag);
    }
  else
    {
    grid = ExchangeMergeSubGridsFast(cellIds, numLists, deleteCellIds,
                          myGrid, deleteMyGrid, filterOutDuplicateCells, tag);
    }
  return grid;
}
vtkIntArray *vtkDistributedDataFilter::ExchangeCounts(int myCount, int tag)
{
  vtkIntArray *ia;

  if (this->UseMinimalMemory)
    {
    ia = this->ExchangeCountsLean(myCount, tag); 
    }
  else
    {
    ia = this->ExchangeCountsFast(myCount, tag); 
    }
  return ia;
}
vtkFloatArray **vtkDistributedDataFilter::
  ExchangeFloatArrays(vtkFloatArray **myArray, int deleteSendArrays, int tag)
{
  vtkFloatArray **fa;

  if (this->UseMinimalMemory)
    {
    fa = this->ExchangeFloatArraysLean(myArray, deleteSendArrays, tag);
    }
  else
    {
    fa = this->ExchangeFloatArraysFast(myArray, deleteSendArrays, tag);
    }
  return fa;
}
vtkIntArray **vtkDistributedDataFilter::
  ExchangeIntArrays(vtkIntArray **myArray, int deleteSendArrays, int tag)
{
  vtkIntArray **ia;

  if (this->UseMinimalMemory)
    {
    ia = this->ExchangeIntArraysLean(myArray, deleteSendArrays, tag); 
    }
  else
    {
    ia = this->ExchangeIntArraysFast(myArray, deleteSendArrays, tag); 
    }
  return ia;
}
// ----------------------- Lean versions ----------------------------//
vtkIntArray *vtkDistributedDataFilter::ExchangeCountsLean(int myCount, int tag)
{
  vtkIntArray *countArray = NULL;

#ifdef VTK_USE_MPI
  int i; 
  int nprocs = this->NumProcesses;

  vtkMPICommunicator::Request req;
  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *counts = new int [nprocs];
  counts[this->MyId] = myCount;

  if (!this->Source)
    {
    this->SetUpPairWiseExchange();
    }

  for (i = 0; i < this->NumProcesses - 1; i++)
    {
    int source = this->Source[i];
    int target = this->Target[i];
    mpiContr->NoBlockReceive(counts + source, 1, source, tag, req);
    mpiContr->Send(&myCount, 1, target, tag);
    req.Wait();
    }

  countArray = vtkIntArray::New();
  countArray->SetArray(counts, nprocs, 0);

#else
  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeCounts requires MPI");
  (void)myCount;
  (void)tag;
#endif
    
  return countArray;
}
vtkFloatArray **
  vtkDistributedDataFilter::ExchangeFloatArraysLean(vtkFloatArray **myArray, 
                                              int deleteSendArrays, int tag)
{
  vtkFloatArray **remoteArrays = NULL;

#ifdef VTK_USE_MPI
  int i; 
  int nprocs = this->NumProcesses;
  int me = this->MyId;

  vtkMPICommunicator::Request req;
  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *recvSize = new int [nprocs];
  int *sendSize = new int [nprocs];

  if (!this->Source)
    {
    this->SetUpPairWiseExchange();
    }

  for (i= 0; i< nprocs; i++)
    {
    sendSize[i] = myArray[i] ? myArray[i]->GetNumberOfTuples() : 0;
    recvSize[i] = 0;
    }

  // Exchange sizes

  int nothers = nprocs - 1;

  for (i = 0; i < nothers; i++)
    {
    int source = this->Source[i];
    int target = this->Target[i];
    mpiContr->NoBlockReceive(recvSize + source, 1, source, tag, req);
    mpiContr->Send(sendSize + target, 1, target, tag);
    req.Wait();
    }

  // Exchange int arrays

  float **recvArrays = new float * [nprocs];
  memset(recvArrays, 0, sizeof(float *) * nprocs);

  if (sendSize[me] > 0)  // sent myself an array
    {
    recvSize[me] = sendSize[me];
    recvArrays[me] = new float [sendSize[me]];
    memcpy(recvArrays[me], myArray[me]->GetPointer(0), sendSize[me] * sizeof(float));
    }

  for (i = 0; i < nothers; i++)
    {
    int source = this->Source[i];
    int target = this->Target[i];
    recvArrays[source] = NULL;

    if (recvSize[source] > 0)
      {
      recvArrays[source] = new float [recvSize[source]];
      if (recvArrays[source] == NULL)
        {
        vtkErrorMacro(<<
          "vtkDistributedDataFilter::ExchangeIntArrays memory allocation");
        return NULL;
        }
      mpiContr->NoBlockReceive(recvArrays[source], recvSize[source], source, tag, req);
      }

    if (sendSize[target] > 0)
      {
      mpiContr->Send(myArray[target]->GetPointer(0), sendSize[target], target, tag);
      }

    if (myArray[target] && deleteSendArrays)
      {
      myArray[target]->Delete();
      }

    if (recvSize[source] > 0)
      {
      req.Wait();
      }
    }

  if (deleteSendArrays)
    {
    if (myArray[me]) myArray[me]->Delete();
    delete [] myArray;
    }

  delete [] sendSize;

  remoteArrays = new vtkFloatArray * [nprocs];

  for (i=0; i<nprocs; i++)
    {
    if (recvSize[i] > 0)
      {
      remoteArrays[i] = vtkFloatArray::New();
      remoteArrays[i]->SetArray(recvArrays[i], recvSize[i], 0);
      }
    else
      {
      remoteArrays[i] = NULL;
      }
    }
  
  delete [] recvArrays;
  delete [] recvSize;

#else
  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeFloatArrays requires MPI");
  (void)myArray;
  (void)deleteSendArrays;
  (void)tag;
#endif
    
  return remoteArrays;
}
vtkIntArray **
  vtkDistributedDataFilter::ExchangeIntArraysLean(vtkIntArray **myArray, 
                                              int deleteSendArrays, int tag)
{
  vtkIntArray **remoteArrays = NULL;

#ifdef VTK_USE_MPI
  int i; 
  int nprocs = this->NumProcesses;
  int me = this->MyId;

  vtkMPICommunicator::Request req;
  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *recvSize = new int [nprocs];
  int *sendSize = new int [nprocs];

  if (!this->Source)
    {
    this->SetUpPairWiseExchange();
    }

  for (i= 0; i< nprocs; i++)
    {
    sendSize[i] = myArray[i] ? myArray[i]->GetNumberOfTuples() : 0;
    recvSize[i] = 0;
    }

  // Exchange sizes

  int nothers = nprocs - 1;

  for (i = 0; i < nothers; i++)
    {
    int source = this->Source[i];
    int target = this->Target[i];
    mpiContr->NoBlockReceive(recvSize + source, 1, source, tag, req);
    mpiContr->Send(sendSize + target, 1, target, tag);
    req.Wait();
    }

  // Exchange int arrays

  int **recvArrays = new int * [nprocs];
  memset(recvArrays, 0, sizeof(int *) * nprocs);

  if (sendSize[me] > 0)  // sent myself an array
    {
    recvSize[me] = sendSize[me];
    recvArrays[me] = new int [sendSize[me]];
    memcpy(recvArrays[me], myArray[me]->GetPointer(0), sendSize[me] * sizeof(int));
    }

  for (i = 0; i < nothers; i++)
    {
    int source = this->Source[i];
    int target = this->Target[i];
    recvArrays[source] = NULL;

    if (recvSize[source] > 0)
      {
      recvArrays[source] = new int [recvSize[source]];
      if (recvArrays[source] == NULL)
        {
        vtkErrorMacro(<<
          "vtkDistributedDataFilter::ExchangeIntArrays memory allocation");
        return NULL;
        }
      mpiContr->NoBlockReceive(recvArrays[source], recvSize[source], source, tag, req);
      }

    if (sendSize[target] > 0)
      {
      mpiContr->Send(myArray[target]->GetPointer(0), sendSize[target], target, tag);
      }

    if (myArray[target] && deleteSendArrays)
      {
      myArray[target]->Delete();
      }

    if (recvSize[source] > 0)
      {
      req.Wait();
      }
    }

  if (deleteSendArrays)
    {
    if (myArray[me]) myArray[me]->Delete();
    delete [] myArray;
    }

  delete [] sendSize;

  remoteArrays = new vtkIntArray * [nprocs];

  for (i=0; i<nprocs; i++)
    {
    if (recvSize[i] > 0)
      {
      remoteArrays[i] = vtkIntArray::New();
      remoteArrays[i]->SetArray(recvArrays[i], recvSize[i], 0);
      }
    else
      {
      remoteArrays[i] = NULL;
      }
    }
  
  delete [] recvArrays;
  delete [] recvSize;

#else
  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeIntArrays requires MPI");
  (void)myArray;
  (void)deleteSendArrays;
  (void)tag;
#endif
    
  return remoteArrays;
}
vtkUnstructuredGrid *
  vtkDistributedDataFilter::ExchangeMergeSubGridsLean(
    vtkIdList ***cellIds, int *numLists, int deleteCellIds,
    vtkDataSet *myGrid, int deleteMyGrid, 
    int filterOutDuplicateCells,   // flag if different processes may send same cells
    int tag)
{
  vtkUnstructuredGrid *mergedGrid = NULL;
#ifdef VTK_USE_MPI
  int i;
  int packedGridSendSize=0, packedGridRecvSize=0;
  char *packedGridSend=NULL, *packedGridRecv=NULL;
  int recvBufSize=0;

  int numReceivedGrids = 0;

  int nprocs = this->NumProcesses;
  int iam = this->MyId;

  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);
  vtkMPICommunicator::Request req;

  vtkDataSet **grids = new vtkDataSet * [nprocs];

  TIMER("Extract/exchange");
  if (numLists[iam] > 0)
    {
    // I was extracting/packing/sending/unpacking ugrids of zero cells,
    // and this caused corrupted data structures.  I don't know why, but
    // I am now being careful not to do that.
    int numCells = 
      vtkDistributedDataFilter::GetIdListSize(cellIds[iam], numLists[iam]);

    if (numCells > 0)
      {
      grids[numReceivedGrids++] = 
        vtkDistributedDataFilter::ExtractCells(cellIds[iam], numLists[iam],
                                        deleteCellIds, myGrid);
      }
    else if (deleteCellIds)
      {
      vtkDistributedDataFilter::FreeIdLists(cellIds[iam], numLists[iam]);
      }
    }

  if (this->Source == NULL)
    {
    this->SetUpPairWiseExchange();
    }

  int nothers = nprocs - 1;

  for (i=0; i<nothers; i++)
    {
    int target = this->Target[i];
    int source = this->Source[i];

    packedGridSendSize = 0;

    if (cellIds[target] && (numLists[target] > 0))
      {
      int numCells = vtkDistributedDataFilter::GetIdListSize(
                      cellIds[target], numLists[target]);

      if (numCells > 0)
        {
        vtkUnstructuredGrid *sendGrid = 
          vtkDistributedDataFilter::ExtractCells(cellIds[target], numLists[target], 
                                               deleteCellIds, myGrid);

        packedGridSend = this->MarshallDataSet(sendGrid, packedGridSendSize);
        sendGrid->Delete();
        }
      else if (deleteCellIds)
        {
        vtkDistributedDataFilter::FreeIdLists(cellIds[target], numLists[target]);
        }
      }

    // exchange size of packed grids

    mpiContr->NoBlockReceive(&packedGridRecvSize, 1, source, tag, req);
    mpiContr->Send(&packedGridSendSize, 1, target, tag);
    req.Wait();

    if (packedGridRecvSize > recvBufSize)
      {
      if (packedGridRecv) delete [] packedGridRecv;
      packedGridRecv = new char [packedGridRecvSize];
      if (!packedGridRecv)
        {
        vtkErrorMacro(<< 
          "vtkDistributedDataFilter::ExchangeMergeSubGrids memory allocation");
        return NULL;
        }
      recvBufSize = packedGridRecvSize;
      }
    
    if (packedGridRecvSize > 0)
      {
      mpiContr->NoBlockReceive(packedGridRecv, packedGridRecvSize, source, tag, req);
      }
      
    if (packedGridSendSize > 0)
      {
      mpiContr->Send(packedGridSend, packedGridSendSize, target, tag);
      delete [] packedGridSend;
      }

    if (packedGridRecvSize > 0)
      {
      req.Wait();

      grids[numReceivedGrids++] = 
        this->UnMarshallDataSet(packedGridRecv, packedGridRecvSize);
      }
    }

  if (recvBufSize > 0)
    {
    delete [] packedGridRecv;
    packedGridRecv = NULL;
    }

  TIMERDONE("Extract/exchange");

  // Merge received grids

  TIMER("Merge");

  const char *globalCellIds = NULL;
  const char *globalNodeIdArrayName = this->GetGlobalNodeIdArray(myGrid);

  if (deleteMyGrid)
    {
    myGrid->Delete();
    }

  if (filterOutDuplicateCells)
    {
    globalCellIds = TEMP_CELL_ID_NAME;
    }

  // this call will merge the grids and then delete them

  float tolerance = 0.0;

  if (this->Kdtree)
    {
    tolerance = (float)this->Kdtree->GetFudgeFactor();
    }

  mergedGrid = 
    vtkDistributedDataFilter::MergeGrids(grids, numReceivedGrids, DeleteYes,
                     globalNodeIdArrayName, tolerance,
                     globalCellIds);

  delete [] grids;
  TIMERDONE("Merge");

#else
  (void)cellIds;       // This is just here for successful compilation,
  (void)numLists;      // it will never execute.  If !VTK_USE_MPI, we
  (void)deleteCellIds; // never get this far.
  (void)myGrid;
  (void)deleteMyGrid;
  (void)filterOutDuplicateCells;
  (void)tag;

  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeMergeSubGrids requires MPI");
#endif

  return mergedGrid;
}
// ----------------------- Fast versions ----------------------------//
vtkIntArray *vtkDistributedDataFilter::ExchangeCountsFast(int myCount, int tag)
{
  vtkIntArray *countArray = NULL;

#ifdef VTK_USE_MPI
  int i; 
  int nprocs = this->NumProcesses;
  int me = this->MyId;

  vtkMPICommunicator::Request *req = new vtkMPICommunicator::Request [nprocs];
  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *counts = new int [nprocs];
  counts[me] = myCount;

  for (i = 0; i < nprocs; i++)
    {
    if (i  == me) continue;
    mpiContr->NoBlockReceive(counts + i, 1, i, tag, req[i]);
    }

  mpiContr->Barrier();

  for (i = 0; i < nprocs; i++)
    {
    if (i  == me) continue;
    mpiContr->Send(&myCount, 1, i, tag);
    }

  countArray = vtkIntArray::New();
  countArray->SetArray(counts, nprocs, 0);

  for (i = 0; i < nprocs; i++)
    {
    if (i  == me) continue;
    req[i].Wait();
    }

  delete [] req;

#else
  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeCounts requires MPI");
  (void)myCount;
  (void)tag;
#endif
    
  return countArray;
}
vtkFloatArray **
  vtkDistributedDataFilter::ExchangeFloatArraysFast(vtkFloatArray **myArray, 
                                              int deleteSendArrays, int tag)
{
  vtkFloatArray **fa = NULL;
#ifdef VTK_USE_MPI
  int proc;
  int nprocs = this->NumProcesses;
  int iam = this->MyId;

  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *sendSize = new int [nprocs];
  int *recvSize = new int [nprocs];

  for (proc=0; proc < nprocs; proc++)
    {
    recvSize[proc] = sendSize[proc] = 0;

    if (proc == iam) continue;

    if (myArray[proc])
      {
      sendSize[proc] = myArray[proc]->GetNumberOfTuples();
      }
    }

  // Exchange sizes of arrays to send and receive

  vtkMPICommunicator::Request *reqBuf = new vtkMPICommunicator::Request [nprocs];
    
  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->NoBlockReceive(recvSize + proc, 1, proc, tag, reqBuf[proc]);
    }

  mpiContr->Barrier();

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->Send(sendSize + proc, 1, proc, tag);
    }

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    reqBuf[proc].Wait();
    }

  // Allocate buffers and post receives

  float **recvBufs = new float * [nprocs];

  for (proc=0; proc < nprocs; proc++)
    {
    if (recvSize[proc] > 0)
      {
      recvBufs[proc] = new float [recvSize[proc]];
      mpiContr->NoBlockReceive(recvBufs[proc], recvSize[proc], proc, tag, reqBuf[proc]);
      }
    else
      {
      recvBufs[proc] = NULL;
      }
    }

  mpiContr->Barrier();

  // Send all arrays

  for (proc=0; proc < nprocs; proc++)
    {
    if (sendSize[proc] > 0)
      {
      mpiContr->Send(myArray[proc]->GetPointer(0), sendSize[proc], proc, tag);
      }
    }
  delete [] sendSize;

  // If I want to send an array to myself, place it in output now

  if (myArray[iam])
    {
    recvSize[iam] = myArray[iam]->GetNumberOfTuples();
    if (recvSize[iam] > 0)
      {
      recvBufs[iam] = new float [recvSize[iam]];
      memcpy(recvBufs[iam], myArray[iam]->GetPointer(0), recvSize[iam] * sizeof(float));
      }
    }

  if (deleteSendArrays)
    {
    for (proc=0; proc < nprocs; proc++)
      {
      if (myArray[proc])
        {
        myArray[proc]->Delete();
        }
      }
    delete [] myArray;
    }

  // Await incoming arrays

  fa = new vtkFloatArray * [nprocs];
  for (proc=0; proc < nprocs; proc++)
    {
    if (recvBufs[proc])
      {
      fa[proc] = vtkFloatArray::New();
      fa[proc]->SetArray(recvBufs[proc], recvSize[proc], 0);
      }
    else
      {
      fa[proc] = NULL;
      }
    }

  delete [] recvSize;

  for (proc=0; proc < nprocs; proc++)
    {
    if (proc == iam) continue;
    if (recvBufs[proc])
      {
      reqBuf[proc].Wait();
      }
    }

  delete [] reqBuf;
  delete [] recvBufs;

#else
  (void)myArray; 
  (void)deleteSendArrays;
  (void)tag;

  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeFloatArrays requires MPI");
#endif

  return fa;
}
vtkIntArray **
  vtkDistributedDataFilter::ExchangeIntArraysFast(vtkIntArray **myArray, 
                                              int deleteSendArrays, int tag)
{
  vtkIntArray **ia = NULL;
#ifdef VTK_USE_MPI
  int proc;
  int nprocs = this->NumProcesses;
  int iam = this->MyId;

  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  int *sendSize = new int [nprocs];
  int *recvSize = new int [nprocs];

  for (proc=0; proc < nprocs; proc++)
    {
    recvSize[proc] = sendSize[proc] = 0;

    if (proc == iam) continue;

    if (myArray[proc])
      {
      sendSize[proc] = myArray[proc]->GetNumberOfTuples();
      }
    }

  // Exchange sizes of arrays to send and receive

  vtkMPICommunicator::Request *reqBuf = new vtkMPICommunicator::Request [nprocs];
    
  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->NoBlockReceive(recvSize + proc, 1, proc, tag, reqBuf[proc]);
    }

  mpiContr->Barrier();

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->Send(sendSize + proc, 1, proc, tag);
    }

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    reqBuf[proc].Wait();
    }

  // Allocate buffers and post receives

  int **recvBufs = new int * [nprocs];

  for (proc=0; proc < nprocs; proc++)
    {
    if (recvSize[proc] > 0)
      {
      recvBufs[proc] = new int [recvSize[proc]];
      mpiContr->NoBlockReceive(recvBufs[proc], recvSize[proc], proc, tag, reqBuf[proc]);
      }
    else
      {
      recvBufs[proc] = NULL;
      }
    }

  mpiContr->Barrier();

  // Send all arrays

  for (proc=0; proc < nprocs; proc++)
    {
    if (sendSize[proc] > 0)
      {
      mpiContr->Send(myArray[proc]->GetPointer(0), sendSize[proc], proc, tag);
      }
    }
  delete [] sendSize;

  // If I want to send an array to myself, place it in output now

  if (myArray[iam])
    {
    recvSize[iam] = myArray[iam]->GetNumberOfTuples();
    if (recvSize[iam] > 0)
      {
      recvBufs[iam] = new int [recvSize[iam]];
      memcpy(recvBufs[iam], myArray[iam]->GetPointer(0), recvSize[iam] * sizeof(int));
      }
    }

  if (deleteSendArrays)
    {
    for (proc=0; proc < nprocs; proc++)
      {
      if (myArray[proc])
        {
        myArray[proc]->Delete();
        }
      }
    delete [] myArray;
    }

  // Await incoming arrays

  ia = new vtkIntArray * [nprocs];
  for (proc=0; proc < nprocs; proc++)
    {
    if (recvBufs[proc])
      {
      ia[proc] = vtkIntArray::New();
      ia[proc]->SetArray(recvBufs[proc], recvSize[proc], 0);
      }
    else
      {
      ia[proc] = NULL;
      }
    }

  delete [] recvSize;

  for (proc=0; proc < nprocs; proc++)
    {
    if (proc == iam) continue;
    if (recvBufs[proc])
      {
      reqBuf[proc].Wait();
      }
    }

  delete [] reqBuf;
  delete [] recvBufs;

#else
  (void)myArray; 
  (void)deleteSendArrays;
  (void)tag;

  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeIntArrays requires MPI");
#endif

  return ia;
}
vtkUnstructuredGrid *
  vtkDistributedDataFilter::ExchangeMergeSubGridsFast(
    vtkIdList ***cellIds, int *numLists, int deleteCellIds,
    vtkDataSet *myGrid, int deleteMyGrid, 
    int filterOutDuplicateCells,   // flag if different processes may send same cells
    int tag)
{
  vtkUnstructuredGrid *mergedGrid = NULL;
#ifdef VTK_USE_MPI
  int proc;
  int nprocs = this->NumProcesses;
  int iam = this->MyId;

  vtkMPIController *mpiContr = vtkMPIController::SafeDownCast(this->Controller);

  TIMER("Extract/exchange");

  vtkUnstructuredGrid **grids = new vtkUnstructuredGrid * [nprocs];
  char **sendBufs = new char * [nprocs];
  char **recvBufs = new char * [nprocs];
  int *sendSize = new int [nprocs];
  int *recvSize = new int [nprocs];

  // create & pack all sub grids

  for (proc=0; proc < nprocs; proc++)
    {
    recvSize[proc] = sendSize[proc] = 0;
    grids[proc] = NULL;
    sendBufs[proc] = recvBufs[proc] = NULL;

    if (numLists[proc] > 0)
      {
      int numCells =
        vtkDistributedDataFilter::GetIdListSize(cellIds[proc], numLists[proc]);
  
      if (numCells > 0)
        {
        grids[proc] =
          vtkDistributedDataFilter::ExtractCells(cellIds[proc], numLists[proc],
                                          deleteCellIds, myGrid);

        if (proc != iam)
          {
          sendBufs[proc] = this->MarshallDataSet(grids[proc], sendSize[proc]);
          grids[proc]->Delete();
          grids[proc] = NULL;
          }
        }
      else if (deleteCellIds)
        {
        vtkDistributedDataFilter::FreeIdLists(cellIds[proc], numLists[proc]);
        }
      }
    }

  if (deleteMyGrid)
    {
    myGrid->Delete();
    }

  // Exchange sizes of grids to send and receive

  vtkMPICommunicator::Request *reqBuf = new vtkMPICommunicator::Request [nprocs];
    
  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->NoBlockReceive(recvSize + proc, 1, proc, tag, reqBuf[proc]);
    }

  mpiContr->Barrier();

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    mpiContr->Send(sendSize + proc, 1, proc, tag);
    }

  for (proc=0; proc<nprocs; proc++)
    { 
    if (proc == iam) continue;
    reqBuf[proc].Wait();
    }

  // Allocate buffers and post receives

  int numReceives = 0;

  for (proc=0; proc < nprocs; proc++)
    {
    if (recvSize[proc] > 0)
      {
      recvBufs[proc] = new char [recvSize[proc]];
      mpiContr->NoBlockReceive(recvBufs[proc], recvSize[proc], proc, tag, reqBuf[proc]);
      numReceives++;
      }
    }

  mpiContr->Barrier();

  // Send all sub grids, then delete them

  for (proc=0; proc < nprocs; proc++)
    {
    if (sendSize[proc] > 0)
      {
      mpiContr->Send(sendBufs[proc], sendSize[proc], proc, tag);
      }
    }

  for (proc=0; proc < nprocs; proc++)
    {
    if (sendSize[proc] > 0) delete [] sendBufs[proc];
    }

  delete [] sendSize;
  delete [] sendBufs;

  // Await incoming sub grids, unpack them

  while (numReceives > 0)
    {
    for (proc=0; proc < nprocs; proc++)
      {
      if (recvBufs[proc] && (reqBuf[proc].Test() == 1))
        {
        grids[proc] = this->UnMarshallDataSet(recvBufs[proc], recvSize[proc]);
        delete [] recvBufs[proc];
        recvBufs[proc] = NULL;
        numReceives--;
        }
      }
    }

  delete [] reqBuf;
  delete [] recvBufs;
  delete [] recvSize;

  TIMERDONE("Extract/exchange");

  // Merge received grids

  TIMER("Merge");

  const char *globalCellIds = NULL;

  if (filterOutDuplicateCells)
    {
    globalCellIds = TEMP_CELL_ID_NAME;
    }

  float tolerance = 0.0;

  if (this->Kdtree)
    {
    tolerance = (float)this->Kdtree->GetFudgeFactor();
    }

  int numReceivedGrids = 0;

  vtkDataSet **ds = new vtkDataSet * [nprocs];
  
  for (proc=0; proc < nprocs; proc++)
    {
    if (grids[proc] != NULL)
      {
      ds[numReceivedGrids++] = static_cast<vtkDataSet *>(grids[proc]);
      }
    }

  delete [] grids;
  
  const char *globalNodeIdArrayName = this->GetGlobalNodeIdArray(ds[0]);

  // this call will merge the grids and then delete them
  mergedGrid = 
    vtkDistributedDataFilter::MergeGrids(ds, numReceivedGrids, DeleteYes,
                     globalNodeIdArrayName, tolerance,
                     globalCellIds);

  delete [] ds;
  TIMERDONE("Merge");

#else
  (void)cellIds;       // This is just here for successful compilation,
  (void)numLists;      // it will never execute.  If !VTK_USE_MPI, we
  (void)deleteCellIds; // never get this far.
  (void)myGrid;
  (void)deleteMyGrid;
  (void)filterOutDuplicateCells;
  (void)tag;

  vtkErrorMacro(<< "vtkDistributedDataFilter::ExchangeMergeSubGrids requires MPI");
#endif

  return mergedGrid;
}

vtkUnstructuredGrid *vtkDistributedDataFilter::MPIRedistribute(vtkDataSet *in)
{
  int proc;
  int nprocs = this->NumProcesses;

  TIMER("Create cell lists");

  // A cell belongs to a spatial region if it's centroid lies in that
  // region.  The kdtree object can create a list for each region of the
  // IDs of each cell I have read in that belong in that region.  If we
  // are building subgrids of all cells that intersect a region (a
  // superset of all cells that belong to a region) then the kdtree object
  // can build another set of lists of all cells that intersect each
  // region (but don't have their centroid in that region).
  
  if (this->IncludeAllIntersectingCells)
    {
    this->Kdtree->IncludeRegionBoundaryCellsOn();   // SLOW!!
    }
  
  this->Kdtree->CreateCellLists();  // required by GetCellIdsForProcess
    
  TIMERDONE("Create cell lists");

  TIMER("Get cell lists");

  vtkIdList ***procCellLists = new vtkIdList ** [nprocs];
  int *numLists = new int [nprocs];

  for (proc = 0; proc < this->NumProcesses; proc++)
    {
    procCellLists[proc] = this->GetCellIdsForProcess(proc, numLists + proc); 
    }

  TIMERDONE("Get cell lists");

  TIMER("Extract, exchange and merge cells");

  int deleteDataSet = DeleteNo;

  if (in != this->GetInput())
    {
    deleteDataSet = DeleteYes;
    }

  vtkUnstructuredGrid *myNewGrid = 
    this->ExchangeMergeSubGrids(procCellLists, numLists, DeleteNo,
       in, deleteDataSet, DuplicateCellsNo, 0x0012);

  TIMERDONE("Extract, exchange and merge cells");

  for (proc = 0; proc < nprocs; proc++)
    {
    delete [] procCellLists[proc];
    }

  delete [] procCellLists;
  delete [] numLists;

  TIMER("Add gl arrays");

  if (myNewGrid && (this->GhostLevel > 0))
    {
    vtkDistributedDataFilter::AddConstantUnsignedCharCellArray(
                            myNewGrid, "vtkGhostLevels", 0);
    vtkDistributedDataFilter::AddConstantUnsignedCharPointArray(
                            myNewGrid, "vtkGhostLevels", 0);
    }
  TIMERDONE("Add gl arrays");

  return myNewGrid;
}

char *vtkDistributedDataFilter::MarshallDataSet(vtkUnstructuredGrid *extractedGrid, int &len)
{
  // taken from vtkCommunicator::WriteDataSet

  vtkUnstructuredGrid *copy;
  vtkDataSetWriter *writer = vtkDataSetWriter::New();

  copy = extractedGrid->NewInstance();
  copy->ShallowCopy(extractedGrid);

  // There is a problem with binary files with no data.
  if (copy->GetNumberOfCells() > 0)
    {
    writer->SetFileTypeToBinary();
    }
  writer->WriteToOutputStringOn();
  writer->SetInput(copy);

  writer->Write();

  len = writer->GetOutputStringLength();

  char *packedFormat = writer->RegisterAndGetOutputString();

  writer->Delete();

  copy->Delete();

  return packedFormat;
}
vtkUnstructuredGrid *vtkDistributedDataFilter::UnMarshallDataSet(char *buf, int size)
{
  // taken from vtkCommunicator::ReadDataSet

  vtkDataSetReader *reader = vtkDataSetReader::New();

  reader->ReadFromInputStringOn();

  vtkCharArray* mystring = vtkCharArray::New();
  
  mystring->SetArray(buf, size, 1);

  reader->SetInputArray(mystring);
  mystring->Delete();

  vtkDataSet *output = reader->GetOutput();
  output->Update();

  vtkUnstructuredGrid *newGrid = vtkUnstructuredGrid::New();

  newGrid->ShallowCopy(output);

  reader->Delete();

  return newGrid;
}
vtkUnstructuredGrid 
  *vtkDistributedDataFilter::ExtractCells(vtkIdList *cells, int deleteCellLists,
                                          vtkDataSet *in)
{
  int deleteTempList = 0;

  if (cells == NULL)
    {
    // We'll get a zero cell unstructured grid which matches the input grid
    cells = vtkIdList::New();
    deleteTempList = 1;
    }

  vtkUnstructuredGrid *subGrid = 
    vtkDistributedDataFilter::ExtractCells(&cells, 1, deleteCellLists, in);

  if (deleteTempList)
    {
    cells->Delete();
    }

  return subGrid;
}
vtkUnstructuredGrid 
  *vtkDistributedDataFilter::ExtractCells(vtkIdList **cells, int nlists, 
                                          int deleteCellLists, vtkDataSet *in)
{
  vtkDataSet* tmpInput = in->NewInstance();
  tmpInput->ShallowCopy(in);

  vtkExtractCells *extCells = vtkExtractCells::New();

  extCells->SetInput(tmpInput);

  for (int i=0; i<nlists; i++)
    {
    if (cells[i])
      {
      extCells->AddCellList(cells[i]);
  
      if (deleteCellLists)
        {
        cells[i]->Delete();
        }
      }
    }

  extCells->Update();

  // If this process has no cells for these regions, a ugrid gets
  // created anyway with field array information

  vtkUnstructuredGrid *keepGrid = vtkUnstructuredGrid::New();
  keepGrid->ShallowCopy(extCells->GetOutput());

  extCells->Delete();

  tmpInput->Delete();

  return keepGrid;
}

// To save on storage, we return actual pointers into the vtkKdTree's lists
// of cell IDs.  So don't free the memory they are pointing to.  
// vtkKdTree::DeleteCellLists will delete them all when we're done.

vtkIdList **vtkDistributedDataFilter::GetCellIdsForProcess(int proc, int *nlists)
{
  *nlists = 0;

  vtkIntArray *regions = vtkIntArray::New();

  int nregions = this->Kdtree->GetRegionAssignmentList(proc, regions);

  if (nregions == 0)
    {
    return NULL;
    }

  *nlists = nregions;

  if (this->IncludeAllIntersectingCells)
    {
    *nlists *= 2;
    }

  vtkIdList **lists = new vtkIdList * [*nlists];

  int nextList = 0;

  for (int reg=0; reg < nregions; reg++)
    {
    lists[nextList++] = this->Kdtree->GetCellList(regions->GetValue(reg));

    if (this->IncludeAllIntersectingCells)
      {
      lists[nextList++] = this->Kdtree->GetBoundaryCellList(regions->GetValue(reg));
      }
    }

  regions->Delete();

  return lists;
}

//-------------------------------------------------------------------------
// Code related to clipping cells to the spatial region 
//-------------------------------------------------------------------------

static int insideBoxFunction(vtkIdType cellId, vtkUnstructuredGrid *grid, void *data)
{   
  char *arrayName = (char *)data;

  vtkDataArray *da= grid->GetCellData()->GetArray(arrayName);
  vtkUnsignedCharArray *inside = vtkUnsignedCharArray::SafeDownCast(da);

  unsigned char where = inside->GetValue(cellId);

  return where;   // 1 if cell is inside spatial region, 0 otherwise
}
void vtkDistributedDataFilter::AddConstantUnsignedCharPointArray(
  vtkUnstructuredGrid *grid, const char *arrayName, unsigned char val)
{
  vtkIdType npoints = grid->GetNumberOfPoints();

  unsigned char *vals = new unsigned char [npoints];

  memset(vals, val, npoints);

  vtkUnsignedCharArray *Array = vtkUnsignedCharArray::New();
  Array->SetName(arrayName);
  Array->SetArray(vals, npoints, 0);

  grid->GetPointData()->AddArray(Array);

  Array->Delete();
}
void vtkDistributedDataFilter::AddConstantUnsignedCharCellArray(
  vtkUnstructuredGrid *grid, const char *arrayName, unsigned char val)
{
  vtkIdType ncells = grid->GetNumberOfCells();

  unsigned char *vals = new unsigned char [ncells];

  memset(vals, val, ncells);

  vtkUnsignedCharArray *Array = vtkUnsignedCharArray::New();
  Array->SetName(arrayName);
  Array->SetArray(vals, ncells, 0);

  grid->GetCellData()->AddArray(Array);

  Array->Delete();
}

// this is here temporarily, until vtkBoxClipDataSet is fixed to
// be able to generate the clipped output

void vtkDistributedDataFilter::ClipWithVtkClipDataSet(
           vtkUnstructuredGrid *grid, double *bounds, 
           vtkUnstructuredGrid **outside, vtkUnstructuredGrid **inside)
{
  vtkUnstructuredGrid *in;
  vtkUnstructuredGrid *out ;

  vtkClipDataSet *clipped = vtkClipDataSet::New();

  vtkBox *box = vtkBox::New();
  box->SetBounds(bounds);

  clipped->SetClipFunction(box);
  box->Delete();
  clipped->SetValue(0.0);
  clipped->InsideOutOn();

  clipped->SetInput(grid);

  if (outside)
    {
    clipped->GenerateClippedOutputOn(); 
    }

  clipped->Update();

  if (outside)
    {
    out = clipped->GetClippedOutput();
    out->Register(this);
    *outside = out;
    }

  in = clipped->GetOutput();
  in->Register(this);
  *inside = in;


  clipped->Delete();
}

// In general, vtkBoxClipDataSet is much faster and makes fewer errors.

void vtkDistributedDataFilter::ClipWithBoxClipDataSet(
           vtkUnstructuredGrid *grid, double *bounds, 
           vtkUnstructuredGrid **outside, vtkUnstructuredGrid **inside)
{
  vtkUnstructuredGrid *in;
  vtkUnstructuredGrid *out ;

  vtkBoxClipDataSet *clipped = vtkBoxClipDataSet::New();

  clipped->SetBoxClip(bounds[0], bounds[1],
                      bounds[2], bounds[3], bounds[4], bounds[5]);

  clipped->SetInput(grid);

  if (outside)
    {
    clipped->GenerateClippedOutputOn();  
    }

  clipped->Update();

  if (outside)
    {
    out = clipped->GetClippedOutput();
    out->Register(this);
    *outside = out;
    }

  in = clipped->GetOutput();
  in->Register(this);
  *inside = in;

  clipped->Delete();
}

void vtkDistributedDataFilter::ClipCellsToSpatialRegion(vtkUnstructuredGrid *grid)
{
  if (this->ConvexSubRegionBounds == NULL)
    {
    this->ComputeMyRegionBounds();
    }

  if (this->NumConvexSubRegions > 1)
    {
    // here we would need to divide the grid into a separate grid for
    // each convex region, and then do the clipping

    vtkErrorMacro(<<
       "vtkDistributedDataFilter::ClipCellsToSpatialRegion - "
       "assigned regions do not form a single convex region");

    return ;
    }

  double *bounds = this->ConvexSubRegionBounds;

  if (this->GhostLevel > 0)
    {
    // We need cells outside the clip box as well.  

    vtkUnstructuredGrid *outside;
    vtkUnstructuredGrid *inside;

    TIMER("Clip");

#if 1
    this->ClipWithBoxClipDataSet(grid, bounds, &outside, &inside);
#else
    this->ClipWithVtkClipDataSet(grid, bounds, &outside, &inside);
#endif

    TIMERDONE("Clip");

    grid->Initialize();

    // Mark the outside cells with a 0, the inside cells with a 1.

    int arrayNameLen = strlen(TEMP_INSIDE_BOX_FLAG);
    char *arrayName = new char [arrayNameLen + 1];
    strcpy(arrayName, TEMP_INSIDE_BOX_FLAG);
    vtkDistributedDataFilter::AddConstantUnsignedCharCellArray(outside, arrayName, 0);
    vtkDistributedDataFilter::AddConstantUnsignedCharCellArray(inside, arrayName, 1);

    // Combine inside and outside into a single ugrid.

    TIMER("Merge inside and outside");

    vtkDataSet *grids[2];
    grids[0] = inside;
    grids[1] = outside;

    vtkUnstructuredGrid *combined = 
      vtkDistributedDataFilter::MergeGrids(grids, 2,  DeleteYes, NULL,
                         (float)this->Kdtree->GetFudgeFactor(), NULL);

    TIMERDONE("Merge inside and outside");

    // Extract the piece inside the box (level 0) and the requested
    // number of levels of ghost cells.

    TIMER("Extract desired cells");

    vtkExtractUserDefinedPiece *ep = vtkExtractUserDefinedPiece::New();

    ep->SetConstantData(arrayName, arrayNameLen + 1);
    ep->SetPieceFunction(insideBoxFunction);
    ep->CreateGhostCellsOn();

    ep->GetOutput()->SetUpdateGhostLevel(this->GhostLevel);
    ep->SetInput(combined);

    ep->Update();

    grid->ShallowCopy(ep->GetOutput());
    grid->GetCellData()->RemoveArray(arrayName);
    
    ep->Delete();
    combined->Delete();

    TIMERDONE("Extract desired cells");

    delete [] arrayName;
    }
  else
    {
    vtkUnstructuredGrid *inside;

#if 1
    this->ClipWithBoxClipDataSet(grid, bounds, NULL, &inside);
#else
    this->ClipWithVtkClipDataSet(grid, bounds, NULL, &inside);
#endif

    grid->ShallowCopy(inside);
    inside->Delete();
    }

  return;
}

//-------------------------------------------------------------------------
// Code related to assigning global node IDs and cell IDs
//-------------------------------------------------------------------------

int vtkDistributedDataFilter::AssignGlobalNodeIds(vtkUnstructuredGrid *grid)
{
  int nprocs = this->NumProcesses;
  int pid;
  int ptId;
  vtkIdType nGridPoints = grid->GetNumberOfPoints();

  int *numPointsOutside = new int [nprocs];
  memset(numPointsOutside, 0, sizeof(int) * nprocs);

  vtkIntArray *globalIds = vtkIntArray::New();
  globalIds->SetNumberOfValues(nGridPoints);

  if (this->GlobalIdArrayName)
    {
    globalIds->SetName(this->GlobalIdArrayName);
    }
  else
    {
    globalIds->SetName(TEMP_NODE_ID_NAME);
    }

  // 1. Count the points in grid which lie within my assigned spatial region

  int myNumPointsInside = 0;

  for (ptId = 0; ptId < nGridPoints; ptId++)
    {
    double *pt = grid->GetPoints()->GetPoint(ptId);

    if (this->InMySpatialRegion(pt[0], pt[1], pt[2]))
      {
      globalIds->SetValue(ptId, 0);  // flag it as mine
      myNumPointsInside++;
      }
    else
      {
      // Well, whose region is this point in?

      int regionId = this->Kdtree->GetRegionContainingPoint(pt[0],pt[1],pt[2]);

      pid = this->Kdtree->GetProcessAssignedToRegion(regionId);

      numPointsOutside[pid]++;
      
      pid += 1;
      pid *= -1;

      globalIds->SetValue(ptId, pid);  // a flag
      }
    }

  // 2. Gather and Broadcast this number of "Inside" points for each process.

  vtkIntArray *numPointsInside = this->ExchangeCounts(myNumPointsInside, 0x0013);

  // 3. Assign global Ids to the points inside my spatial region

  int firstId = 0;
  int numGlobalIdsSoFar = 0;

  for (pid = 0; pid < nprocs; pid++)
    {
    if (pid < this->MyId) firstId += numPointsInside->GetValue(pid);
    numGlobalIdsSoFar += numPointsInside->GetValue(pid);
    }

  numPointsInside->Delete();

  for (ptId = 0; ptId < nGridPoints; ptId++)
    {
    if (globalIds->GetValue(ptId) == 0)
      {
      globalIds->SetValue(ptId, firstId++);
      }
    }

  // -----------------------------------------------------------------
  // All processes have assigned global IDs to the points in their grid
  // which lie within their assigned spatial region.  
  // Now they have to get the IDs for the
  // points in their grid which lie outside their region, and which
  // are within the spatial region of another process.
  // -----------------------------------------------------------------

  // 4. For every other process, build a list of points I have
  // which are in the region of that process.  In practice, the
  // processes for which I need to request points IDs should be
  // a small subset of all the other processes.

  // question: if the vtkPointArray has type double, should we
  // send doubles instead of floats to insure we get the right
  // global ID back?

  vtkFloatArray **ptarrayOut = new vtkFloatArray * [nprocs];
  memset(ptarrayOut, 0, sizeof(vtkFloatArray *) * nprocs);

  vtkIntArray **localIds     = new vtkIntArray * [nprocs];
  memset(localIds, 0, sizeof(vtkIntArray *) * nprocs);

  int *next = new int [nprocs];
  int *next3 = new int [nprocs];

  for (ptId = 0; ptId < nGridPoints; ptId++)
    {
    pid = globalIds->GetValue(ptId);

    if (pid >= 0) continue;   // that's one of mine

    pid *= -1;
    pid -= 1;

    if (ptarrayOut[pid] == NULL)
      {
      int npoints = numPointsOutside[pid];

      ptarrayOut[pid] = vtkFloatArray::New();
      ptarrayOut[pid]->SetNumberOfValues(npoints * 3);
      
      localIds[pid] = vtkIntArray::New();
      localIds[pid]->SetNumberOfValues(npoints);
      
      next[pid] = 0;
      next3[pid] = 0;
      }

    localIds[pid]->SetValue(next[pid]++, ptId);

    double *dp = grid->GetPoints()->GetPoint(ptId);

    ptarrayOut[pid]->SetValue(next3[pid]++, (float)dp[0]);
    ptarrayOut[pid]->SetValue(next3[pid]++, (float)dp[1]);
    ptarrayOut[pid]->SetValue(next3[pid]++, (float)dp[2]);
    }

  delete [] numPointsOutside;
  delete [] next;
  delete [] next3;

  // 5. Do pairwise exchanges of the points we want global IDs for,
  //    and delete outgoing point arrays.

  vtkFloatArray **ptarrayIn = this->ExchangeFloatArrays(ptarrayOut, 
              DeleteYes, 0x0014);

  // 6. Find the global point IDs that have been requested of me,
  //    and delete incoming point arrays.  Count "missing points":
  //    the number of unique points I receive which are not in my 
  //    grid (this may happen if IncludeAllIntersectingCells is OFF).

  int myNumMissingPoints = 0;

  vtkIntArray **idarrayOut = 
    this->FindGlobalPointIds(ptarrayIn, globalIds, grid, myNumMissingPoints);

  vtkIntArray *missingCount = this->ExchangeCounts(myNumMissingPoints, 0x0015);

  if (this->IncludeAllIntersectingCells == 1)
    {
    // Make sure all points were found

    int aok = 1;
    for (pid=0; pid<nprocs; pid++)
      {
      if (missingCount->GetValue(pid) > 0)
        {
         vtkErrorMacro(<< 
          "vtkDistributedDataFilter::AssignGlobalNodeIds bad point");
        aok = 0;
        break;
        }
      }
    if (!aok)
      {
      this->FreeIntArrays(idarrayOut);
      this->FreeIntArrays(localIds);
      missingCount->Delete();
      globalIds->Delete();

      return 1;
      }
    }

  // 7. Do pairwise exchanges of the global point IDs, and delete the
  //    outgoing point ID arrays.

  vtkIntArray **idarrayIn = this->ExchangeIntArrays(idarrayOut, 
                    DeleteYes, 0x0016);

  // 8. It's possible (if IncludeAllIntersectingCells is OFF) that some
  //    processes had "missing points".  Process A has a point P in it's
  //    grid which lies in the spatial region of process B.  But P is not
  //    in process B's grid.  We need to assign global IDs to these points
  //    too.

  int *missingId = new int [nprocs];

  if (this->IncludeAllIntersectingCells == 0)
    {
    missingId[0] = numGlobalIdsSoFar;
  
    for (pid = 1; pid < nprocs; pid++)
      {
      int prev = pid - 1;
      missingId[pid] = missingId[prev] + missingCount->GetValue(prev);
      }
    }

  missingCount->Delete();

  // 9. Update my ugrid with these mutually agreed upon global point IDs

  for (pid = 0; pid < nprocs; pid++)
    {
    if (idarrayIn[pid] == NULL)
      {
      continue;
      }

    int count = idarrayIn[pid]->GetNumberOfTuples();
      
    for (ptId = 0; ptId < count; ptId++)
      {
      int myLocalId = localIds[pid]->GetValue(ptId);
      int yourGlobalId = idarrayIn[pid]->GetValue(ptId);

      if (yourGlobalId >= 0)
        {
        globalIds->SetValue(myLocalId, yourGlobalId);
        }
      else
        {
        int ptIdOffset = yourGlobalId * -1;
        ptIdOffset -= 1;

        globalIds->SetValue(myLocalId, missingId[pid] + ptIdOffset);
        }
      }
    localIds[pid]->Delete();
    idarrayIn[pid]->Delete();
    }

  delete [] localIds;
  delete [] idarrayIn;
  delete [] missingId;

  grid->GetPointData()->AddArray(globalIds);
  globalIds->Delete();

  return 0;
}

// If grids were distributed with IncludeAllIntersectingCells OFF, it's
// possible there are points in my spatial region that are not in my
// grid.  They need global Ids, so I will keep track of how many such unique
// points I receive from other processes, and will assign them temporary
// IDs.  They will get permanent IDs later on.

vtkIntArray **vtkDistributedDataFilter::FindGlobalPointIds(
     vtkFloatArray **ptarray, vtkIntArray *ids, vtkUnstructuredGrid *grid,
     int &numUniqueMissingPoints)
{
  vtkKdTree *kd = vtkKdTree::New();

  kd->BuildLocatorFromPoints(grid->GetPoints());

  int procId;
  int ptId, localId;

  int nprocs = this->NumProcesses;

  vtkIntArray **gids = new vtkIntArray * [nprocs];

  vtkPointLocator *pl = NULL;
  vtkPoints *missingPoints = NULL;

  if (this->IncludeAllIntersectingCells == 0)
    {
    if (this->ConvexSubRegionBounds == NULL)
      {
      this->ComputeMyRegionBounds();
      }
    pl = vtkPointLocator::New();
    pl->SetTolerance(this->Kdtree->GetFudgeFactor());
    missingPoints = vtkPoints::New();
    pl->InitPointInsertion(missingPoints, this->ConvexSubRegionBounds);
    }

  for (procId = 0; procId < nprocs; procId++)
    {
    if ((ptarray[procId] == NULL) || 
        (ptarray[procId]->GetNumberOfTuples() == 0))
      {
      gids[procId] = NULL;
      if (ptarray[procId]) ptarray[procId]->Delete();
      continue;
      }

    gids[procId] = vtkIntArray::New();

    int npoints = ptarray[procId]->GetNumberOfTuples() / 3;

    gids[procId]->SetNumberOfValues(npoints);
    int next = 0;

    float *pt = ptarray[procId]->GetPointer(0);

    for (ptId = 0; ptId < npoints; ptId++)
      {
      localId = kd->FindPoint(pt);

      if (localId >= 0)
        {
        gids[procId]->SetValue(next++, ids->GetValue(localId));  // global Id
        }
      else
        {
        // This point is not in my grid

        if (this->IncludeAllIntersectingCells)
          {
          // This is an error
          gids[procId]->SetValue(next++, -1);
          numUniqueMissingPoints++;
          }
        else
          {
          // Flag these with a negative point ID.  We'll assign
          // them real point IDs later.

          vtkIdType nextId;
          double dpt[3];
          dpt[0] = pt[0]; dpt[1] = pt[1]; dpt[2] = pt[2];
          pl->InsertUniquePoint(dpt, nextId); 

          nextId += 1;
          nextId *= -1;
          gids[procId]->SetValue(next++, nextId);
          }
        }
      pt += 3;
      }

    ptarray[procId]->Delete();
    }

  delete [] ptarray;

  kd->Delete();

  if (missingPoints)
    {
    numUniqueMissingPoints = missingPoints->GetNumberOfPoints();
    missingPoints->Delete();
    pl->Delete();
    }

  return gids;
}
vtkIntArray *vtkDistributedDataFilter::AssignGlobalCellIds(vtkDataSet *in)
{
  int i;
  int myNumCells = in->GetNumberOfCells();
  vtkIntArray *numCells = this->ExchangeCounts(myNumCells, 0x0017);

  vtkIntArray *globalCellIds = vtkIntArray::New();
  globalCellIds->SetNumberOfValues(myNumCells);

  int StartId = 0;

  for (i=0; i < this->MyId; i++)
    {
    StartId += numCells->GetValue(i);
    } 

  numCells->Delete();

  for (i=0; i<myNumCells; i++)
    {
    globalCellIds->SetValue(i, StartId++);
    }

  globalCellIds->SetName(TEMP_CELL_ID_NAME);

  return globalCellIds;
}

//-------------------------------------------------------------------------
// Code related to acquiring ghost cells
//-------------------------------------------------------------------------

int vtkDistributedDataFilter::InMySpatialRegion(float x, float y, float z)
{
  return this->InMySpatialRegion((double)x, (double)y, (double)z);
}
int vtkDistributedDataFilter::InMySpatialRegion(double x, double y, double z)
{
  if (this->ConvexSubRegionBounds == NULL)
    {
    this->ComputeMyRegionBounds();
    }

  double *box = this->ConvexSubRegionBounds;

  if (!box) return 0;

  // To avoid ambiguity, a point on a boundary is assigned to 
  // the region for which it is on the upper boundary.  Or
  // (in one dimension) the region between points A and B
  // contains all points p such that A < p <= B.

  if ( (x <= box[0]) || (x > box[1]) ||
       (y <= box[2]) || (y > box[3]) ||
       (z <= box[4]) || (z > box[5])   )
    {
      return 0;
    }

  return 1;
}
int vtkDistributedDataFilter::StrictlyInsideMyBounds(float x, float y, float z)
{
  return this->StrictlyInsideMyBounds((double)x, (double)y, (double)z);
}
int vtkDistributedDataFilter::StrictlyInsideMyBounds(double x, double y, double z)
{
  if (this->ConvexSubRegionBounds == NULL)
    {
    this->ComputeMyRegionBounds();
    }

  double *box = this->ConvexSubRegionBounds;

  if (!box) return 0;

  if ( (x <= box[0]) || (x >= box[1]) ||
       (y <= box[2]) || (y >= box[3]) ||
       (z <= box[4]) || (z >= box[5])   )
    {
      return 0;
    }

  return 1;
}

vtkIntArray **vtkDistributedDataFilter::MakeProcessLists(
                                    vtkIntArray **pointIds,
                                    vtkstd::multimap<int,int> *procs)
{
  // Build a list of pointId/processId pairs for each process that
  // sent me point IDs.  The process Ids are all those processes
  // that had the specified point in their ghost level zero grid.

  int nprocs = this->NumProcesses;

  vtkstd::multimap<int, int>::iterator mapIt;

  vtkIntArray **processList = new vtkIntArray * [nprocs];
  memset(processList, 0, sizeof (vtkIntArray *) * nprocs);

  for (int i=0; i<nprocs; i++)
    {
    if (pointIds[i] == NULL) continue;

    int size = pointIds[i]->GetNumberOfTuples();

    if (size > 0)
      {
      for (int j=0; j<size; )
        {
        // These are all the points in my spatial region
        // for which process "i" needs ghost cells.

        int gid = pointIds[i]->GetValue(j);
        int ncells = pointIds[i]->GetValue(j+1);

        mapIt = procs->find(gid);

        if (mapIt != procs->end())
          {
          while (mapIt->first == gid)
            {
            int processId = mapIt->second;
  
            if (processId != i)
              {
              // Process "i" needs to know that process
              // "processId" also has cells using this point
  
              if (processList[i] == NULL)
                {
                processList[i] = vtkIntArray::New();
                }
              processList[i]->InsertNextValue(gid);
              processList[i]->InsertNextValue(processId);
              }
            ++mapIt;
            }
          }
        j += (2 + ncells);
        }
      }
    }

  return processList;
}
vtkIntArray *vtkDistributedDataFilter::AddPointAndCells(
                        int gid, int localId, vtkUnstructuredGrid *grid, 
                        int *gidCells, vtkIntArray *ids)
{
  if (ids == NULL)
    {
    ids = vtkIntArray::New();
    }

  ids->InsertNextValue(gid);

  vtkIdList *cellList = vtkIdList::New();

  grid->GetPointCells(localId, cellList);

  vtkIdType numCells = cellList->GetNumberOfIds(); 

  ids->InsertNextValue((int)numCells);

  for (int j=0; j<numCells; j++)
    {
    int globalCellId = gidCells[cellList->GetId(j)];
    ids->InsertNextValue(globalCellId);
    }

  cellList->Delete();

  return ids;
}
vtkIntArray **vtkDistributedDataFilter::GetGhostPointIds(
                             int ghostLevel, vtkUnstructuredGrid *grid,
                             int AddCellsIAlreadyHave)
{
  int processId = -1;
  int regionId = -1;

  vtkPKdTree *kd = this->Kdtree;
  int nprocs = this->NumProcesses;
  int me = this->MyId;

  vtkPoints *pts = grid->GetPoints();
  vtkIdType numPoints = pts->GetNumberOfPoints();

  int rc = this->GlobalNodeIdAccessStart(grid);

  if (rc == 0)
    {
    return NULL;
    }

  vtkIntArray **ghostPtIds = new vtkIntArray * [nprocs];
  memset(ghostPtIds, 0, sizeof(vtkIntArray *) * nprocs);

  vtkDataArray *da = grid->GetCellData()->GetArray(TEMP_CELL_ID_NAME);
  vtkIntArray *ia= vtkIntArray::SafeDownCast(da);
  int *gidsCell = ia->GetPointer(0);

  da = grid->GetPointData()->GetArray("vtkGhostLevels");
  vtkUnsignedCharArray *uca = vtkUnsignedCharArray::SafeDownCast(da);
  unsigned char *levels = uca->GetPointer(0);

  unsigned char level = (unsigned char)(ghostLevel - 1);

  for (int i=0; i<numPoints; i++)
    {
    double *pt = pts->GetPoint(i);
    regionId = kd->GetRegionContainingPoint(pt[0], pt[1], pt[2]);
    processId = kd->GetProcessAssignedToRegion(regionId);

    if (ghostLevel == 1)
      {
      // I want all points that are outside my spatial region

      if (processId == me) continue;

      // Don't include points that are not part of any cell

      int used = vtkDistributedDataFilter::LocalPointIdIsUsed(grid, i);

      if (!used) continue;
      }
    else
      {
      // I want all points having the correct ghost level

      if (levels[i] != level) continue;
      }

    int gid = this->GlobalNodeIdAccessGetId(i);

    if (AddCellsIAlreadyHave)
      {
      // To speed up exchange of ghost cells and creation of
      // new ghost cell grid, we tell other
      // processes which cells we already have, so they don't
      // send them to us.

      ghostPtIds[processId] = 
        vtkDistributedDataFilter::AddPointAndCells(gid, i, grid, gidsCell,
                                       ghostPtIds[processId]);
      }
    else
      {
      if (ghostPtIds[processId] == NULL)
        {
        ghostPtIds[processId] = vtkIntArray::New();
        }
      ghostPtIds[processId]->InsertNextValue(gid);
      ghostPtIds[processId]->InsertNextValue(0);
      }
    }
  return ghostPtIds;
}
int vtkDistributedDataFilter::LocalPointIdIsUsed(
                              vtkUnstructuredGrid *grid, int ptId)
{
  int used = 1;

  int numPoints = grid->GetNumberOfPoints();

  if ((ptId < 0) || (ptId >= numPoints))
    {
    used = 0;
    }
  else
    {
    vtkIdType id = (vtkIdType)ptId;
    vtkIdList *cellList = vtkIdList::New();

    grid->GetPointCells(id, cellList);

    if (cellList->GetNumberOfIds() == 0)
      {
      used = 0;
      }

    cellList->Delete();
    }

  return used;
}
int vtkDistributedDataFilter::GlobalPointIdIsUsed(vtkUnstructuredGrid *grid, 
                              int ptId, vtkstd::map<int, int> *globalToLocal)
{
  int used = 1;

  vtkstd::map<int, int>::iterator mapIt;

  mapIt = globalToLocal->find(ptId);

  if (mapIt == globalToLocal->end())
    {
    used = 0;
    }
  else
    {
    int id = mapIt->second;

    used = vtkDistributedDataFilter::LocalPointIdIsUsed(grid, id);
    }

  return used;
}
int vtkDistributedDataFilter::FindId(vtkIntArray *ids, int gid, int startLoc)
{
  int gidLoc = -1;

  if (ids == NULL) return gidLoc;

  int numIds = ids->GetNumberOfTuples();

  while ((ids->GetValue(startLoc) != gid) && (startLoc < numIds))
    {
    int ncells = ids->GetValue(++startLoc);
    startLoc += (ncells + 1);
    }

  if (startLoc < numIds)
    {
    gidLoc = startLoc;
    }

  return gidLoc;
}

// We create an expanded grid with the required number of ghost
// cells.  This is for the case where IncludeAllIntersectingCells is OFF.
// This means that when the grid was redistributed, each cell was 
// uniquely assigned to one process, the process owning the spatial 
// region that the cell's centroid lies in.

vtkUnstructuredGrid *
vtkDistributedDataFilter::AddGhostCellsUniqueCellAssignment(
                                     vtkUnstructuredGrid *myGrid,
                                     vtkstd::map<int, int> *globalToLocalMap)
{
  int i,j,k;
  int ncells=0;
  int processId=0; 
  int gid=0; 
  int size=0;

  int nprocs = this->NumProcesses;
  int me = this->MyId;

  int gl = 1;

  // For each ghost level, processes request and send ghost cells

  vtkUnstructuredGrid *newGhostCellGrid = NULL;
  vtkIntArray **ghostPointIds = NULL;
  
  vtkstd::multimap<int, int> insidePointMap;
  vtkstd::multimap<int, int>::iterator mapIt;

  while (gl <= this->GhostLevel)
    {
    // For ghost level 1, create a list for each process (not 
    // including me) of all points I have in that process' 
    // assigned region.  We use this list for two purposes:
    // (1) to build a list on each process of all other processes
    // that have cells containing points in our region, (2)
    // these are some of the points that we need ghost cells for.
    //
    // For ghost level above 1, create a list for each process
    // (including me) of all my points in that process' assigned 
    // region for which I need ghost cells.
  
    if (gl == 1)
      {
      ghostPointIds = this->GetGhostPointIds(gl, myGrid, 0);
      }
    else
      {
      ghostPointIds = this->GetGhostPointIds(gl, newGhostCellGrid, 1);
      }
  
    // Exchange these lists.
  
    vtkIntArray **insideIds = 
      this->ExchangeIntArrays(ghostPointIds, DeleteNo, 
                              0x0018);

    if (gl == 1)
      {
      // For every point in my region that was sent to me by another process,
      // I now know the identity of all processes having cells containing
      // that point.  Begin by building a mapping from point IDs to the IDs
      // of processes that sent me that point.
    
      for (i=0; i<nprocs; i++)
        {
        if (insideIds[i] == NULL) continue;

        size = insideIds[i]->GetNumberOfTuples();
    
        if (size > 0)
          {
          for (j=0; j<size; j+=2)
            {
            // map global point id to process ids
            insidePointMap.insert(
              vtkstd::pair<int, int>(insideIds[i]->GetValue(j), i));
            }
          }
        }
      }

    // Build a list of pointId/processId pairs for each process that
    // sent me point IDs.  To process P, for every point ID sent to me
    // by P, I send the ID of every other process (not including myself
    // and P) that has cells in it's ghost level 0 grid which use
    // this point. 

    vtkIntArray **processListSent = MakeProcessLists(insideIds, &insidePointMap);

    // Exchange these new lists.

    vtkIntArray **processList = 
      this->ExchangeIntArrays(processListSent, DeleteYes,
                              0x0019);

    // I now know the identity of every process having cells containing
    // points I need ghost cells for.  Create a request to each process
    // for these cells.

    vtkIntArray **ghostCellsPlease = new vtkIntArray * [nprocs];
    for (i=0; i<nprocs; i++)
      {
      ghostCellsPlease[i] = vtkIntArray::New();
      ghostCellsPlease[i]->SetNumberOfComponents(1);
      }

    for (i=0; i<nprocs; i++)
      {
      if (i == me) continue;

      if (ghostPointIds[i])       // points I have in your spatial region,
        {                         // maybe you have cells that use them?

        for (j=0; j<ghostPointIds[i]->GetNumberOfTuples(); j++)
          {
          ghostCellsPlease[i]->InsertNextValue(ghostPointIds[i]->GetValue(j));
          }
        }
      if (processList[i])         // other processes you say that also have
        {                         // cells using those points
        int size = processList[i]->GetNumberOfTuples();
        int *array = processList[i]->GetPointer(0);
        int nextLoc = 0;
  
        for (j=0; j < size; j += 2)
          {
          gid = array[j];
          processId = array[j+1];
  
          ghostCellsPlease[processId]->InsertNextValue(gid);

          // add the list of cells I already have for this point

          int where = 
            vtkDistributedDataFilter::FindId(ghostPointIds[i], gid, nextLoc);

          if (where < 0)
            {
            // error
            cout << "error 1" << endl;
            }

          ncells = ghostPointIds[i]->GetValue(where + 1);

          ghostCellsPlease[processId]->InsertNextValue(ncells);

          for (k=0; k <ncells; k++)
            {
            int cellId = ghostPointIds[i]->GetValue(where + 2 + k);
            ghostCellsPlease[processId]->InsertNextValue(cellId);
            }

          nextLoc = where;
          }
        }
      if ((gl==1) && insideIds[i])   // points you have in my spatial region,
        {                            // which I may need ghost cells for
        for (j=0; j<insideIds[i]->GetNumberOfTuples();)
          {
          gid = insideIds[i]->GetValue(j);  
          int used = vtkDistributedDataFilter::GlobalPointIdIsUsed(
                                  myGrid, gid, globalToLocalMap);
          if (used)
            {
            ghostCellsPlease[i]->InsertNextValue(gid);
            ghostCellsPlease[i]->InsertNextValue(0);
            }

          ncells = insideIds[i]->GetValue(j+1);
          j += (ncells + 2);
          }
        }
      }

    if (gl > 1)
      {
      if (ghostPointIds[me])   // these points are actually inside my region
        {
        size = ghostPointIds[me]->GetNumberOfTuples();

        for (i=0; i<size;)
          {
          gid = ghostPointIds[me]->GetValue(i);
          ncells = ghostPointIds[me]->GetValue(i+1);

          mapIt = insidePointMap.find(gid);

          if (mapIt != insidePointMap.end())
            {
            while (mapIt->first == gid)
              { 
              processId = mapIt->second;
              ghostCellsPlease[processId]->InsertNextValue(gid);
              ghostCellsPlease[processId]->InsertNextValue(ncells);

              for (k=0; k<ncells; k++)
                {
                int cellId = ghostPointIds[me]->GetValue(i+1+k);
                ghostCellsPlease[processId]->InsertNextValue(cellId);
                }

              ++mapIt;
              }
            }
          i += (ncells + 2);
          }
        }
      }

    this->FreeIntArrays(ghostPointIds);
    this->FreeIntArrays(insideIds);
    this->FreeIntArrays(processList);

    // Exchange these ghost cell requests.

    vtkIntArray **ghostCellRequest 
      = this->ExchangeIntArrays(ghostCellsPlease, DeleteYes,
                                0x001a);
  
    // Build a list of cell IDs satisfying each request received. 
    // Delete request arrays.

    vtkIdList **sendCellList =
      this->BuildRequestedGrids(ghostCellRequest, myGrid, globalToLocalMap);
  
    // Build subgrids and exchange them

    vtkUnstructuredGrid *incomingGhostCells = this->ExchangeMergeSubGrids(
             sendCellList, DeleteYes, myGrid, DeleteNo, DuplicateCellsNo,
             0x001b);

    delete [] sendCellList;

    // Set ghost level of new cells, and merge into grid of other
    // ghost cells received.  

    newGhostCellGrid = this->SetMergeGhostGrid(newGhostCellGrid,
                              incomingGhostCells, gl, globalToLocalMap);
  
    gl++;
  }

  insidePointMap.erase(insidePointMap.begin(),insidePointMap.end());

  vtkUnstructuredGrid *newGrid = NULL;

  if (newGhostCellGrid && (newGhostCellGrid->GetNumberOfCells() > 0))
    {
    vtkDataSet *grids[2];

    grids[0] = myGrid;
    grids[1] = newGhostCellGrid;

    const char *nodeIds = this->GetGlobalNodeIdArray(myGrid);
   
    newGrid = 
      vtkDistributedDataFilter::MergeGrids(grids, 2, DeleteYes, nodeIds, 0, NULL);
    }
  else
    {
    newGrid = myGrid;
    }

  return newGrid;
}

// We create an expanded grid that contains the ghost cells we need.
// This is in the case where IncludeAllIntersectingCells is ON.  This
// is easier in some respects because we know if that if a point lies
// in a region owned by a particular process, that process has all
// cells which use that point.  So it is easy to find ghost cells.
// On the otherhand, because cells are not uniquely assigned to regions,
// we may get multiple processes sending us the same cell, so we
// need to filter these out.

vtkUnstructuredGrid *
vtkDistributedDataFilter::AddGhostCellsDuplicateCellAssignment(
                                     vtkUnstructuredGrid *myGrid,
                                     vtkstd::map<int, int> *globalToLocalMap)
{
  int i,j;

  int nprocs = this->NumProcesses;
  int me = this->MyId;

  int gl = 1;

  // For each ghost level, processes request and send ghost cells

  vtkUnstructuredGrid *newGhostCellGrid = NULL;
  vtkIntArray **ghostPointIds = NULL;
  vtkIntArray **extraGhostPointIds = NULL;

  vtkstd::map<int, int>::iterator mapIt;

  vtkPoints *pts = myGrid->GetPoints();

  while (gl <= this->GhostLevel)
    {
    // For ghost level 1, create a list for each process of points
    // in my grid which lie in that other process' spatial region.
    // This is normally all the points for which I need ghost cells, 
    // with one EXCEPTION.  If a cell is axis-aligned, and a face of 
    // the cell is on my upper boundary, then the vertices of this
    // face are in my spatial region, but I need their ghost cells.
    // I can detect this case when the process across the boundary
    // sends me a request for ghost cells of these points.
    //
    // For ghost level above 1, create a list for each process of
    // points in my ghost grid which are in that process' spatial
    // region and for which I need ghost cells.
  
    if (gl == 1)
      {
      ghostPointIds = this->GetGhostPointIds(gl, myGrid, 1);
      }
    else
      {
      ghostPointIds = this->GetGhostPointIds(gl, newGhostCellGrid, 1);
      }
  
    // Exchange these lists.
  
    vtkIntArray **insideIds = 
      this->ExchangeIntArrays(ghostPointIds, DeleteYes, 0x001c);

    // For ghost level 1, examine the points Ids I received from
    // other processes, to see if the exception described above
    // applies and I need ghost cells from them for those points.

    if (gl == 1)
      {
      vtkDataArray *da = myGrid->GetCellData()->GetArray(TEMP_CELL_ID_NAME);
      vtkIntArray *ia= vtkIntArray::SafeDownCast(da);
      int *gidsCell = ia->GetPointer(0);

      extraGhostPointIds = new vtkIntArray * [nprocs];

      for (i=0; i<nprocs; i++)
        {
        extraGhostPointIds[i] = NULL;

        if (i == me) continue;

        if (insideIds[i] == NULL) continue;
  
        int size = insideIds[i]->GetNumberOfTuples();
  
        for (j=0; j<size;)
          {
          int gid = insideIds[i]->GetValue(j);
  
          mapIt = globalToLocalMap->find(gid);
  
          if (mapIt == globalToLocalMap->end())
            {
            // error
            cout << " error 2 " << endl;
            }
          int localId = mapIt->second;

          double *pt = pts->GetPoint(localId);

          int interior = this->StrictlyInsideMyBounds(pt[0], pt[1], pt[2]);

          if (!interior)
            {
            extraGhostPointIds[i] = this->AddPointAndCells(gid, localId, 
                            myGrid, gidsCell, extraGhostPointIds[i]);
            }

          int ncells = insideIds[i]->GetValue(j+1);
          j += (ncells + 2);
          }
        }

      // Exchange these lists.
  
      vtkIntArray **extraInsideIds = 
        this->ExchangeIntArrays(extraGhostPointIds, DeleteYes, 0x001d);
  
      // Add the extra point ids to the previous list

      for (i=0; i<nprocs; i++)
        {
        if (i == me) continue;

        if (extraInsideIds[i])
          { 
          int size = extraInsideIds[i]->GetNumberOfTuples();

          if (insideIds[i] == NULL)
            {
            insideIds[i] = vtkIntArray::New();
            }

          for (j=0; j<size; j++)
            {
            insideIds[i]->InsertNextValue(extraInsideIds[i]->GetValue(j));
            }
          }
        }
        this->FreeIntArrays(extraInsideIds);
      }

    // Build a list of cell IDs satisfying each request received.

    vtkIdList **sendCellList =
      this->BuildRequestedGrids(insideIds, myGrid, globalToLocalMap);

    // Build subgrids and exchange them

    vtkUnstructuredGrid *incomingGhostCells = 
      this->ExchangeMergeSubGrids( sendCellList, DeleteYes, myGrid, DeleteNo, 
                                   DuplicateCellsYes, 0x001e);

    delete [] sendCellList;

    // Set ghost level of new cells, and merge into grid of other
    // ghost cells received.

    newGhostCellGrid = this->SetMergeGhostGrid(newGhostCellGrid,
                              incomingGhostCells, gl, globalToLocalMap);

    gl++;
  }

  vtkUnstructuredGrid *newGrid = NULL;

  if (newGhostCellGrid && (newGhostCellGrid->GetNumberOfCells() > 0))
    {
    vtkDataSet *grids[2];

    grids[0] = myGrid;
    grids[1] = newGhostCellGrid;

    const char *nodeIds = this->GetGlobalNodeIdArray(myGrid);

    newGrid = 
      vtkDistributedDataFilter::MergeGrids(grids, 2, DeleteYes, nodeIds, 0, NULL);
    }
  else
    {
    newGrid = myGrid;
    }

  return newGrid;
}

// For every process that sent me a list of point IDs, create a list
// of all the cells I have in my original grid containing those points.  
// We omit cells the remote process already has.

vtkIdList **vtkDistributedDataFilter::BuildRequestedGrids(
                        vtkIntArray **globalPtIds, 
                        vtkUnstructuredGrid *grid, 
                        vtkstd::map<int, int> *ptIdMap)
{
  int id, proc;
  int nprocs = this->NumProcesses;
  vtkIdType cellId;
  vtkIdType nelts;

  // for each process, create a list of the ids of cells I need
  // to send to it

  vtkstd::map<int, int>::iterator imap;

  vtkIdList *cellList = vtkIdList::New();

  vtkDataArray *da = grid->GetCellData()->GetArray(TEMP_CELL_ID_NAME);

  vtkIntArray *gidCells = vtkIntArray::SafeDownCast(da);

  vtkIdList **sendCells = new vtkIdList * [nprocs];

  for (proc = 0; proc < nprocs; proc++)
    {
    sendCells[proc] = vtkIdList::New();

    if (globalPtIds[proc] == NULL)
      {
      continue;
      }

    if ((nelts = globalPtIds[proc]->GetNumberOfTuples()) == 0)
      {
      globalPtIds[proc]->Delete();
      continue;
      }

    int *ptarray = globalPtIds[proc]->GetPointer(0);

    vtkstd::set<vtkIdType> subGridCellIds;

    int nYourCells = 0;

    for (id = 0; id < nelts; id += (nYourCells + 2))
      {
      int ptId = ptarray[id];
      nYourCells = ptarray[id+1];

      imap = ptIdMap->find(ptId);

      if (imap == ptIdMap->end())
        {
        continue; // I don't have this point
        }

      vtkIdType myPtId = (vtkIdType)imap->second;   // convert to my local point Id

      grid->GetPointCells(myPtId, cellList);

      vtkIdType nMyCells = cellList->GetNumberOfIds();

      if (nMyCells == 0)
        {
        continue;
        }

      if (nYourCells > 0)
        {
        // We don't send cells the remote process tells us it already
        // has.  This is much faster than removing duplicate cells on
        // the receive side.

        int *remoteCells = ptarray + id + 2;
        vtkDistributedDataFilter::RemoveRemoteCellsFromList(cellList, 
                                     gidCells, remoteCells, nYourCells);
        }

      vtkIdType nSendCells = cellList->GetNumberOfIds();

      if (nSendCells == 0)
        {
        continue;
        }

      for (cellId = 0; cellId < nSendCells; cellId++)
        {
        subGridCellIds.insert(cellList->GetId(cellId));
        }
      }

    globalPtIds[proc]->Delete();

    int numUniqueCellIds = subGridCellIds.size();

    if (numUniqueCellIds == 0)
      {
      continue;
      }

    sendCells[proc]->SetNumberOfIds(numUniqueCellIds);
    vtkIdType next = 0;

    vtkstd::set<vtkIdType>::iterator it;

    for (it = subGridCellIds.begin(); it != subGridCellIds.end(); ++it)
      {
      sendCells[proc]->SetId(next++, *it);
      }
    }
  
  delete [] globalPtIds;

  cellList->Delete();

  return sendCells;
}
void vtkDistributedDataFilter::RemoveRemoteCellsFromList(
     vtkIdList *cellList, vtkIntArray *gidCells, int *remoteCells, int nRemoteCells)
{
  vtkIdType id, nextId;
  int id2;
  int nLocalCells = cellList->GetNumberOfIds();

  // both lists should be very small, so we just do an n^2 lookup

  for (id = 0, nextId = 0; id < nLocalCells; id++)
    {
    vtkIdType localCellId  = cellList->GetId(id);
    int globalCellId = gidCells->GetValue(localCellId);

    int found = 0;

    for (id2 = 0; id2 < nRemoteCells; id2++)
      {
      if (remoteCells[id2] == globalCellId)
        {
        found = 1;
        break;
        } 
      }

    if (!found)
      {
      cellList->SetId(nextId++, localCellId);
      }
    }

  cellList->SetNumberOfIds(nextId);
}

// Set the ghost levels for the points and cells in the received cells.  
// Merge the new ghost cells into the supplied grid, and return the new grid.  
// Delete all grids except the new merged grid.

vtkUnstructuredGrid *vtkDistributedDataFilter::SetMergeGhostGrid(
                            vtkUnstructuredGrid *ghostCellGrid,
                            vtkUnstructuredGrid *incomingGhostCells,
                            int ghostLevel, vtkstd::map<int, int> *idMap)

{
  int i;

  if (incomingGhostCells->GetNumberOfCells() < 1)
    {
    return ghostCellGrid;
    }

  // Set the ghost level of all new cells, and set the ghost level of all
  // the points.  We know some points in the new grids actually have ghost
  // level one lower, because they were on the boundary of the previous
  // grid.  This is OK if ghostLevel is > 1.  When we merge, vtkMergeCells 
  // will skip these points because they are already in the previous grid.
  // But if ghostLevel is 1, those boundary points were in our original
  // grid, and we need to use the global ID map to determine if the
  // point ghost levels should be set to 0.  

  vtkDataArray *da = incomingGhostCells->GetCellData()->GetArray("vtkGhostLevels");
  vtkUnsignedCharArray *cellGL = vtkUnsignedCharArray::SafeDownCast(da);
      
  da  = incomingGhostCells->GetPointData()->GetArray("vtkGhostLevels");
  vtkUnsignedCharArray *ptGL = vtkUnsignedCharArray::SafeDownCast(da);

  unsigned char *ia = cellGL->GetPointer(0);

  for (i=0; i < incomingGhostCells->GetNumberOfCells(); i++)
    {
    ia[i] = (unsigned char)ghostLevel;
    }

  ia = ptGL->GetPointer(0);

  for (i=0; i < incomingGhostCells->GetNumberOfPoints(); i++)
    {
    ia[i] = (unsigned char)ghostLevel;
    }

  // now merge

  vtkUnstructuredGrid *mergedGrid = incomingGhostCells;

  if (ghostCellGrid && (ghostCellGrid->GetNumberOfCells() > 0))
    {
    vtkDataSet *sets[2];

    sets[0] = ghostCellGrid;     // both sets will be deleted by MergeGrids
    sets[1] = incomingGhostCells;

    const char *nodeIds = this->GetGlobalNodeIdArray(ghostCellGrid);

    mergedGrid = 
      vtkDistributedDataFilter::MergeGrids(sets, 2, DeleteYes, nodeIds, 0.0, NULL);
    }

  // If this is ghost level 1, mark any points from our original grid
  // as ghost level 0.

  if (ghostLevel == 1)
    {
    vtkDataArray *da = mergedGrid->GetPointData()->GetArray("vtkGhostLevels");
    vtkUnsignedCharArray *ptGL = vtkUnsignedCharArray::SafeDownCast(da);

    this->GlobalNodeIdAccessStart(mergedGrid);
    int npoints = mergedGrid->GetNumberOfPoints();

    vtkstd::map<int, int>::iterator imap;

    for (i=0; i < npoints; i++)
      {
      int globalId = this->GlobalNodeIdAccessGetId(i);

      imap = idMap->find(globalId);

      if (imap != idMap->end())
        {
        ptGL->SetValue(i,0);   // found among my ghost level 0 cells
        }
      }
    }

  return mergedGrid;
}
vtkUnstructuredGrid *vtkDistributedDataFilter::MergeGrids(
         vtkDataSet **sets, int nsets, int deleteDataSets,
         const char *globalNodeIdArrayName, float pointMergeTolerance, 
         const char *globalCellIdArrayName)
{
  if (nsets == 0)
    {
    return NULL;
    }

  vtkUnstructuredGrid *newGrid = vtkUnstructuredGrid::New();
  
  vtkMergeCells *mc = vtkMergeCells::New();
  mc->SetUnstructuredGrid(newGrid);
  
  mc->SetTotalNumberOfDataSets(nsets);

  int totalPoints = 0;
  int totalCells = 0;
  int i;

  for (i=0; i<nsets; i++)
    {
    totalPoints += sets[i]->GetNumberOfPoints();
    totalCells += sets[i]->GetNumberOfCells();
    }

  mc->SetTotalNumberOfPoints(totalPoints);
  mc->SetTotalNumberOfCells(totalCells);

  if (globalNodeIdArrayName)
    {
    mc->SetGlobalIdArrayName(globalNodeIdArrayName);
    }
  else
    {
    mc->SetPointMergeTolerance(pointMergeTolerance);
    }

  if (globalCellIdArrayName)
    {
    mc->SetGlobalCellIdArrayName(globalCellIdArrayName);
    }

  for (i=0; i<nsets; i++)
    {
    mc->MergeDataSet(sets[i]);

    if (deleteDataSets)
      {
      sets[i]->Delete();
      }
    }
  
  mc->Finish();
  mc->Delete();

  return newGrid;
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
void vtkDistributedDataFilter::PrintTiming(ostream& os, vtkIndent indent)
{
  (void)indent;
  vtkTimerLog::DumpLogWithIndents(&os, (float)0.0);
}
void vtkDistributedDataFilter::PrintSelf(ostream& os, vtkIndent indent)
{  
  this->Superclass::PrintSelf(os,indent);
  
  os << indent << "Kdtree: " << this->Kdtree << endl;
  os << indent << "Controller: " << this->Controller << endl;
  os << indent << "NumProcesses: " << this->NumProcesses << endl;
  os << indent << "MyId: " << this->MyId << endl;
  os << indent << "Target: " << this->Target << endl;
  os << indent << "Source: " << this->Source << endl;
  if (this->GlobalIdArrayName)
    {
    os << indent << "GlobalIdArrayName: " << this->GlobalIdArrayName << endl;
    }
  os << indent << "RetainKdtree: " << this->RetainKdtree << endl;
  os << indent << "IncludeAllIntersectingCells: " << this->IncludeAllIntersectingCells << endl;
  os << indent << "ClipCells: " << this->ClipCells << endl;

  os << indent << "Timing: " << this->Timing << endl;
  os << indent << "TimerLog: " << this->TimerLog << endl;
  os << indent << "UseMinimalMemory: " << this->UseMinimalMemory << endl;
}

