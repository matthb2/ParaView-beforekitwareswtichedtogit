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
// This will be the default.
#include "vtkMultiProcessController.h"

#include "vtkByteSwap.h"
#include "vtkCollection.h"
#include "vtkDummyController.h"
#include "vtkObjectFactory.h"
#include "vtkOutputWindow.h"
#include "vtkProcessGroup.h"
#include "vtkSubCommunicator.h"
#include "vtkToolkits.h"

#ifdef VTK_USE_MPI
#include "vtkMPIController.h"
#endif

//----------------------------------------------------------------------------
// Needed when we don't use the vtkStandardNewMacro.
vtkInstantiatorNewMacro(vtkMultiProcessController);

//----------------------------------------------------------------------------
// Helper class to contain the RMI information.  
// A subclass of vtkObject so that I can keep them in a collection.
class VTK_PARALLEL_EXPORT vtkMultiProcessControllerRMI : public vtkObject
{
public:
  static vtkMultiProcessControllerRMI *New(); 
  vtkTypeRevisionMacro(vtkMultiProcessControllerRMI, vtkObject);
  
  int Tag;
  vtkRMIFunctionType Function;
  void *LocalArgument;

  unsigned long Id;
  
protected:
  vtkMultiProcessControllerRMI() {};
  vtkMultiProcessControllerRMI(const vtkMultiProcessControllerRMI&);
  void operator=(const vtkMultiProcessControllerRMI&);
};

vtkCxxRevisionMacro(vtkMultiProcessControllerRMI, "$Revision$");
vtkStandardNewMacro(vtkMultiProcessControllerRMI);

vtkCxxRevisionMacro(vtkMultiProcessController, "$Revision$");

//----------------------------------------------------------------------------
// An RMI function that will break the "ProcessRMIs" loop.
void vtkMultiProcessControllerBreakRMI(void *localArg, 
                                       void *remoteArg, int remoteArgLength, 
                                       int vtkNotUsed(remoteId))
{
  vtkMultiProcessController *controller;
  remoteArg = remoteArg;
  remoteArgLength = remoteArgLength;
  controller = (vtkMultiProcessController*)(localArg);
  controller->SetBreakFlag(1);
}



//----------------------------------------------------------------------------
vtkMultiProcessController::vtkMultiProcessController()
{
  int i;

  this->RMICount = 1;
  
  this->RMIs = vtkCollection::New();
  
  this->SingleMethod = 0;
  this->SingleData = 0;   

  this->Communicator = 0;
  this->RMICommunicator = 0;
  
  for ( i = 0; i < VTK_MAX_THREADS; i++ )
    {
    this->MultipleMethod[i] = NULL;
    this->MultipleData[i] = NULL;
    }

  this->BreakFlag = 0;
  this->ForceDeepCopy = 1;

  this->OutputWindow = 0;

  // Define an rmi internally to exit from the processing loop.
  this->AddRMI(vtkMultiProcessControllerBreakRMI, this, BREAK_RMI_TAG);
}

