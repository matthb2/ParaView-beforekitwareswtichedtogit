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
// .NAME vtkSMStringListDomain - list of strings
// .SECTION Description
// vtkSMStringListDomain represents a domain consisting of a list of
// strings. It only works with vtkSMStringVectorProperty. 
// Valid XML elements are:
// @verbatim
// * <String value="">
// @endverbatim
// .SECTION See Also
// vtkSMDomain vtkSMStringVectorProperty

#ifndef __vtkSMStringListDomain_h
#define __vtkSMStringListDomain_h

#include "vtkSMDomain.h"

//BTX
struct vtkSMStringListDomainInternals;
//ETX

class VTK_EXPORT vtkSMStringListDomain : public vtkSMDomain
{
public:
  static vtkSMStringListDomain* New();
  vtkTypeRevisionMacro(vtkSMStringListDomain, vtkSMDomain);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Returns true if the value of the property is in the domain.
  // The propery has to be a vtkSMStringVectorProperty. If all 
  // vector values are in the domain, it returns 1. It returns
  // 0 otherwise.
  virtual int IsInDomain(vtkSMProperty* property);

  // Description:
  // Returns true if the string is in the domain.
  int IsInDomain(const char* string, unsigned int& idx);

  // Description:
  // Returns the number of strings in the domain.
  unsigned int GetNumberOfStrings();

  // Description:
  // Returns a string in the domain. The pointer may become
  // invalid once the domain has been modified.
  const char* GetString(unsigned int idx);

  // Description:
  // Adds a new string to the domain.
  unsigned int AddString(const char* string);

  // Description:
  // Removes a string from the domain.
  void RemoveString(const char* string);

  // Description:
  // Removes all strings from the domain.
  void RemoveAllStrings();

protected:
  vtkSMStringListDomain();
  ~vtkSMStringListDomain();

  // Description:
  // Set the appropriate ivars from the xml element. Should
  // be overwritten by subclass if adding ivars.
  virtual int ReadXMLAttributes(vtkSMProperty* prop, vtkPVXMLElement* element);

  virtual void SaveState(const char* name, ostream* file, vtkIndent indent);

  vtkSMStringListDomainInternals* SLInternals;

private:
  vtkSMStringListDomain(const vtkSMStringListDomain&); // Not implemented
  void operator=(const vtkSMStringListDomain&); // Not implemented
};

#endif
