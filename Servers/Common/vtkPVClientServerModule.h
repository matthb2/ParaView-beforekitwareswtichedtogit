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
// .NAME vtkPVClientServerModule
// .SECTION Description
// A class to encapaulate all of the process initialization,
// distributed data model and duplication  of the pipeline.
// Filters and compositers will still need a controller, 
// but every thing else should be handled here.  This class 
// sets up the default MPI processes with the user interface
// running on process 0.  I plan to make an alternative module
// for client server mode, where the client running the UI 
// is not in the MPI group but links to the MPI group through 
// a socket connection.

#ifndef __vtkPVClientServerModule_h
#define __vtkPVClientServerModule_h

#include "vtkPVProcessModule.h"

class vtkKWRemoteExecute;
class vtkMapper;
class vtkMapper;
class vtkMultiProcessController;
class vtkPVPart;
class vtkSocketController;
class vtkSource;
class vtkSocketCommunicator;

class VTK_EXPORT vtkPVClientServerModule : public vtkPVProcessModule
{
public:
  static vtkPVClientServerModule* New();
  vtkTypeRevisionMacro(vtkPVClientServerModule,vtkPVProcessModule);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // This starts the whole application.
  // This method initializes the MPI controller, then passes control
  // onto the init method.
  virtual int Start(int argc, char **argv);

  // Description:  
  // Start calls this method to continue initialization.
  // This method initializes the sockets and then calls
  // vtkPVApplication::Start(argc, argv);
  void Initialize();

  // Description:
  // This breaks rmi loops and cleans up processes.`                
  virtual void Exit();

  // Description:
  // Get the Partition piece. -1 means no partition assigned to this process.
  virtual int GetPartitionId();

  // Description:
  // Get the number of processes participating in sharing the data.
  virtual int GetNumberOfPartitions();
  vtkGetMacro(NumberOfProcesses, int);
  vtkSetMacro(NumberOfProcesses, int);
  vtkGetMacro(MultiProcessMode, int);
  vtkSetMacro(MultiProcessMode, int);
  vtkGetMacro(NumberOfServerProcesses, int);
  vtkSetMacro(NumberOfServerProcesses, int);
  
  // Description:
  // This is a socket controller used to communicate
  // between the client and process 0 of the server.
  vtkGetObjectMacro(SocketController, vtkSocketController);

  // Description:
  // Return the socket to the RenderServer, if this is not
  // set, return the SocketController.
  vtkSocketController* GetRenderServerSocketController();

  //BTX
  // Description:
  // Module dependant method for collecting data information from all procs.
  virtual void GatherInformation(vtkPVInformation* info,
                                 vtkClientServerID id);
  virtual void GatherInformationRenderServer(vtkPVInformation* info,
                                             vtkClientServerID id);
  //ETX
  virtual void GatherInformationInternal(const char* infoClassName,
                                         vtkObject* object);

  // Description:
  // Get a directory listing for the given directory.  This
  // implementation will always give a listing on the server side.
  virtual int GetDirectoryListing(const char* dir, vtkStringList* dirs,
                                  vtkStringList* files, int save);
  
//BTX
  enum 
    {
    SINGLE_PROCESS_MODE = 0,
    MPI_MODE
    };
//ETX
  
  static void ErrorCallback(vtkObject *caller, unsigned long eid, void *clientdata, void *calldata);

  // Description:
  // Process a client server message on the server.
  void ProcessMessage(unsigned char* arg, size_t len);
  
  friend void vtkPVClientServerLastResultRMI(  void *, void* , int ,int );
  //ETX

  // Description:
  // Used internally.  Do not call.  Use LoadModule instead.
  virtual int LoadModuleInternal(const char* name, const char* directory);

  // Description:
  // ConnectID is used as a very simple security check. It is usually
  // specified on the command line of the server and the client.
  // The server will exit unless the ids match.
  vtkSetMacro(ConnectID, int);
  vtkGetMacro(ConnectID, int);

  // Description:  
  // Internal use. Made public to allow callbacks access.
  vtkGetMacro(Enabled, int);

protected:
  vtkPVClientServerModule();
  ~vtkPVClientServerModule();

  // Description:
  // Given the servers that need to receive the stream, create a flag
  // that will send it to the correct places for this process module and
  // make sure it only gets sent to each server once.
  virtual vtkTypeUInt32 CreateSendFlag(vtkTypeUInt32 servers);
  // send a stream to the client
  virtual int SendStreamToClient(vtkClientServerStream&);
  // send a stream to the data server
  virtual int SendStreamToDataServer(vtkClientServerStream&);
  // send a stream to the data server root mpi process
  virtual int SendStreamToDataServerRoot(vtkClientServerStream&);
  // send a stream to the render server
  virtual int SendStreamToRenderServer(vtkClientServerStream&);
  // send a stream to the render server root mpi process
  virtual int SendStreamToRenderServerRoot(vtkClientServerStream&);
  
  // Description:
  // Get the last result from the DataServer, RenderServer or Client.
  // If these are MPI processes, only the root last result is returned.
  virtual const vtkClientServerStream& GetLastDataServerResult();
  virtual const vtkClientServerStream& GetLastRenderServerResult();
  
  // Description:
  // Send the last client server result to the client called from an RMI
  void SendLastClientServerResult();
  // Description:
  // Connect to servers or clients, this will either set up a wait
  // loop waiting for a connection, or it will create a 
  void Connect();
  // Description:
  // Connect to a remote server or client already waiting for us.
  void ConnectToRemote();
  // Description:
  // Setup a wait connection that is waiting for a remote process to
  // connect to it.  This can be either the client or the server.
  void SetupWaitForConnection();
  // Description:
  // Return 1 if the connection should wait, and 0 if the connet
  int ShouldWaitForConnection();
  // Description:
  // Start a remote paraview server process.  Return 0 if connection failed.
  int StartRemoteParaView(vtkSocketCommunicator* comm);
  // Description:
  // Open a dialog to enter server information, if the start
  // variable is set to 1 in this function, then a remote paraview
  // should be started with StartRemoteParaView.
  int OpenConnectionDialog(int* start);

  // Description:
  // Create connection between render server and data server
  void InitializeRenderServer();
  
  int NumberOfServerProcesses;
  vtkSocketController* SocketController;
  vtkSocketController* RenderServerSocket;
  int NumberOfRenderServerProcesses;
  // To pass arguments through controller single method.
  int    ArgumentCount;
  char** Arguments;
  int    ReturnValue;

  int MultiProcessMode;
  int NumberOfProcesses;
  int GatherRenderServer;
  vtkClientServerStream* LastServerResultStream;
  
  vtkKWRemoteExecute* RemoteExecution;

  int Enabled;
  int ConnectID;
private:  
  vtkPVClientServerModule(const vtkPVClientServerModule&); // Not implemented
  void operator=(const vtkPVClientServerModule&); // Not implemented
};

#endif
