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
// .NAME Test to print system information useful for remote debugging.
// .SECTION Description
// Remote dashboard debugging often requires access to the
// CMakeCache.txt file.  This test will display the file.

#include "vtkDebugLeaks.h"
#include "../vtk/Testing/Cxx/SystemInformation.h"
#include <sys/stat.h>

void vtkSystemInformationPrintFile(const char* name, ostream& os)
{
  os << "================================================================\n";
  struct stat fs;
  if(stat(name, &fs) != 0)
    {
    os << "The file \"" << name << "\" does not exist.\n";
    return;
    }

#ifdef _WIN32
  ifstream fin(name, ios::in | ios::binary);
#else
  ifstream fin(name, ios::in);
#endif

  if(fin)
    {
    os << "Contents of \"" << name << "\":\n";
    os << "----------------------------------------------------------------\n";
    const int bufferSize = 4096;
    char buffer[bufferSize];
    // This copy loop is very sensitive on certain platforms with
    // slightly broken stream libraries (like HPUX).  Normally, it is
    // incorrect to not check the error condition on the fin.read()
    // before using the data, but the fin.gcount() will be zero if an
    // error occurred.  Therefore, the loop should be safe everywhere.
    while(fin)
      {
      fin.read(buffer, bufferSize);
      if(fin.gcount())
        {
        os.write(buffer, fin.gcount());
        }
      }
    os.flush();
    }
  else
    {
    os << "Error opening \"" << name << "\" for reading.\n";
    }
}

int main(int,char *[])
{
  vtkDebugLeaks::PromptUserOff();
  const char* files[] =
    {
      Xdmf_BINARY_DIR "/CMakeCache.txt", 
      Xdmf_BINARY_DIR "/Ice/libsrc/IceConfig.h",
      Xdmf_BINARY_DIR "/libsrc/XdmfConfig.h.h",
      Xdmf_BINARY_DIR "/CMakeError.log",
      Xdmf_BINARY_DIR "/CMake/CMakeCache.txt", 
      Xdmf_BINARY_DIR "/XDMFBuildSettings.cmake",
      Xdmf_BINARY_DIR "/XDMFLibraryDepends.cmake",
      Xdmf_BINARY_DIR "/XDMFConfig.cmake",
      0
    };

  const char** f;
  for(f = files; *f; ++f)
    {
    vtkSystemInformationPrintFile(*f, cout);
    }
  
  return 0;
} 
