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
#include "vtkDataObject.h"

#include "vtkExtentTranslator.h"
#include "vtkFieldData.h"
#include "vtkObjectFactory.h"
#include "vtkSource.h"

vtkCxxRevisionMacro(vtkDataObject, "$Revision$");
vtkStandardNewMacro(vtkDataObject);

vtkCxxSetObjectMacro(vtkDataObject,FieldData,vtkFieldData);

// Initialize static member that controls global data release 
// after use by filter
static int vtkDataObjectGlobalReleaseDataFlag = 0;

//----------------------------------------------------------------------------
vtkDataObject::vtkDataObject()
{
  this->Source = NULL;

  // We have to assume that if a user is creating the data on their own,
  // then they will fill it with valid data.
  this->DataReleased = 0;

  this->ReleaseDataFlag = 0;
  this->FieldData = NULL;
  vtkFieldData *fd = vtkFieldData::New();
  this->SetFieldData(fd);
  fd->Delete();

  // The extent is uninitialized
  this->WholeExtent[0] = this->WholeExtent[2] = this->WholeExtent[4] = 0;
  this->WholeExtent[1] = this->WholeExtent[3] = this->WholeExtent[5] = -1;

  this->Extent[0] = this->Extent[2] = this->Extent[4] =  0;
  this->Extent[1] = this->Extent[3] = this->Extent[5] = -1;

  this->UpdateExtent[0] = this->UpdateExtent[2] = this->UpdateExtent[4] = 0;
  this->UpdateExtent[1] = this->UpdateExtent[3] = this->UpdateExtent[5] = 0;

  // If we used pieces instead of 3D extent, then assume this object was
  // created by the user and this is piece 0 of 1 pieces.
  this->Piece          =  -1;
  this->NumberOfPieces =  1;
  this->MaximumNumberOfPieces = -1;

  this->UpdatePiece          =   0;
  this->UpdateNumberOfPieces =   1;

  // ivars for ghost levels
  this->GhostLevel = 0;
  this->UpdateGhostLevel = 0;
  
  this->PipelineMTime = 0;

  // First update, the update extent will be set to the whole extent.
  this->UpdateExtentInitialized = 0;

  this->RequestExactExtent = 0;
  
  this->Locality = 0.0;

  this->ExtentTranslator = vtkExtentTranslator::New();
  this->ExtentTranslator->Register(this);
  this->ExtentTranslator->Delete();

  this->LastUpdateExtentWasOutsideOfTheExtent = 0;

  this->NumberOfConsumers = 0;
  this->Consumers = 0;
}

//----------------------------------------------------------------------------
vtkDataObject::~vtkDataObject()
{
  this->SetFieldData(NULL);

  this->SetExtentTranslator(NULL);
  delete [] this->Consumers;
}


//----------------------------------------------------------------------------
// Determine the modified time of this object
unsigned long int vtkDataObject::GetMTime()
{
  unsigned long result;

  result = vtkObject::GetMTime();
  if ( this->FieldData )
    {
    unsigned long mtime = this->FieldData->GetMTime();
    result = ( mtime > result ? mtime : result);
    }
  
  return result;
}

//----------------------------------------------------------------------------
void vtkDataObject::Initialize()
{
  this->FieldData->Initialize();

  this->Extent[0] = this->Extent[2] = this->Extent[4] = 0;
  this->Extent[1] = this->Extent[3] = this->Extent[5] = -1;
  this->Piece = -1;
  this->NumberOfPieces = 0;
  this->GhostLevel = 0;
  this->Modified();
}

void vtkDataObject::AddConsumer(vtkObject *c)
{
  // make sure it isn't already there
  if (this->IsConsumer(c))
    {
    return;
    }
  // add it to the list, reallocate memory
  vtkObject **tmp = this->Consumers;
  this->NumberOfConsumers++;
  this->Consumers = new vtkObject* [this->NumberOfConsumers];
  for (int i = 0; i < (this->NumberOfConsumers-1); i++)
    {
    this->Consumers[i] = tmp[i];
    }
  this->Consumers[this->NumberOfConsumers-1] = c;
  // free old memory
  delete [] tmp;
}

