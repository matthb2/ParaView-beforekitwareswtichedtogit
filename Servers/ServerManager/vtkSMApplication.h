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
// .NAME vtkSMApplication - provides initialization and finalization for server manager
// .SECTION Description
// vtkSMApplication provides methods to initialize and finalize the
// server manager.

#ifndef __vtkSMApplication_h
#define __vtkSMApplication_h

#include "vtkSMObject.h"

class VTK_EXPORT vtkSMApplication : public vtkSMObject
{
public:
  static vtkSMApplication* New();
  vtkTypeRevisionMacro(vtkSMApplication, vtkSMObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Perform initialization: add the server manager symbols to the
  // interpreter, read default interfaces from strings, create
  // singletons... Should be called before any server manager objects
  // are created.
  void Initialize();

  // Description:
  // Cleanup: cleans singletons
  // Should be called before exit, after all server manager objects
  // are deleted.
  void Finalize();

  // Description:
  int ParseConfigurationFile(const char* fname, const char* dir);

protected:
  vtkSMApplication();
  ~vtkSMApplication();

  virtual void SaveState(const char*, ostream*, vtkIndent) {};

private:
  vtkSMApplication(const vtkSMApplication&); // Not implemented
  void operator=(const vtkSMApplication&); // Not implemented
};

#endif