//----------------------------------------------------------------------------
// This is an old comment that I do not know is true:
// (We need to have a "GetNetReferenceCount" to avoid memory leaks.)
vtkMultiProcessController::~vtkMultiProcessController()
{
  if ( this->OutputWindow &&
       (this->OutputWindow == vtkOutputWindow::GetInstance()) )
    {
    vtkOutputWindow::SetInstance(0);
    }

  if (this->OutputWindow)
    {
    this->OutputWindow->Delete();
    }

  this->RMIs->Delete();
  this->RMIs = NULL;
}

 
//----------------------------------------------------------------------------
void vtkMultiProcessController::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  vtkMultiProcessControllerRMI *rmi;
  vtkIndent nextIndent = indent.GetNextIndent();
  
  os << indent << "Break flag: " << (this->BreakFlag ? "(yes)" : "(no)") 
     << endl;
  os << indent << "Force deep copy: " << (this->ForceDeepCopy ? "(yes)" : "(no)") 
     << endl;
  os << indent << "Output window: ";
  if (this->OutputWindow)
    {
    os << endl;
    this->OutputWindow->PrintSelf(os, nextIndent);
    }
  else
    {
    os << "(none)" << endl;
    }
  os << indent << "Communicator: ";
  if (this->Communicator)
    {
    os << endl;
    this->Communicator->PrintSelf(os, nextIndent);
    }
  else
    {
    os << "(none)" << endl;
    }
  os << indent << "RMI communicator: ";
  if (this->RMICommunicator)
    {
    os << endl;
    this->RMICommunicator->PrintSelf(os, nextIndent);
    }
  else
    {
    os << "(none)" << endl;
    }
  os << indent << "RMIs: \n";
  
  this->RMIs->InitTraversal();
  while ( (rmi = (vtkMultiProcessControllerRMI*)(this->RMIs->GetNextItemAsObject())) )
    {
    os << nextIndent << rmi->Tag << endl;
    }
  
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::SetNumberOfProcesses(int num)
{
  if(this->Communicator)
    {
    this->Communicator->SetNumberOfProcesses(num);
    }
  else
    {
    vtkErrorMacro("Communicator not set.");
    }
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::GetNumberOfProcesses()
{
  if(this->Communicator)
    {
    return this->Communicator->GetNumberOfProcesses();
    }
  else
    {
    vtkErrorMacro("Communicator not set.");
    return 0;
    }
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::GetLocalProcessId()
{
  if(this->Communicator)
    {
    return this->Communicator->GetLocalProcessId();
    }
  else
    {
    vtkErrorMacro("Communicator not set.");
    return -1;
    }
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::SetSingleMethod( vtkProcessFunctionType f, 
                                                 void *data )
{
  this->SingleMethod = f;
  this->SingleData   = data;
}

//----------------------------------------------------------------------------
// Set one of the user defined methods that will be run on NumberOfProcesses
// processes when MultipleMethodExecute is called. This method should be
// called with index = 0, 1, ..,  NumberOfProcesses-1 to set up all the
// required user defined methods
void vtkMultiProcessController::SetMultipleMethod( int index,
                                 vtkProcessFunctionType f, void *data )
{ 
  // You can only set the method for 0 through NumberOfProcesses-1
  if ( index >= this->GetNumberOfProcesses() )
    {
    vtkErrorMacro( << "Can't set method " << index << 
      " with a processes count of " << this->GetNumberOfProcesses() );
    }
  else
    {
    this->MultipleMethod[index] = f;
    this->MultipleData[index]   = data;
    }
}

//-----------------------------------------------------------------------------
vtkMultiProcessController *vtkMultiProcessController::CreateSubController(
                                                         vtkProcessGroup *group)
{
  if (group->GetCommunicator() != this->Communicator)
    {
    vtkErrorMacro(<< "Invalid group for creating a sub controller.");
    return NULL;
    }

  if (group->FindProcessId(this->GetLocalProcessId()) < 0)
    {
    // The group does not contain this process.  Just return NULL.
    return NULL;
    }

  vtkSubCommunicator *subcomm = vtkSubCommunicator::New();
  subcomm->SetGroup(group);

  // We only need a basic implementation of vtkMultiProcessController for the
  // subgroup, so we just use vtkDummyController here.  It's a bit of a misnomer
  // and may lead to confusion, but I think it's better than creating yet
  // another class we have to maintain.
  vtkDummyController *subcontroller = vtkDummyController::New();
  subcontroller->SetCommunicator(subcomm);
  subcontroller->SetRMICommunicator(subcomm);

  subcomm->Delete();

  return subcontroller;
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::RemoveFirstRMI(int tag)
{
  vtkMultiProcessControllerRMI *rmi = NULL;
  this->RMIs->InitTraversal();
  while ( (rmi = (vtkMultiProcessControllerRMI*)(this->RMIs->GetNextItemAsObject())) )
    {
    if (rmi->Tag == tag)
      {
      this->RMIs->RemoveItem(rmi);
      return 1;
      }
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::RemoveRMI(unsigned long id)
{
  vtkMultiProcessControllerRMI *rmi = NULL;
  this->RMIs->InitTraversal();
  while ( (rmi = (vtkMultiProcessControllerRMI*)(this->RMIs->GetNextItemAsObject())) )
    {
    if (rmi->Id == id)
      {
      this->RMIs->RemoveItem(rmi);
      return 1;
      }
    }

  return 0;
}

//----------------------------------------------------------------------------
unsigned long vtkMultiProcessController::AddRMI(vtkRMIFunctionType f, 
                                       void *localArg, int tag)
{
  vtkMultiProcessControllerRMI *rmi = vtkMultiProcessControllerRMI::New();

  // Remove any previously registered RMI handler for the tag.
  this->RemoveFirstRMI(tag);

  rmi->Tag = tag;
  rmi->Function = f;
  rmi->LocalArgument = localArg;
  rmi->Id = this->RMICount;
  this->RMICount++;

  this->RMIs->AddItem(rmi);
  rmi->Delete();

  return rmi->Id;
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::TriggerRMIOnAllChildren(
  void *arg, int argLength, int rmiTag)
{
  int myid = this->GetLocalProcessId();
    this->TriggerRMIInternal(-1, arg, argLength, rmiTag, true);
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::TriggerRMI(int remoteProcessId, 
                                           void *arg, int argLength,
                                           int rmiTag)
{
  // Deal with sending RMI to ourself here for now.
  if (remoteProcessId == this->GetLocalProcessId())
    {
    this->ProcessRMI(remoteProcessId, arg, argLength, rmiTag);
    return;
    }

  this->TriggerRMIInternal(remoteProcessId, arg, argLength, rmiTag, false);
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::TriggerRMIInternal(int remoteProcessId, 
    void* arg, int argLength, int rmiTag, bool propagate)
{
  int addressedMessage[129];
  int* triggerMessage = &(addressedMessage[1]);
  triggerMessage[0] = rmiTag;
  triggerMessage[1] = argLength;
  
  addressedMessage[0] = remoteProcessId; //include address in message
					//and process rest of function as was

  // It is important for the remote process to know what process invoked it.
  // Multiple processes might try to invoke the method at the same time.
  // The remote method will know where to get additional args.
  triggerMessage[2] = this->GetLocalProcessId();
  
  // Pass the propagate flag.
  triggerMessage[3] = propagate? 1 : 0;

  // We send the header in Little Endian order.
  vtkByteSwap::SwapLERange(triggerMessage, 4);

  // If the message is small, we will try to get the message sent over using a
  // single Send(), rather than two. This helps speed up communication
  // significantly, since sending multiple small messages is generally slower
  // than sending a single large message.
 // if (argLength >= 0 && static_cast<unsigned int>(argLength) < sizeof(int)*(128-4))
//    {  //FIXME:: Figure out how to deal with this later
//    if (argLength > 0)
 //     {
//      memcpy(&triggerMessage[4], arg, argLength);
//      }
//    int num_bytes = static_cast<int>(4*sizeof(int)) + argLength;
//    num_bytes += sizeof(int); //for address
//    this->RMICommunicator->Send(reinterpret_cast<unsigned char*>(addressedMessage),
//      num_bytes, 0, RMI_TAG);
//    }
//  else
//    {
	printf("node %d is sending to node 1\n",this->GetLocalProcessId());
printf("addressed message 0x%x%x%x%x%x, triggerMessage 0x%x%x%x%x%x%x\n",addressedMessage[0],addressedMessage[1],addressedMessage[2],addressedMessage[3],addressedMessage[4],triggerMessage[0],triggerMessage[1],triggerMessage[2],triggerMessage[3],triggerMessage[4]);

    this->RMICommunicator->Send(
      reinterpret_cast<unsigned char*>(addressedMessage), 
      static_cast<int>(4*sizeof(int)) + sizeof(int), 1, RMI_TAG);
    if (argLength > 0)
      {
      this->RMICommunicator->Send((char*)arg, argLength, 1,  
        RMI_ARG_TAG);
      }
        printf("node %d done sending\n",this->GetLocalProcessId());
//    }
}

//----------------------------------------------------------------------------
void vtkMultiProcessController::TriggerBreakRMIs()
{
  int idx, num;

  if (this->GetLocalProcessId() != 0)
    {
    vtkErrorMacro("Break should be triggered from process 0.");
    return;
    }

  num = this->GetNumberOfProcesses();
  for (idx = 1; idx < num; ++idx)
    {
    this->TriggerRMI(idx, NULL, 0, BREAK_RMI_TAG);
    }
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::ProcessRMIs()
{
  return this->ProcessRMIs(1, 0);
}

//----------------------------------------------------------------------------
int vtkMultiProcessController::ProcessRMIs(int reportErrors, int dont_loop)
{
  int  addressedMessage[129];
  int* triggerMessage = &(addressedMessage[1]);
  unsigned char *arg = NULL;
  int flag;
  int error = RMI_NO_ERROR;
printf("ProcessRMIs on %d\n",this->GetLocalProcessId());
  do 
    {
 if(this->GetLocalProcessId() == 1) {
    this->RMICommunicator->Iprobe(1,RMI_TAG,&flag);
    if(flag) 
    {
	    printf("Rank 1 about to recv\n"); 
	    if (!this->RMICommunicator->Receive(
	        reinterpret_cast<unsigned char*>(addressedMessage), 
	        static_cast<vtkIdType>(129*sizeof(int)), ANY_SOURCE, RMI_TAG) ||
	      this->RMICommunicator->GetCount() < static_cast<int>(4*sizeof(int)))
	      {
	      if (reportErrors)
	        {
	        vtkErrorMacro("Could not receive RMI trigger message.");
	        }
	      error = RMI_TAG_ERROR;
	    printf("recv done\n");
     } //FIXME remove break? 
     else {
	printf("Should have recieved something, but didn't \n");
	}
      break;
      
} else {goto process_rmi_loop;}
//fixme else recieve broadcast
#ifdef VTK_WORDS_BIGENDIAN
    // Header is sent in little-endian form. We need to convert it to  big
    // endian.
    vtkByteSwap::SwapLERange(triggerMessage, 4);
#endif
    	if(addressedMessage[0] != this->GetLocalProcessId())
    		this->RMICommunicator->Broadcast(reinterpret_cast<unsigned char*>(addressedMessage),129*sizeof(int),1); 
  } //fixme indentation -- close for if(rank==0)
  else { //possibly shouldn't use else for performance?? causing deadlock? //may7 now pointless
   this->RMICommunicator->Broadcast(reinterpret_cast<unsigned char*>(addressedMessage),129*sizeof(int),1);
       }
   if(addressedMessage[0] == this->GetLocalProcessId() || addressedMessage[0] <0) {
   triggerMessage = &(addressedMessage[1]);
   this->ProcessRMI(triggerMessage[2], arg, triggerMessage[1], 
      triggerMessage[0]);
    if (arg)
      {
      delete [] arg;
      arg = NULL;
      }
    }
    else
    {
	continue;
    }
   printf("processrmis: addressed message 0x%x%x%x%x%x, triggerMessage 0x%x%x%x%x%x%x\n",addressedMessage[0],addressedMessage[1],addressedMessage[2],addressedMessage[3],addressedMessage[4],triggerMessage[0],triggerMessage[1],triggerMessage[2],triggerMessage[3],triggerMessage[4]);
process_rmi_loop:
    // check for break
    if (this->BreakFlag)
      {
      this->BreakFlag = 0;
      return error;
      }
    } while (!dont_loop);

  return error;
}


//----------------------------------------------------------------------------
void vtkMultiProcessController::ProcessRMI(int remoteProcessId, 
                                           void *arg, int argLength,
                                           int rmiTag)
{
  vtkMultiProcessControllerRMI *rmi = NULL;
  int found = 0;

  // find the rmi
  this->RMIs->InitTraversal();
  while ( !found &&
   (rmi = (vtkMultiProcessControllerRMI*)(this->RMIs->GetNextItemAsObject())) )
    {
    if (rmi->Tag == rmiTag)
      {
      found = 1;
      }
    }
  
  if ( ! found)
    {
    vtkErrorMacro("Process " << this->GetLocalProcessId() << 
                  " Could not find RMI with tag " << rmiTag);
    }
  else
    {
    if ( rmi->Function )
      {
      (*rmi->Function)(rmi->LocalArgument, arg, argLength, remoteProcessId);
      }     
    }
}

//============================================================================
// The intent is to give access to a processes controller from a static method.

vtkMultiProcessController *VTK_GLOBAL_MULTI_PROCESS_CONTROLLER = NULL;
//----------------------------------------------------------------------------
vtkMultiProcessController *vtkMultiProcessController::GetGlobalController()
{
  if (VTK_GLOBAL_MULTI_PROCESS_CONTROLLER == NULL)
    {
    return NULL;
    }
  
  return VTK_GLOBAL_MULTI_PROCESS_CONTROLLER->GetLocalController();
}
//----------------------------------------------------------------------------
// This can be overridden in the subclass to translate controllers.
vtkMultiProcessController *vtkMultiProcessController::GetLocalController()
{
  return VTK_GLOBAL_MULTI_PROCESS_CONTROLLER;
}
//----------------------------------------------------------------------------
// This can be overridden in the subclass to translate controllers.
void vtkMultiProcessController::SetGlobalController(
                                   vtkMultiProcessController *controller)
{
  VTK_GLOBAL_MULTI_PROCESS_CONTROLLER = controller;
}