void vtkDataObject::RemoveConsumer(vtkObject *c)
{
  // make sure it is already there
  if (!this->IsConsumer(c))
    {
    return;
    }
  // remove it from the list, reallocate memory
  vtkObject **tmp = this->Consumers;
  this->NumberOfConsumers--;
  this->Consumers = new vtkObject* [this->NumberOfConsumers];
  int cnt = 0;
  int i;
  for (i = 0; i <= this->NumberOfConsumers; i++)
    {
    if (tmp[i] != c)
      {
      this->Consumers[cnt] = tmp[i];
      cnt++;
      }
    }
  // free old memory
  delete [] tmp;
}

int vtkDataObject::IsConsumer(vtkObject *c)
{
  int i;
  for (i = 0; i < this->NumberOfConsumers; i++)
    {
    if (this->Consumers[i] == c)
      {
      return 1;
      }
    }
  return 0;
}

vtkObject *vtkDataObject::GetConsumer(int i)
{
  if (i >= this->NumberOfConsumers)
    {
    return 0;
    }
  return this->Consumers[i];
}

//----------------------------------------------------------------------------
void vtkDataObject::SetGlobalReleaseDataFlag(int val)
{
  if (val == vtkDataObjectGlobalReleaseDataFlag)
    {
    return;
    }
  vtkDataObjectGlobalReleaseDataFlag = val;
}

//----------------------------------------------------------------------------

void vtkDataObject::DataHasBeenGenerated()
{
  this->DataReleased = 0;
  this->UpdateTime.Modified();

  // This is here so that the data can be easlily marked as up to date.
  // It is used specifically when the filter vtkQuadricClustering
  // is executed manually with the append methods. 
  this->Piece = this->UpdatePiece;
  this->NumberOfPieces = this->UpdateNumberOfPieces;
  this->GhostLevel = this->UpdateGhostLevel;
}

//----------------------------------------------------------------------------
int vtkDataObject::GetGlobalReleaseDataFlag()
{
  return vtkDataObjectGlobalReleaseDataFlag;
}

//----------------------------------------------------------------------------
void vtkDataObject::ReleaseData()
{
  this->Initialize();
  this->DataReleased = 1;
}

