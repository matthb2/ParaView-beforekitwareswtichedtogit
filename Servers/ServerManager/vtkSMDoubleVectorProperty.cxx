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
#include "vtkSMDoubleVectorProperty.h"

#include "vtkClientServerStream.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkProcessModule.h"

#include <vtkstd/vector>

vtkStandardNewMacro(vtkSMDoubleVectorProperty);
vtkCxxRevisionMacro(vtkSMDoubleVectorProperty, "$Revision$");

struct vtkSMDoubleVectorPropertyInternals
{
  vtkstd::vector<double> Values;
  vtkstd::vector<double> UncheckedValues;
};

//---------------------------------------------------------------------------
vtkSMDoubleVectorProperty::vtkSMDoubleVectorProperty()
{
  this->Internals = new vtkSMDoubleVectorPropertyInternals;
  this->ArgumentIsArray = 0;
  this->SetNumberCommand = 0;
}

//---------------------------------------------------------------------------
vtkSMDoubleVectorProperty::~vtkSMDoubleVectorProperty()
{
  delete this->Internals;
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::AppendCommandToStream(
  vtkSMProxy*, vtkClientServerStream* str, vtkClientServerID objectId )
{
  if (!this->Command || this->IsReadOnly)
    {
    return;
    }

  if (this->SetNumberCommand)
    {
    *str << vtkClientServerStream::Invoke 
         << objectId << this->SetNumberCommand << this->GetNumberOfElements()
         << vtkClientServerStream::End;
    }

  if (!this->RepeatCommand)
    {
    *str << vtkClientServerStream::Invoke << objectId << this->Command;
    int numArgs = this->GetNumberOfElements();
    if (this->ArgumentIsArray)
      {
      *str << vtkClientServerStream::InsertArray(
        &(this->Internals->Values[0]), numArgs);
      }
    else
      {
      for(int i=0; i<numArgs; i++)
        {
        *str << this->GetElement(i);
        }
      }
    *str << vtkClientServerStream::End;
    }
  else
    {
    int numArgs = this->GetNumberOfElements();
    int numCommands = numArgs / this->NumberOfElementsPerCommand;
    for(int i=0; i<numCommands; i++)
      {
      *str << vtkClientServerStream::Invoke << objectId << this->Command;
      if (this->UseIndex)
        {
        *str << i;
        }
      if (this->ArgumentIsArray)
        {
        *str << vtkClientServerStream::InsertArray(
          &(this->Internals->Values[i*this->NumberOfElementsPerCommand]),
          this->NumberOfElementsPerCommand);
        }
      else
        {
        for (int j=0; j<this->NumberOfElementsPerCommand; j++)
          {
          *str << this->GetElement(i*this->NumberOfElementsPerCommand+j);
          }
        }
      *str << vtkClientServerStream::End;
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::UpdateInformation( vtkClientServerID objectId )
{
  if (!this->InformationOnly)
    {
    return;
    }

  vtkClientServerStream str;
  str << vtkClientServerStream::Invoke 
      << objectId << this->Command
      << vtkClientServerStream::End;

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  pm->SendStream(vtkProcessModule::DATA_SERVER, str, 0);

  const vtkClientServerStream& res = pm->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT);

  int numMsgs = res.GetNumberOfMessages();
  if (numMsgs < 1)
    {
    return;
    }

  int numArgs = res.GetNumberOfArguments(0);
  if (numArgs < 1)
    {
    return;
    }

  int argType = res.GetArgumentType(0, 0);

  if (argType == vtkClientServerStream::float64_value ||
      argType == vtkClientServerStream::float32_value)
    {
    double ires;
    int retVal = res.GetArgument(0, 0, &ires);
    if (!retVal)
      {
      vtkErrorMacro("Error getting argument.");
      return;
      }
    this->SetNumberOfElements(1);
    this->SetElement(0, ires);
    }
  else if (argType == vtkClientServerStream::float64_array)
    {
    vtkTypeUInt32 length;
    res.GetArgumentLength(0, 0, &length);
    if (length >= 128)
      {
      vtkErrorMacro("Only arguments of length 128 or less are supported");
      return;
      }
    double values[128];
    int retVal = res.GetArgument(0, 0, values, length);
    if (!retVal)
      {
      vtkErrorMacro("Error getting argument.");
      return;
      }
    this->SetNumberOfElements(length);
    this->SetElements(values);
    }
  else if (argType == vtkClientServerStream::float32_array)
    {
    vtkTypeUInt32 length;
    res.GetArgumentLength(0, 0, &length);
    if (length >= 128)
      {
      vtkErrorMacro("Only arguments of length 128 or less are supported");
      return;
      }
    float values[128];
    int retVal = res.GetArgument(0, 0, values, length);
    if (!retVal)
      {
      vtkErrorMacro("Error getting argument.");
      return;
      }
    this->SetNumberOfElements(length);
    for (unsigned int i=0; i<length; i++)
      {
      this->SetElement(i, values[i]);
      }
    }

}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::SetNumberOfUncheckedElements(unsigned int num)
{
  this->Internals->UncheckedValues.resize(num);
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::SetNumberOfElements(unsigned int num)
{
  this->Internals->Values.resize(num);
  this->Internals->UncheckedValues.resize(num);
  this->Modified();
}

//---------------------------------------------------------------------------
unsigned int vtkSMDoubleVectorProperty::GetNumberOfUncheckedElements()
{
  return this->Internals->UncheckedValues.size();
}

//---------------------------------------------------------------------------
unsigned int vtkSMDoubleVectorProperty::GetNumberOfElements()
{
  return this->Internals->Values.size();
}

//---------------------------------------------------------------------------
double vtkSMDoubleVectorProperty::GetElement(unsigned int idx)
{
  return this->Internals->Values[idx];
}

//---------------------------------------------------------------------------
double vtkSMDoubleVectorProperty::GetUncheckedElement(unsigned int idx)
{
  return this->Internals->UncheckedValues[idx];
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::SetUncheckedElement(
  unsigned int idx, double value)
{
  if (idx >= this->GetNumberOfUncheckedElements())
    {
    this->SetNumberOfUncheckedElements(idx+1);
    }
  this->Internals->UncheckedValues[idx] = value;
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElement(unsigned int idx, double value)
{
  if (this->IsReadOnly)
    {
    return 0;
    }

  if ( vtkSMProperty::GetCheckDomains() )
    {
    int numArgs = this->GetNumberOfElements();
    memcpy(&this->Internals->UncheckedValues[0], 
           &this->Internals->Values[0], 
           numArgs*sizeof(double));
    
    this->SetUncheckedElement(idx, value);
    if (!this->IsInDomains())
      {
      this->SetNumberOfUncheckedElements(this->GetNumberOfElements());
      return 0;
      }
    }
  
  if (idx >= this->GetNumberOfElements())
    {
    this->SetNumberOfElements(idx+1);
    }
  this->Internals->Values[idx] = value;
  this->Modified();
  return 1;
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElements1(double value0)
{
  return this->SetElement(0, value0);
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElements2(double value0, double value1)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  return (retVal1 && retVal2);
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElements3(
  double value0, double value1, double value2)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  int retVal3 = this->SetElement(2, value2);
  return (retVal1 && retVal2 && retVal3);
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElements4(
  double value0, double value1, double value2, double value3)
{
  int retVal1 = this->SetElement(0, value0);
  int retVal2 = this->SetElement(1, value1);
  int retVal3 = this->SetElement(2, value2);
  int retVal4 = this->SetElement(3, value3);
  return (retVal1 && retVal2 && retVal3 && retVal4);
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::SetElements(const double* values)
{
  if (this->IsReadOnly)
    {
    return 0;
    }

  int numArgs = this->GetNumberOfElements();

  if ( vtkSMProperty::GetCheckDomains() )
    {
    memcpy(&this->Internals->UncheckedValues[0], 
           values, 
           numArgs*sizeof(double));
    if (!this->IsInDomains())
      {
      return 0;
      }
    }

  memcpy(&this->Internals->Values[0], values, numArgs*sizeof(double));
  this->Modified();
  return 1;
}

//---------------------------------------------------------------------------
int vtkSMDoubleVectorProperty::ReadXMLAttributes(vtkSMProxy* proxy,
                                                 vtkPVXMLElement* element)
{
  int retVal;

  retVal = this->Superclass::ReadXMLAttributes(proxy, element);
  if (!retVal)
    {
    return retVal;
    }

  int arg_is_array;
  retVal = element->GetScalarAttribute("argument_is_array", &arg_is_array);
  if(retVal) 
    { 
    this->SetArgumentIsArray(arg_is_array); 
    }

  const char* numCommand = element->GetAttribute("set_number_command");
  if (numCommand)
    {
    this->SetSetNumberCommand(numCommand);
    }

  int numElems = this->GetNumberOfElements();
  if (numElems > 0)
    {
    double* initVal = new double[numElems];
    int numRead = element->GetVectorAttribute("default_values",
                                              numElems,
                                              initVal);

    if (numRead > 0)
      {
      if (numRead != numElems)
        {
        vtkErrorMacro("The number of default values does not match the number "
                      "of elements. Initialization failed.");
        delete[] initVal;
        return 0;
        }
      this->SetElements(initVal);
      }
    else
      {
      vtkErrorMacro("No default value is specified for property: "
                    << this->GetXMLName()
                    << ". This might lead to stability problems");
      }
    delete[] initVal;
    }
    
  return 1;
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::SaveState(
  const char* name, ostream* file, vtkIndent indent)
{
  unsigned int size = this->GetNumberOfElements();
  *file << indent << "<Property name=\"" << (this->XMLName?this->XMLName:"")
        << "\" id=\"" << name << "\" ";
  if (size > 0)
    {
    *file << "number_of_elements=\"" << size << "\"";
    }
  *file << ">" << endl;
  for (unsigned int i=0; i<size; i++)
    {
    *file << indent.GetNextIndent() << "<Element index=\""
          << i << "\" " << "value=\"" << this->GetElement(i) << "\"/>"
          << endl;
    }
  this->Superclass::SaveState(name, file, indent);
  *file << indent << "</Property>" << endl;
}

//---------------------------------------------------------------------------
void vtkSMDoubleVectorProperty::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "ArgumentIsArray: " << this->ArgumentIsArray << endl;

  os << indent << "Values: ";
  for (unsigned int i=0; i<this->GetNumberOfElements(); i++)
    {
    os << this->GetElement(i) << " ";
    }
  os << endl;
}
