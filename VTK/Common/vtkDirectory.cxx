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
#include "vtkDirectory.h"

#include "vtkDebugLeaks.h"

vtkCxxRevisionMacro(vtkDirectory, "$Revision$");

//----------------------------------------------------------------------------
// Needed when we don't use the vtkStandardNewMacro.
vtkInstantiatorNewMacro(vtkDirectory);

//----------------------------------------------------------------------------
vtkDirectory* vtkDirectory::New()
{
#ifdef VTK_DEBUG_LEAKS
  vtkDebugLeaks::ConstructClass("vtkDirectory");
#endif    
  return new vtkDirectory;
}

vtkDirectory::vtkDirectory() 
  : Path(0), Files(0), NumberOfFiles(0)
{
}



vtkDirectory::~vtkDirectory() 
{
  for(int i =0; i < this->NumberOfFiles; i++)
    {
    delete [] this->Files[i];
    }
  delete [] this->Files;
  delete [] this->Path;
}



void vtkDirectory::PrintSelf(ostream& os, vtkIndent indent)
{ 
  this->Superclass::PrintSelf(os, indent);
  if(!this->Path)
    {
    os << indent << "Directory not open\n";
    return;
    }
  
  os << indent << "Directory for: " <<  this->Path << "\n";
  os << indent << "Contains the following files:\n";
  indent = indent.GetNextIndent();
  for(int i =0; i < this->NumberOfFiles; i++)
    {
    os << indent << this->Files[i] << "\n";
    }
}


// First microsoft compilers

#ifdef _MSC_VER
#include <windows.h>
#include <io.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int vtkDirectory::Open(const char* name)
{
  char* buf=0;
  int n = static_cast<int>(strlen(name));
  if (name[n - 1] == '/') 
    {
    buf = new char[n + 1 + 1];
    sprintf(buf, "%s*", name);
    } 
  else
    {
    buf = new char[n + 2 + 1];
    sprintf(buf, "%s/*", name);
    }
  struct _finddata_t data;      // data of current file
  
  // First count the number of files in the directory
  long srchHandle = _findfirst(buf, &data);
  if (srchHandle == -1)
    {
    this->NumberOfFiles = 0;
    _findclose(srchHandle);
    delete[] buf;
    return 0;
    }
  
  this->NumberOfFiles = 1;
  while(_findnext(srchHandle, &data) != -1)
    {
    this->NumberOfFiles++;
    }
  this->Files = new char*[this->NumberOfFiles];
  // close the handle 
  _findclose(srchHandle);
  // Now put them into the file array
  srchHandle = _findfirst(buf, &data);
  delete [] buf;
  
  if (srchHandle == -1)
    {
    this->NumberOfFiles = 0;
    _findclose(srchHandle);
    return 0;
    }
  
  // Loop through names
  int i = 0;
  do 
    {
    this->Files[i] = strcpy(new char[strlen(data.name)+1], data.name);
    i++;
    } 
  while (_findnext(srchHandle, &data) != -1);
  this->Path = strcpy(new char[strlen(name)+1], name);
  return _findclose(srchHandle) != -1;
}

const char* vtkDirectory::GetCurrentWorkingDirectory(char* buf, 
                                                     unsigned int len)
{
  return _getcwd(buf, len);
}

#else

// Now the POSIX style directory access

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

int vtkDirectory::Open(const char* name)
{
  DIR* dir = opendir(name);
  if (!dir) 
    {
    return 0;
    }
  this->NumberOfFiles = 0;
  dirent* d =0;
  
  for (d = readdir(dir); d; d = readdir(dir))
    {
    this->NumberOfFiles++;
    }
  this->Files = new char*[this->NumberOfFiles];
  closedir(dir);
  
  dir = opendir(name);
  if (!dir) 
    {
    return 0;
    }
  int i = 0;
  for (d = readdir(dir); d; d = readdir(dir))
    {
    this->Files[i] = strcpy(new char[strlen(d->d_name)+1], d->d_name);
    i++;
    }
  this->Path = strcpy(new char[strlen(name)+1], name);
  closedir(dir);
  return 1;
}

const char* vtkDirectory::GetCurrentWorkingDirectory(char* buf, 
                                                     unsigned int len)
{
  return getcwd(buf, len);
}

#endif


const char* vtkDirectory::GetFile(int index)
{
  if(index >= this->NumberOfFiles || index < 0)
    {
    vtkErrorMacro( << "Bad index for GetFile on vtkDirectory\n");
    return 0;
    }
  
  return this->Files[index];
}