//----------------------------------------------------------------------------
int vtkDataObject::ShouldIReleaseData()
{
  if ( vtkDataObjectGlobalReleaseDataFlag || this->ReleaseDataFlag )
    {
    return 1;
    }
  else
    {
    return 0;
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::Update()
{
  this->UpdateInformation();
  this->PropagateUpdateExtent();
  this->TriggerAsynchronousUpdate();
  this->UpdateData();
}

//----------------------------------------------------------------------------
void vtkDataObject::UpdateInformation()
{
  if (this->Source)
    {
    this->Source->UpdateInformation();
    }
  else
    {
    // If we don't have a source, then let's make our whole
    // extent equal to our extent.
    memcpy( this->WholeExtent, this->Extent, 6*sizeof(int) );
    // We also need to set the PipeineMTime to our MTime.
    this->PipelineMTime = this->GetMTime();
    }
  
  // Now we should know what our whole extent is. If our update extent
  // was not set yet, then set it to the whole extent.
  if ( ! this->UpdateExtentInitialized)
    {
    this->SetUpdateExtentToWholeExtent();
    }

  this->LastUpdateExtentWasOutsideOfTheExtent = 0;
}

//----------------------------------------------------------------------------

void vtkDataObject::PropagateUpdateExtent()
{  
  // If we need to update due to PipelineMTime, or the fact that our
  // data was released, then propagate the update extent to the source 
  // if there is one.
  if ( this->UpdateTime < this->PipelineMTime || this->DataReleased ||
       this->UpdateExtentIsOutsideOfTheExtent())
    {
    if (this->Source)
      {
      this->Source->PropagateUpdateExtent(this);
      }
    }
  
  // Check that the update extent lies within the whole extent
  this->VerifyUpdateExtent();
}

//----------------------------------------------------------------------------

void vtkDataObject::TriggerAsynchronousUpdate()
{  
  if (this->MaximumNumberOfPieces > 0 &&
      this->UpdatePiece >= this->MaximumNumberOfPieces)
    {
    return;
    }

  // If we need to update due to PipelineMTime, or the fact that our
  // data was released, then propagate the trigger to the source
  // if there is one.
  if ( this->UpdateTime < this->PipelineMTime || this->DataReleased ||
       this->UpdateExtentIsOutsideOfTheExtent() ||
       this->LastUpdateExtentWasOutsideOfTheExtent)
    {
    if (this->Source)
      {
      this->Source->TriggerAsynchronousUpdate();
      }
    }

  this->LastUpdateExtentWasOutsideOfTheExtent =        
            this->UpdateExtentIsOutsideOfTheExtent();
}

//----------------------------------------------------------------------------

void vtkDataObject::UpdateData()
{  
  // This condition gives the default behavior if the user asks
  // for a piece that cannot be generated by the source.
  // Just ignore the request and return empty.
  if (this->MaximumNumberOfPieces > 0 &&
      this->UpdatePiece >= this->MaximumNumberOfPieces)
    {
    this->Initialize();
    this->Piece = this->UpdatePiece;
    this->NumberOfPieces = this->UpdateNumberOfPieces;
    this->GhostLevel = this->UpdateGhostLevel;
    return;
    }

  // If we need to update due to PipelineMTime, or the fact that our
  // data was released, then propagate the UpdateData to the source
  // if there is one.
  if ( this->UpdateTime < this->PipelineMTime || this->DataReleased ||
       this->UpdateExtentIsOutsideOfTheExtent())
    {
    if (this->Source)
      {
      this->Source->UpdateData(this);
      // This is now douplicated in the method "DataHasBeenGenerated"
      // It can probably be removed from this method.
      this->Piece = this->UpdatePiece;
      this->NumberOfPieces = this->UpdateNumberOfPieces;
      this->GhostLevel = this->UpdateGhostLevel;
      } 
    } 

  // Filters, that can't handle more data than they request, set this flag.
  if (this->RequestExactExtent)
    { // clip the data down to the UpdateExtent.
    this->Crop();
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdateExtent( int x1, int x2, 
                                     int y1, int y2, 
                                     int z1, int z2 )
{
  this->UpdateExtent[0] = x1;
  this->UpdateExtent[1] = x2;
  this->UpdateExtent[2] = y1;
  this->UpdateExtent[3] = y2;
  this->UpdateExtent[4] = z1;
  this->UpdateExtent[5] = z2;
  
  this->UpdateExtentInitialized = 1;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdateExtent( int ext[6] )
{
  memcpy( this->UpdateExtent, ext, 6*sizeof(int) );
  this->UpdateExtentInitialized = 1;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdatePiece( int piece )
{
  this->UpdatePiece = piece;
  this->UpdateExtentInitialized = 1;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdateNumberOfPieces( int num )
{
  this->UpdateNumberOfPieces = num;
  this->UpdateExtentInitialized = 1;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdateGhostLevel(int level)
{
  this->UpdateGhostLevel = level;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetRequestExactExtent( int flag )
{
  this->RequestExactExtent = flag;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetSource(vtkSource *arg)
{
  vtkDebugMacro( << this->GetClassName() << " (" 
                 << this << "): setting Source to " << arg ); 

  if (this->Source != arg) 
    {
    vtkSource *tmp = this->Source;
    this->Source = arg; 
    if (this->Source != NULL) 
      { 
      this->Source->Register(this); 
      } 
    if (tmp != NULL) 
      { 
      tmp->UnRegister(this); 
      }
    this->Modified(); 
    } 
}


//----------------------------------------------------------------------------
void vtkDataObject::UnRegister(vtkObjectBase *o)
{
  // detect the circular loop source <-> data
  // If we have two references and one of them is my data
  // and I am not being unregistered by my data, break the loop.
  if (this->ReferenceCount == 2 && this->Source != NULL &&
      o != this->Source && this->Source->InRegisterLoop(this))
    {
    this->SetSource(NULL);
    }
  
  this->vtkObject::UnRegister(o);
}

//----------------------------------------------------------------------------
unsigned long vtkDataObject::GetUpdateTime()
{
  return this->UpdateTime.GetMTime();
}

unsigned long vtkDataObject::GetEstimatedMemorySize()
{
  // This should be implemented in a subclass. If not, default to
  // estimating that no memory is used.
  return 0;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetUpdateExtentToWholeExtent()
{
  // I am setting the update extent to not initialized here so that 
  // the update extent will always be the whole extent even
  // it the whole extent changes.
  this->UpdateExtentInitialized = 0;    

  switch ( this->GetExtentType() )
    {
    // Our update extent will be the first piece of one piece (the whole thing)
    case VTK_PIECES_EXTENT:
      this->UpdateNumberOfPieces  = 1;
      this->UpdatePiece           = 0;
      break;

    // Our update extent will be the whole extent
    case VTK_3D_EXTENT:
      memcpy( this->UpdateExtent, this->WholeExtent, 6*sizeof(int) );
      break;

    // We should never have this case occur
    default:
      vtkErrorMacro( << "Internal error - invalid extent type!" );
      break;
    }
}

//----------------------------------------------------------------------------
int vtkDataObject::VerifyUpdateExtent()
{
  int retval = 1;

  switch ( this->GetExtentType() )
    {
    // Are we asking for more pieces than we can get?
    case VTK_PIECES_EXTENT:
      if ( 0 && this->UpdatePiece >= this->UpdateNumberOfPieces ||
        this->UpdatePiece < 0 )
        {
        retval = 0;
        }
      break;

    // Is our update extent within the whole extent?
    case VTK_3D_EXTENT:
      if (this->UpdateExtent[0] < this->WholeExtent[0] ||
          this->UpdateExtent[1] > this->WholeExtent[1] ||
          this->UpdateExtent[2] < this->WholeExtent[2] ||
          this->UpdateExtent[3] > this->WholeExtent[3] ||
          this->UpdateExtent[4] < this->WholeExtent[4] ||
          this->UpdateExtent[5] > this->WholeExtent[5] )
        { // Update extent outside whole extent.
        if (this->UpdateExtent[1] >= this->UpdateExtent[0] &&
            this->UpdateExtent[3] >= this->UpdateExtent[2] &&
            this->UpdateExtent[5] >= this->UpdateExtent[4])
          { // non empty update extent.
          vtkErrorMacro( << "Update extent does not lie within whole extent" );
          vtkErrorMacro( << "Update extent is: " <<
            this->UpdateExtent[0] << ", " <<
            this->UpdateExtent[1] << ", " <<
            this->UpdateExtent[2] << ", " <<
            this->UpdateExtent[3] << ", " <<
            this->UpdateExtent[4] << ", " <<
            this->UpdateExtent[5]);
          vtkErrorMacro( << "Whole extent is: " <<
            this->WholeExtent[0] << ", " <<
            this->WholeExtent[1] << ", " <<
            this->WholeExtent[2] << ", " <<
            this->WholeExtent[3] << ", " <<
            this->WholeExtent[4] << ", " <<
            this->WholeExtent[5]);
          }
        retval = 0;
        }
      break;

    // We should never have this case occur
    default:
      vtkErrorMacro( << "Internal error - invalid extent type!" );
      break;
    }

  return retval;
}

//----------------------------------------------------------------------------
int vtkDataObject::UpdateExtentIsOutsideOfTheExtent()
{
  switch ( this->GetExtentType() )
    {
    case VTK_PIECES_EXTENT:
      if ( this->UpdatePiece != this->Piece ||
           this->UpdateNumberOfPieces != this->NumberOfPieces ||
           this->UpdateGhostLevel != this->GhostLevel)
        {
        return 1;
        }
      break;

    case VTK_3D_EXTENT:
      if ( this->UpdateExtent[0] < this->Extent[0] ||
           this->UpdateExtent[1] > this->Extent[1] ||
           this->UpdateExtent[2] < this->Extent[2] ||
           this->UpdateExtent[3] > this->Extent[3] ||
           this->UpdateExtent[4] < this->Extent[4] ||
           this->UpdateExtent[5] > this->Extent[5] )
        {
        return 1;
        }
      break;

    // We should never have this case occur
    default:
      vtkErrorMacro( << "Internal error - invalid extent type!" );
      break;
    }
  return 0;
}


//----------------------------------------------------------------------------

unsigned long vtkDataObject::GetActualMemorySize()
{
  return this->FieldData->GetActualMemorySize();
}

//----------------------------------------------------------------------------

void vtkDataObject::CopyInformation( vtkDataObject *data )
{
  if ( this->GetExtentType() == VTK_3D_EXTENT &&
       data->GetExtentType() == VTK_3D_EXTENT )
    {
    memcpy( this->WholeExtent, data->GetWholeExtent(), 6*sizeof(int) );
    }
  else
    {
    this->MaximumNumberOfPieces = data->GetMaximumNumberOfPieces();
    }
  this->SetExtentTranslator(data->GetExtentTranslator());
}

//----------------------------------------------------------------------------
void vtkDataObject::ShallowCopy(vtkDataObject *src)
{
  if (!src)
    {
    vtkWarningMacro("Attempted to ShallowCopy from null.");
    return;
    }

  this->InternalDataObjectCopy(src);

  if (!src->FieldData)
    {
    this->SetFieldData(0);
    }
  else
    {
    if (this->FieldData)
      {
      this->FieldData->ShallowCopy(src->FieldData);
      }
    else
      {
      vtkFieldData* fd = vtkFieldData::New();
      fd->ShallowCopy(src->FieldData);
      this->SetFieldData(fd);
      fd->Delete();
      }
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::DeepCopy(vtkDataObject *src)
{
  vtkFieldData *srcFieldData = src->GetFieldData();
  
  this->InternalDataObjectCopy(src);

  if (srcFieldData)
    {
    vtkFieldData *newFieldData = vtkFieldData::New();
    newFieldData->DeepCopy(srcFieldData);
    this->SetFieldData(newFieldData);
    newFieldData->Delete();
    }
  else
    {
    this->SetFieldData(NULL);
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::InternalDataObjectCopy(vtkDataObject *src)
{
  int idx;

  this->DataReleased = src->DataReleased;
  for (idx = 0; idx < 6; ++idx)
    {
    this->WholeExtent[idx] = src->WholeExtent[idx];
    this->Extent[idx] = src->Extent[idx];
    // Copying these update variables caused me no end of grief.
    //this->UpdateExtent[idx] = src->UpdateExtent[idx];
    }
  this->Piece = src->Piece;
  this->NumberOfPieces = src->NumberOfPieces;
  this->MaximumNumberOfPieces = src->MaximumNumberOfPieces;
  // Copying these update variables caused me no end of grief.
  //this->UpdateNumberOfPieces = src->UpdateNumberOfPieces;
  //this->UpdatePiece = src->UpdatePiece;
  //this->UpdateGhostLevel = src->UpdateGhostLevel;
  this->ReleaseDataFlag = src->ReleaseDataFlag;
  // This also caused a pipeline problem.
  // An input pipelineMTime was copied to output.  Pipeline did not execute...
  // We do not copy MTime of object, so why should we copy these.
  //this->PipelineMTime = src->PipelineMTime;
  //this->UpdateTime = src->UpdateTime;
  //this->Locality = src->Locality;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetExtentTranslator(vtkExtentTranslator *t)
{
  if (this->ExtentTranslator == t)
    {
    return;
    }

  if (this->ExtentTranslator)
    {
    this->ExtentTranslator->UnRegister(this);
    this->ExtentTranslator = NULL;
    }
  if (t)
    {
    t->Register(this);
    this->ExtentTranslator = t;
    }

  this->Modified();
} 

//----------------------------------------------------------------------------
vtkExtentTranslator *vtkDataObject::GetExtentTranslator()
{
  return this->ExtentTranslator;
}

//----------------------------------------------------------------------------
// This should be a pure virutal method.
void vtkDataObject::Crop()
{
}

//----------------------------------------------------------------------------
void vtkDataObject::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->Source )
    {
    os << indent << "Source: " << this->Source << "\n";
    }
  else
    {
    os << indent << "Source: (none)\n";
    }

  os << indent << "Release Data: " 
     << (this->ReleaseDataFlag ? "On\n" : "Off\n");

  os << indent << "Data Released: " 
     << (this->DataReleased ? "True\n" : "False\n");
  
  os << indent << "Global Release Data: " 
     << (vtkDataObjectGlobalReleaseDataFlag ? "On\n" : "Off\n");

  os << indent << "MaximumNumberOfPieces: " << this->MaximumNumberOfPieces << endl;
  os << indent << "PipelineMTime: " << this->PipelineMTime << endl;
  os << indent << "UpdateTime: " << this->UpdateTime << endl;
  
  if (this->UpdateExtentInitialized)
    {
    os << indent << "UpdateExtent: Initialized\n";
    }
  else
    {
    os << indent << "UpdateExtent: Not Initialized\n";
    }
  os << indent << "Update Number Of Pieces: " << this->UpdateNumberOfPieces << endl;
  os << indent << "Update Piece: " << this->UpdatePiece << endl;
  os << indent << "Update Ghost Level: " << this->UpdateGhostLevel << endl;
  
  if (this->RequestExactExtent)
    {
    os << indent << "RequestExactExtent: On\n ";  
    }
  else
    {
    os << indent << "RequestExactExtent: Off\n ";  
    }    
  
  os << indent << "UpdateExtent: " << this->UpdateExtent[0] << ", "
     << this->UpdateExtent[1] << ", " << this->UpdateExtent[2] << ", "
     << this->UpdateExtent[3] << ", " << this->UpdateExtent[4] << ", "
     << this->UpdateExtent[5] << endl;
  os << indent << "WholeExtent: " << this->WholeExtent[0] << ", "
     << this->WholeExtent[1] << ", " << this->WholeExtent[2] << ", "
     << this->WholeExtent[3] << ", " << this->WholeExtent[4] << ", "
     << this->WholeExtent[5] << endl;

  os << indent << "Field Data:\n";
  this->FieldData->PrintSelf(os,indent.GetNextIndent());

  os << indent << "Locality: " << this->Locality << endl;
  os << indent << "NumberOfConsumers: " << this->NumberOfConsumers << endl;
  os << indent << "ExtentTranslator: (" << this->ExtentTranslator << ")\n";
  os << indent << "MaximumNumberOfPieces: " << this->MaximumNumberOfPieces << endl;
}

//----------------------------------------------------------------------------
#ifndef VTK_REMOVE_LEGACY_CODE
vtkDataObject* vtkDataObject::MakeObject()
{
  VTK_LEGACY_METHOD(MakeObject, "4.2");  
  return this->NewInstance();
}
#endif
