/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2000-2001 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name of Kitware nor the names of any contributors may be used
   to endorse or promote products derived from this software without specific 
   prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
// .NAME vtkKWDirectoryUtilities - Platform independent directory handling.
// .SECTION Description
// vtkKWDirectoryUtilities provides a set of tools for platform
// independent handling of directories, environment variables, and
// program locations.

#ifndef __vtkKWDirectoryUtilities_h
#define __vtkKWDirectoryUtilities_h

#include "vtkObject.h"

class VTK_EXPORT vtkKWDirectoryUtilities : public vtkObject
{
public:
  vtkTypeRevisionMacro(vtkKWDirectoryUtilities, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);  
  static vtkKWDirectoryUtilities* New();
  
  // Description:
  // Get an environment variable with the given name.  Returns 0 if
  // the variable does not exist.
  const char* GetEnv(const char* key);
  
  // Description:
  // Get the current working directory.
  const char* GetCWD();
  
  // Description:
  // Convert a path to UNIX-style slashes.  Backslashes are replaced
  // with forward slashes.  Trailing slashes are removed, and leading
  // tildas ("~") are replaced with the home directory if the HOME
  // environment variable is set.
  const char* ConvertToUnixSlashes(const char* path);
  
  // Description:
  // Tests the existence of a file.  Returns 1 for exists, 0 otherwise.
  int FileExists(const char* filename);
  
  // Description:
  // Tests whether a file is a directory.  Returns 1 for yes, 0 for no.
  int FileIsDirectory(const char* name);
  
  //BTX
  
  // Description:
  // Get the system path from the PATH environment variable.  Returns
  // an array of pointers to each path entry.  The list is terminated
  // by a 0 pointer.
  const char*const* GetSystemPath();
  //ETX
  
  // Description:
  // Find a program with the given name.  Returns the full path to the
  // executable file, including its name.  If the program is not
  // found, 0 is returned.
  const char* FindProgram(const char* name);
  
  // Description:
  // Get the given directory in a simplified full path format.
  const char* CollapseDirectory(const char* dir);
  
  // Description:
  // Find the location of the executable from the value of argv[0].
  const char* FindSelfPath(const char* argv0);
  
protected:
  vtkKWDirectoryUtilities();
  ~vtkKWDirectoryUtilities();
  
  char** SystemPath;
  char* CWD;
  char* UnixSlashes;
  char* ProgramFound;
  char* SelfPath;
  char* CollapsedDirectory;
  
private:
  vtkKWDirectoryUtilities(const vtkKWDirectoryUtilities&); // Not implemented
  void operator=(const vtkKWDirectoryUtilities&); // Not implemented
};

#endif
