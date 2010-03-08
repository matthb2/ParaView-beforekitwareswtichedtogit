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
// .NAME vtkSMCameraConfigurationReader - A reader for XML camera configuration.
//
// .SECTION Description
// A reader for XML camera configuration. Reades camera configuration files.
// writen by the vtkSMCameraConfigurationWriter.
//
// .SECTION See Also
// vtkSMCameraConfigurationWriter, vtkSMProxyConfigurationReader
//
// .SECTION Thanks
// This class was contributed by SciberQuest Inc.
#ifndef __vtkSMCameraConfigurationReader_h
#define __vtkSMCameraConfigurationReader_h

#include "vtkSMProxyConfigurationReader.h"

class vtkSMProxy;
class vtkSMRenderViewProxy;
class vtkPVXMLElement;

class VTK_EXPORT vtkSMCameraConfigurationReader : public vtkSMProxyConfigurationReader
{
public:
  vtkTypeRevisionMacro(vtkSMCameraConfigurationReader,vtkSMProxyConfigurationReader);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkSMCameraConfigurationReader *New();

  // Description:
  // Set the render view proxy to extract camera properties from.
  void SetRenderViewProxy(vtkSMRenderViewProxy *rvProxy);


  // Description:
  // Read the named file, and push the properties into the underying
  // managed render view proxy. This will make sure the renderview is
  // updated after the read.
  virtual int ReadConfiguration(const char *filename);
  virtual int ReadConfiguration(vtkPVXMLElement *x);
  // unhide
  virtual int ReadConfiguration()
    {
    return this->Superclass::ReadConfiguration();
    }

protected:
  vtkSMCameraConfigurationReader();
  virtual ~vtkSMCameraConfigurationReader();

  // Protect the superclass's SetProxy, clients are forced to use
  // SetRenderViewProxy
  void SetProxy(vtkSMProxy *proxy){ vtkErrorMacro("Use SetRenderViewProxy."); }

private:
  vtkSMCameraConfigurationReader(const vtkSMCameraConfigurationReader&);  // Not implemented.
  void operator=(const vtkSMCameraConfigurationReader&);  // Not implemented.
};

#endif

