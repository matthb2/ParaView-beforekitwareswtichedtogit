/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

// self includes
#include "pqSMAdaptor.h"

// qt includes
#include <QString>
#include <QVariant>

// vtk includes
#include "vtkConfigure.h"   // for 64-bitness

// server manager includes
#include "vtkSMArrayListDomain.h"
#include "vtkSMBooleanDomain.h"
#include "vtkSMBoundsDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMDoubleRangeDomain.h"
#include "vtkSMArrayRangeDomain.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMEnumerationDomain.h"
#include "vtkSMIdTypeVectorProperty.h"
#include "vtkSMInputProperty.h"
#include "vtkSMIntRangeDomain.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMPropertyAdaptor.h"
#include "vtkSMProperty.h"
#include "vtkSMProxyGroupDomain.h"
#include "vtkSMProxyGroupDomain.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyListDomain.h"
#include "vtkSMProxyManager.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringListDomain.h"
#include "vtkSMStringListRangeDomain.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMVectorProperty.h"
#include "vtkSMExtentDomain.h"

// ParaView includes
#include "pqSMProxy.h"

pqSMAdaptor::pqSMAdaptor()
{
}

pqSMAdaptor::~pqSMAdaptor()
{
}

pqSMAdaptor::PropertyType pqSMAdaptor::getPropertyType(vtkSMProperty* Property)
{

  pqSMAdaptor::PropertyType type = pqSMAdaptor::UNKNOWN;
  if(!Property)
    {
    return type;
    }

  vtkSMProxyProperty* proxy = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxy)
    {
    vtkSMInputProperty* input = vtkSMInputProperty::SafeDownCast(Property);
    if(input && input->GetMultipleInput())
      {
      type = pqSMAdaptor::PROXYLIST;
      }
    type = pqSMAdaptor::PROXY;
    if (vtkSMProxyListDomain::SafeDownCast(Property->GetDomain("proxy_list")))
      {
      type = pqSMAdaptor::PROXYSELECTION;
      }
    }
  else if(Property->GetDomain("field_list"))
    {
    type = pqSMAdaptor::FIELD_SELECTION;
    }
  else
    {
    vtkSMPropertyAdaptor* adaptor = vtkSMPropertyAdaptor::New();
    adaptor->SetProperty(Property);

    if(adaptor->GetPropertyType() == vtkSMPropertyAdaptor::SELECTION)
      {
      type = pqSMAdaptor::SELECTION;
      }
    else if(adaptor->GetPropertyType() == vtkSMPropertyAdaptor::ENUMERATION)
      {
      type = pqSMAdaptor::ENUMERATION;
      }
    else if(adaptor->GetPropertyType() == vtkSMPropertyAdaptor::FILE_LIST)
      {
      type = pqSMAdaptor::FILE_LIST;
      }
    else 
      {
      vtkSMVectorProperty* VectorProperty;
      VectorProperty = vtkSMVectorProperty::SafeDownCast(Property);

      Q_ASSERT(VectorProperty != NULL);
      if(VectorProperty && 
        (VectorProperty->GetNumberOfElements() > 1 || VectorProperty->GetRepeatCommand()))
        {
        type = pqSMAdaptor::MULTIPLE_ELEMENTS;
        }
      else if(VectorProperty && VectorProperty->GetNumberOfElements() == 1)
        {
        type = pqSMAdaptor::SINGLE_ELEMENT;
        }
      }
    adaptor->Delete();
    }

  return type;
}

pqSMProxy pqSMAdaptor::getProxyProperty(vtkSMProperty* Property)
{
  pqSMAdaptor::PropertyType type = pqSMAdaptor::getPropertyType(Property);
  if( type == pqSMAdaptor::PROXY || type == pqSMAdaptor::PROXYSELECTION)
    {
    vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
    if(proxyProp->GetNumberOfProxies())
      {
      return pqSMProxy(proxyProp->GetProxy(0));
      }
    else
      {
      // TODO fix this -- we should do this automatically ??
      // no proxy property defined and one is required, so go find one to set
      QList<pqSMProxy> domain;
      domain = pqSMAdaptor::getProxyPropertyDomain(Property);
      if(domain.size())
        {
        //pqSMAdaptor::setProxyProperty(Property, domain[0]);
        return domain[0];
        }
      }
    }
  return pqSMProxy(NULL);
}

void pqSMAdaptor::removeProxyProperty(vtkSMProperty* Property, pqSMProxy Value)
{
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxyProp)
    {
    proxyProp->RemoveProxy(Value);
    }
}

void pqSMAdaptor::addProxyProperty(vtkSMProperty* Property, 
                                   pqSMProxy Value)
{
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxyProp)
    {
    proxyProp->AddProxy(Value);
    }
}

void pqSMAdaptor::setProxyProperty(vtkSMProperty* Property, 
                                   pqSMProxy Value)
{
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxyProp)
    {
    if (proxyProp->GetNumberOfProxies() == 1)
      {
      proxyProp->SetProxy(0, Value);
      }
    else
      {
      proxyProp->RemoveAllProxies();
      proxyProp->AddProxy(Value);
      }
    }
}

void pqSMAdaptor::setUncheckedProxyProperty(vtkSMProperty* Property,
                                   pqSMProxy Value)
{
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxyProp)
    {
    proxyProp->RemoveAllUncheckedProxies();
    proxyProp->AddUncheckedProxy(Value);
    proxyProp->UpdateDependentDomains();
    }
}

QList<pqSMProxy> pqSMAdaptor::getProxyListProperty(vtkSMProperty* Property)
{
  QList<pqSMProxy> value;
  if(pqSMAdaptor::getPropertyType(Property) == pqSMAdaptor::PROXYLIST)
    {
    vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
    unsigned int num = proxyProp->GetNumberOfProxies();
    for(unsigned int i=0; i<num; i++)
      {
      value.append(proxyProp->GetProxy(i));
      }
    }
  return value;
}

void pqSMAdaptor::setProxyListProperty(vtkSMProperty* Property, 
                                       QList<pqSMProxy> Value)
{
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if (proxyProp)
    {
    proxyProp->RemoveAllProxies();
    foreach(pqSMProxy p, Value)
      {
      proxyProp->AddProxy(p);
      }
    }
}

QList<pqSMProxy> pqSMAdaptor::getProxyPropertyDomain(vtkSMProperty* Property)
{
  QList<pqSMProxy> proxydomain;
  vtkSMProxyProperty* proxyProp = vtkSMProxyProperty::SafeDownCast(Property);
  if(proxyProp)
    {
    vtkSMProxyManager* pm = vtkSMProxyManager::GetProxyManager();
    
    // get group domain of this property 
    // and add all proxies in those groups to our list
    vtkSMProxyGroupDomain* gd;
    vtkSMProxyListDomain* ld;
    ld = vtkSMProxyListDomain::SafeDownCast(Property->GetDomain("proxy_list"));
    gd = vtkSMProxyGroupDomain::SafeDownCast(Property->GetDomain("groups"));
    if (ld)
      {
      unsigned int numProxies = ld->GetNumberOfProxies();
      for (unsigned int cc=0; cc < numProxies; cc++)
        {
        proxydomain.append(ld->GetProxy(cc));
        }
      }
    else if (gd)
      {
      unsigned int numGroups = gd->GetNumberOfGroups();
      for(unsigned int i=0; i<numGroups; i++)
        {
        const char* group = gd->GetGroup(i);
        unsigned int numProxies = pm->GetNumberOfProxies(group);
        for(unsigned int j=0; j<numProxies; j++)
          {
          const char *name = pm->GetProxyName(group, j);
          if(!name)
            {
            continue;
            }
          pqSMProxy p = pm->GetProxy(group, name);
          proxydomain.append(p);
          }
        }
      }
    }
  return proxydomain;
}


QList<QList<QVariant> > pqSMAdaptor::getSelectionProperty(vtkSMProperty* Property)
{
  QList<QList<QVariant> > ret;
  
  vtkSMStringVectorProperty* StringProperty = NULL;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(StringProperty)
    {
    int numSelections = 0;
    numSelections = StringProperty->GetNumberOfElements() / 2;

    for(int i=0; i<numSelections; i++)
      {
      QList<QVariant> tmp;
      tmp = pqSMAdaptor::getSelectionProperty(Property, i);
      ret.append(tmp);
      }
    }

  return ret;
}

QList<QVariant> pqSMAdaptor::getSelectionProperty(vtkSMProperty* Property, 
                                                  unsigned int Index)
{
  QList<QVariant> ret;
  vtkSMStringVectorProperty* StringProperty = NULL;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);
  vtkSMStringListRangeDomain* StringDomain = NULL;

  if(StringProperty)
    {
    vtkSMDomainIterator* iter = Property->NewDomainIterator();
    iter->Begin();
    while(StringDomain == NULL && !iter->IsAtEnd())
      {
      vtkSMDomain* d = iter->GetDomain();
      StringDomain = vtkSMStringListRangeDomain::SafeDownCast(d);
      iter->Next();
      }
    iter->Delete();
    }
  
  if(StringProperty && StringDomain)
    {
    QString StringName = StringDomain->GetString(Index);
    if(!StringName.isNull())
      {
      ret.append(StringName);
      QVariant value;

      int numElements = StringProperty->GetNumberOfElements();
      if(numElements % 2 == 0)
        {
        for(int i=0; i<numElements; i+=2)
          {
          if(StringName == StringProperty->GetElement(i))
            {
            value = StringProperty->GetElement(i+1);
            break;
            }
          }
        }
      // make up a zero
      if(!value.isValid())
        {
        qWarning("had to make up a value for selection\n");
        value = "0";
        }
      if(StringDomain->GetIntDomainMode() ==
         vtkSMStringListRangeDomain::BOOLEAN)
        {
        value.convert(QVariant::Bool);
        }
      ret.append(value);
      }
    }

  return ret;
}

void pqSMAdaptor::setSelectionProperty(vtkSMProperty* Property, 
                                   QList<QList<QVariant> > Value)
{
  foreach(QList<QVariant> l, Value)
    {
    pqSMAdaptor::setSelectionProperty(Property, l);
    }
  
  vtkSMStringVectorProperty* StringProperty;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);
  if(StringProperty)
    {
    StringProperty->SetNumberOfElements(Value.size() * 2);
    }
}

void pqSMAdaptor::setUncheckedSelectionProperty(vtkSMProperty* Property,
                                  QList<QList<QVariant> > Value)
{
  foreach(QList<QVariant> l, Value)
    {
    pqSMAdaptor::setUncheckedSelectionProperty(Property, l);
    }
}

void pqSMAdaptor::setSelectionProperty(vtkSMProperty* Property, 
                                       QList<QVariant> Value)
{
  vtkSMStringVectorProperty* StringProperty;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);
  if(StringProperty && Value.size() == 2)
    {
    vtkSMStringListRangeDomain* StringDomain = NULL;
    vtkSMDomainIterator* iter = Property->NewDomainIterator();
    iter->Begin();
    while(StringDomain == NULL && !iter->IsAtEnd())
      {
      vtkSMDomain* d = iter->GetDomain();
      StringDomain = vtkSMStringListRangeDomain::SafeDownCast(d);
      iter->Next();
      }
    iter->Delete();

    if(StringDomain)
      {
      QString name = Value[0].toString();
      QVariant value = Value[1];
      if(value.type() == QVariant::Bool)
        {
        value = value.toInt();
        }
      QString valueStr = value.toString();
      unsigned int numElems;
      numElems = StringProperty->GetNumberOfElements();
      if (numElems % 2 != 0)
        {
        return;
        }
      unsigned int i;
      for(i=0; i<numElems; i+=2)
        {
        if(name == StringProperty->GetElement(i))
          {
          StringProperty->SetElement(i+1, valueStr.toAscii().data());
          return;
          }
        }
      // not found, just put it in the first empty slot
      for(i=0; i<numElems; i+=2)
        {
        const char* elem = StringProperty->GetElement(i);
        if(!elem || elem[0] == '\0')
          {
          StringProperty->SetElement(i, name.toAscii().data());
          StringProperty->SetElement(i+1, valueStr.toAscii().data());
          return;
          }
        }
      // If we didn't find any empty spots, append to the vector
      StringProperty->SetElement(numElems, name.toAscii().data());
      StringProperty->SetElement(numElems+1, valueStr.toAscii().data());
      return;
      }
    }
}

void pqSMAdaptor::setUncheckedSelectionProperty(vtkSMProperty* Property,
                                                QList<QVariant> Value)
{
  vtkSMStringVectorProperty* StringProperty;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);
  if(StringProperty && Value.size() == 2)
    {
    vtkSMStringListRangeDomain* StringDomain = NULL;
    vtkSMDomainIterator* iter = Property->NewDomainIterator();
    iter->Begin();
    while(StringDomain == NULL && !iter->IsAtEnd())
      {
      vtkSMDomain* d = iter->GetDomain();
      StringDomain = vtkSMStringListRangeDomain::SafeDownCast(d);
      iter->Next();
      }
    iter->Delete();

    if(StringDomain)
      {
      QString name = Value[0].toString();
      QVariant value = Value[1];
      if(value.type() == QVariant::Bool)
        {
        value = value.toInt();
        }
      QString valueStr = value.toString();
      unsigned int numElems;
      numElems = StringProperty->GetNumberOfUncheckedElements();
      if (numElems % 2 != 0)
        {
        return;
        }
      unsigned int i;
      for(i=0; i<numElems; i+=2)
        {
        if(name == StringProperty->GetUncheckedElement(i))
          {
          StringProperty->SetUncheckedElement(i+1, valueStr.toAscii().data());
          Property->UpdateDependentDomains();
          return;
          }
        }
      // not found, just put it in the first empty slot
      for(i=0; i<numElems; i+=2)
        {
        const char* elem = StringProperty->GetUncheckedElement(i);
        if(!elem || elem[0] == '\0')
          {
          StringProperty->SetUncheckedElement(i, name.toAscii().data());
          StringProperty->SetUncheckedElement(i+1, valueStr.toAscii().data());
          Property->UpdateDependentDomains();
          return;
          }
        }
      // If we didn't find any empty spots, append to the vector
      StringProperty->SetUncheckedElement(numElems, name.toAscii().data());
      StringProperty->SetUncheckedElement(numElems+1, valueStr.toAscii().data());
      Property->UpdateDependentDomains();
      return;
      }
    }
}

QList<QVariant> pqSMAdaptor::getSelectionPropertyDomain(vtkSMProperty* Property)
{
  QList<QVariant> ret;
  
  vtkSMStringVectorProperty* StringProperty;
  StringProperty = vtkSMStringVectorProperty::SafeDownCast(Property);
  if(StringProperty)
    {
    vtkSMStringListRangeDomain* StringDomain = NULL;
    vtkSMDomainIterator* iter = Property->NewDomainIterator();
    iter->Begin();
    while(StringDomain == NULL && !iter->IsAtEnd())
      {
      vtkSMDomain* d = iter->GetDomain();
      StringDomain = vtkSMStringListRangeDomain::SafeDownCast(d);
      iter->Next();
      }
    iter->Delete();

    if(StringDomain)
      {
      int num = StringDomain->GetNumberOfStrings();
      for(int i=0; i<num; i++)
        {
        ret.append(StringDomain->GetString(i));
        }
      }
    }
  return ret;
}
  
QVariant pqSMAdaptor::getEnumerationProperty(vtkSMProperty* Property)
{
  QVariant var;

  vtkSMBooleanDomain* BooleanDomain = NULL;
  vtkSMEnumerationDomain* EnumerationDomain = NULL;
  vtkSMStringListDomain* StringListDomain = NULL;
  vtkSMProxyGroupDomain* ProxyGroupDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd())
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!BooleanDomain)
      {
      BooleanDomain = vtkSMBooleanDomain::SafeDownCast(d);
      }
    if(!EnumerationDomain)
      {
      EnumerationDomain = vtkSMEnumerationDomain::SafeDownCast(d);
      }
    if(!StringListDomain)
      {
      StringListDomain = vtkSMStringListDomain::SafeDownCast(d);
      }
    if(!ProxyGroupDomain)
      {
      ProxyGroupDomain = vtkSMProxyGroupDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();
  
  vtkSMIntVectorProperty* ivp;
  vtkSMStringVectorProperty* svp;
  vtkSMProxyProperty* pp;
  
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);
  pp = vtkSMProxyProperty::SafeDownCast(Property);

  if(BooleanDomain && ivp && ivp->GetNumberOfElements() > 0)
    {
    var = (ivp->GetElement(0)) == 0 ? false : true;
    }
  else if(EnumerationDomain && ivp && ivp->GetNumberOfElements() > 0)
    {
    // Some vtkSMIntVectorProperty with enumeration domains
    // may have repeat_command="1". In which case the value
    // is expected to be a list of values.
    if (ivp->GetRepeatCommand())
      {
       QList<QVariant> list = pqSMAdaptor::getMultipleElementProperty(ivp);
       QList<QVariant> values;
       foreach (QVariant value, list)
         {
         int val = value.toInt();
         for (unsigned int i=0; i<EnumerationDomain->GetNumberOfEntries(); i++)
           {
           if (EnumerationDomain->GetEntryValue(i) == val)
             {
             values.push_back(EnumerationDomain->GetEntryText(i));
             break;
             }
           }
         }
       var = values;
      }
    else
      {
      int val = ivp->GetElement(0);
      for (unsigned int i=0; i<EnumerationDomain->GetNumberOfEntries(); i++)
        {
        if (EnumerationDomain->GetEntryValue(i) == val)
          {
          var = EnumerationDomain->GetEntryText(i);
          break;
          }
        }
      }
    }
  else if(StringListDomain && svp)
    {
    // If repeat command is set, then a value is a list of QVariants
    // and each QVariant is added to the property.
    if (svp->GetRepeatCommand())
      {
      var = pqSMAdaptor::getMultipleElementProperty(svp);
      }
    else
      {
      unsigned int nos = svp->GetNumberOfElements();
      for (unsigned int i=0; i < nos ; i++)
        {
        if (svp->GetElementType(i) == vtkSMStringVectorProperty::STRING)
          {
          var = svp->GetElement(i);
          break;
          }
        }
      }
    }
  else if (ProxyGroupDomain && pp && pp->GetNumberOfProxies() > 0)
    {
    vtkSMProxy* p = pp->GetProxy(0);
    var = ProxyGroupDomain->GetProxyName(p);
    }

  return var;
}

void pqSMAdaptor::setEnumerationProperty(vtkSMProperty* Property,
                                         QVariant Value)
{
  // TODO:  need to handle array lists?
  vtkSMBooleanDomain* BooleanDomain = NULL;
  vtkSMEnumerationDomain* EnumerationDomain = NULL;
  vtkSMStringListDomain* StringListDomain = NULL;
  vtkSMProxyGroupDomain* ProxyGroupDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd())
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!BooleanDomain)
      {
      BooleanDomain = vtkSMBooleanDomain::SafeDownCast(d);
      }
    if(!EnumerationDomain)
      {
      EnumerationDomain = vtkSMEnumerationDomain::SafeDownCast(d);
      }
    if(!StringListDomain)
      {
      StringListDomain = vtkSMStringListDomain::SafeDownCast(d);
      }
    if(!ProxyGroupDomain)
      {
      ProxyGroupDomain = vtkSMProxyGroupDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();
  
  vtkSMIntVectorProperty* ivp;
  vtkSMStringVectorProperty* svp;
  vtkSMProxyProperty* pp;
  
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);
  pp = vtkSMProxyProperty::SafeDownCast(Property);

  if(BooleanDomain && ivp && ivp->GetNumberOfElements() > 0)
    {
    bool ok = true;
    int v = Value.toInt(&ok);
    if(ok)
      {
      ivp->SetElement(0, v);
      }
    }
  else if(EnumerationDomain && ivp)
    {
    // Some vtkSMIntVectorProperty with enumeration domains
    // may have repeat_command="1". In which case the value
    // is expected to be a list of values.
    if (ivp->GetRepeatCommand())
      {
      QList<QVariant> values = Value.toList();
      QList<QVariant> domainStrings = pqSMAdaptor::getEnumerationPropertyDomain(ivp);
      QList<QVariant> actualValues;
      foreach (QVariant val, values)
        {
        int index = domainStrings.indexOf(val);
        if (index != -1)
          {
          actualValues << EnumerationDomain->GetEntryValue(index);
          }
        }
      pqSMAdaptor::setMultipleElementProperty(Property, actualValues);
      }
    else
      {
      QString str = Value.toString();
      unsigned int numEntries = EnumerationDomain->GetNumberOfEntries();
      for(unsigned int i=0; i<numEntries; i++)
        {
        if(str == EnumerationDomain->GetEntryText(i))
          {
          ivp->SetElement(0, EnumerationDomain->GetEntryValue(i));
          }
        }
      }
    }
  else if(StringListDomain && svp)
    {
    // If repeat command is set, then a value is a list of QVariants
    // and each QVariant is added to the property.
    if (svp->GetRepeatCommand())
      {
      pqSMAdaptor::setMultipleElementProperty(svp, Value.toList());
      }
    else
      {
      unsigned int nos = svp->GetNumberOfElements();
      for (unsigned int i=0; i < nos ; i++)
        {
        if (svp->GetElementType(i) == vtkSMStringVectorProperty::STRING)
          {
          svp->SetElement(i, Value.toString().toAscii().data());
          }
        }
      }
    }
  else if (ProxyGroupDomain && pp)
    {
    QString str = Value.toString();
    vtkSMProxy* toadd = ProxyGroupDomain->GetProxy(str.toAscii().data());
    if (pp->GetNumberOfProxies() < 1)
      {
      pp->AddProxy(toadd);
      }
    else
      {
      pp->SetProxy(0, toadd);
      }
    }
}

void pqSMAdaptor::setUncheckedEnumerationProperty(vtkSMProperty* Property,
                                                  QVariant Value)
{
  vtkSMBooleanDomain* BooleanDomain = NULL;
  vtkSMEnumerationDomain* EnumerationDomain = NULL;
  vtkSMStringListDomain* StringListDomain = NULL;
  vtkSMProxyGroupDomain* ProxyGroupDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd())
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!BooleanDomain)
      {
      BooleanDomain = vtkSMBooleanDomain::SafeDownCast(d);
      }
    if(!EnumerationDomain)
      {
      EnumerationDomain = vtkSMEnumerationDomain::SafeDownCast(d);
      }
    if(!StringListDomain)
      {
      StringListDomain = vtkSMStringListDomain::SafeDownCast(d);
      }
    if(!ProxyGroupDomain)
      {
      ProxyGroupDomain = vtkSMProxyGroupDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();
  
  vtkSMIntVectorProperty* ivp;
  vtkSMStringVectorProperty* svp;
  vtkSMProxyProperty* pp;
  
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);
  pp = vtkSMProxyProperty::SafeDownCast(Property);

  if(BooleanDomain && ivp && ivp->GetNumberOfElements() > 0)
    {
    bool ok = true;
    int v = Value.toInt(&ok);
    if(ok)
      {
      ivp->SetUncheckedElement(0, v);
      Property->UpdateDependentDomains();
      }
    }
  else if(EnumerationDomain && ivp && ivp->GetNumberOfElements() > 0)
    {
    QString str = Value.toString();
    unsigned int numEntries = EnumerationDomain->GetNumberOfEntries();
    for(unsigned int i=0; i<numEntries; i++)
      {
      if(str == EnumerationDomain->GetEntryText(i))
        {
        ivp->SetUncheckedElement(0, EnumerationDomain->GetEntryValue(i));
        Property->UpdateDependentDomains();
        }
      }
    }
  else if(StringListDomain && svp)
    {
    // If repeat command is set, then a value is a list of QVariants
    // and each QVariant is added to the property.
    if (svp->GetRepeatCommand())
      {
      pqSMAdaptor::setUncheckedMultipleElementProperty(svp, Value.toList());
      }
    else
      {
      unsigned int nos = svp->GetNumberOfElements();
      for (unsigned int i=0; i < nos ; i++)
        {
        if (svp->GetElementType(i) == vtkSMStringVectorProperty::STRING)
          {
          svp->SetUncheckedElement(i, Value.toString().toAscii().data());
          }
        }
      Property->UpdateDependentDomains();
      }
    }
  else if (ProxyGroupDomain && pp)
    {
    QString str = Value.toString();
    vtkSMProxy* toadd = ProxyGroupDomain->GetProxy(str.toAscii().data());
    if (pp->GetNumberOfUncheckedProxies() < 1)
      {
      pp->AddUncheckedProxy(toadd);
      Property->UpdateDependentDomains();
      }
    else
      {
      pp->SetUncheckedProxy(0, toadd);
      Property->UpdateDependentDomains();
      }
    }

}

QList<QVariant> pqSMAdaptor::getEnumerationPropertyDomain(
                                          vtkSMProperty* Property)
{
  QList<QVariant> enumerations;

  vtkSMBooleanDomain* BooleanDomain = NULL;
  vtkSMEnumerationDomain* EnumerationDomain = NULL;
  vtkSMStringListDomain* StringListDomain = NULL;
  vtkSMProxyGroupDomain* ProxyGroupDomain = NULL;
  vtkSMArrayListDomain* ArrayListDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd())
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!BooleanDomain)
      {
      BooleanDomain = vtkSMBooleanDomain::SafeDownCast(d);
      }
    if(!EnumerationDomain)
      {
      EnumerationDomain = vtkSMEnumerationDomain::SafeDownCast(d);
      }
    if(!StringListDomain)
      {
      StringListDomain = vtkSMStringListDomain::SafeDownCast(d);
      }
    if(!ArrayListDomain)
      {
      ArrayListDomain = vtkSMArrayListDomain::SafeDownCast(d);
      }
    if(!ProxyGroupDomain)
      {
      ProxyGroupDomain = vtkSMProxyGroupDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();

  if(BooleanDomain)
    {
    enumerations.push_back(false);
    enumerations.push_back(true);
    }
  else if(ArrayListDomain)
    {
    unsigned int numEntries = ArrayListDomain->GetNumberOfStrings();
    for(unsigned int i=0; i<numEntries; i++)
      {
      enumerations.push_back(ArrayListDomain->GetString(i));
      }
    }
  else if(EnumerationDomain)
    {
    unsigned int numEntries = EnumerationDomain->GetNumberOfEntries();
    for(unsigned int i=0; i<numEntries; i++)
      {
      enumerations.push_back(EnumerationDomain->GetEntryText(i));
      }
    }
  else if(ProxyGroupDomain)
    {
    unsigned int numEntries = ProxyGroupDomain->GetNumberOfProxies();
    for(unsigned int i=0; i<numEntries; i++)
      {
      enumerations.push_back(ProxyGroupDomain->GetProxyName(i));
      }
    }
  else if(StringListDomain)
    {
    unsigned int numEntries = StringListDomain->GetNumberOfStrings();
    for(unsigned int i=0; i<numEntries; i++)
      {
      enumerations.push_back(StringListDomain->GetString(i));
      }
    }
  
  return enumerations;
}

QVariant pqSMAdaptor::getElementProperty(vtkSMProperty* Property)
{
  return pqSMAdaptor::getMultipleElementProperty(Property, 0);
}

void pqSMAdaptor::setElementProperty(vtkSMProperty* Property, QVariant Value)
{
  pqSMAdaptor::setMultipleElementProperty(Property, 0, Value);
}

void pqSMAdaptor::setUncheckedElementProperty(vtkSMProperty* Property, 
                                              QVariant Value)
{
  pqSMAdaptor::setUncheckedMultipleElementProperty(Property, 0, Value);
}

QList<QVariant> pqSMAdaptor::getElementPropertyDomain(vtkSMProperty* Property)
{
  return pqSMAdaptor::getMultipleElementPropertyDomain(Property, 0);
}
  
QList<QVariant> pqSMAdaptor::getMultipleElementProperty(vtkSMProperty* Property)
{
  QList<QVariant> props;
  
  vtkSMVectorProperty* VectorProperty;
  VectorProperty = vtkSMVectorProperty::SafeDownCast(Property);
  if(!VectorProperty)
    {
    return props;
    }

  int i;
  int num = VectorProperty->GetNumberOfElements();
  for(i=0; i<num; i++)
    {
    props.push_back(
       pqSMAdaptor::getMultipleElementProperty(Property, i)
       );
    }

  return props;
}

void pqSMAdaptor::setMultipleElementProperty(vtkSMProperty* Property, 
                                             QList<QVariant> Value)
{
  vtkSMDoubleVectorProperty* dvp;
  vtkSMIntVectorProperty* ivp;
  vtkSMIdTypeVectorProperty* idvp;
  vtkSMStringVectorProperty* svp;
  
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  idvp = vtkSMIdTypeVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  int num = Value.size();

  if(dvp)
    {
    double *dvalues = new double[num+1];
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      double v = Value[i].toDouble(&ok);
      dvalues[i] = ok? v : 0.0;
      }
    dvp->SetNumberOfElements(num);
    if (num > 0)
      {
      dvp->SetElements(dvalues);
      }
    delete[] dvalues;
    }
  else if(ivp)
    {
    int *ivalues = new int[num+1];
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      int v = Value[i].toInt(&ok);
      ivalues[i] = ok? v : 0;
      }
    ivp->SetNumberOfElements(num);
    if (num>0)
      {
      ivp->SetElements(ivalues);
      }
    delete []ivalues;
    }
  else if(svp)
    {
    for(int i=0; i<num; i++)
      {
      QString v = Value[i].toString();
      if(!v.isNull())
        {
        svp->SetElement(i, v.toAscii().data());
        }
      }
    svp->SetNumberOfElements(num);
    }
  else if(idvp)
    {
    vtkIdType* idvalues = new vtkIdType[num+1];
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      vtkIdType v;
#if defined(VTK_USE_64BIT_IDS)
      v = Value[i].toLongLong(&ok);
#else
      v = Value[i].toInt(&ok);
#endif
      idvalues[i] = ok? v : 0;
      }
    idvp->SetNumberOfElements(num);
    if (num>0)
      {
      idvp->SetElements(idvalues);
      }
    delete[] idvalues;
    }
}

void pqSMAdaptor::setUncheckedMultipleElementProperty(vtkSMProperty* Property,
                                                      QList<QVariant> Value)
{
  vtkSMDoubleVectorProperty* dvp;
  vtkSMIntVectorProperty* ivp;
  vtkSMIdTypeVectorProperty* idvp;
  vtkSMStringVectorProperty* svp;
  
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  idvp = vtkSMIdTypeVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);
  
  int num = Value.size();

  if(dvp)
    {
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      double v = Value[i].toDouble(&ok);
      if(ok)
        {
        dvp->SetUncheckedElement(i, v);
        }
      }
    }
  else if(ivp)
    {
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      int v = Value[i].toInt(&ok);
      if(ok)
        {
        ivp->SetUncheckedElement(i, v);
        }
      }
    }
  else if(svp)
    {
    for(int i=0; i<num; i++)
      {
      QString v = Value[i].toString();
      if(!v.isNull())
        {
        svp->SetUncheckedElement(i, v.toAscii().data());
        }
      }
    }
  else if(idvp)
    {
    for(int i=0; i<num; i++)
      {
      bool ok = true;
      vtkIdType v;
#if defined(VTK_USE_64BIT_IDS)
      v = Value[i].toLongLong(&ok);
#else
      v = Value[i].toInt(&ok);
#endif
      if(ok)
        {
        idvp->SetUncheckedElement(i, v);
        }
      }
    }
  Property->UpdateDependentDomains();
}

QList<QList<QVariant> > pqSMAdaptor::getMultipleElementPropertyDomain(
                           vtkSMProperty* Property)
{
  QList< QList<QVariant> > domains;
  
  vtkSMDoubleRangeDomain* DoubleDomain = NULL;
  vtkSMIntRangeDomain* IntDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd() && !DoubleDomain && !IntDomain)
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!DoubleDomain)
      {
      DoubleDomain = vtkSMDoubleRangeDomain::SafeDownCast(d);
      }
    if(!IntDomain)
      {
      IntDomain = vtkSMIntRangeDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();

  if(DoubleDomain)
    {
    vtkSMDoubleVectorProperty* dvp;
    dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
    vtkSMArrayRangeDomain* arrayDomain;
    arrayDomain = vtkSMArrayRangeDomain::SafeDownCast(DoubleDomain);

    unsigned int numElems = dvp->GetNumberOfElements();
    for(unsigned int i=0; i<numElems; i++)
      {
      QList<QVariant> domain;
      int exists1, exists2;
      int which = i;
      if(arrayDomain)
        {
        which = 0;
        }
      double min = DoubleDomain->GetMinimum(which, exists1);
      double max = DoubleDomain->GetMaximum(which, exists2);
      if(exists1 && exists2)  // what if one of them exists?
        {
        domain.push_back(min);
        domain.push_back(max);
        }
      domains.push_back(domain);
      }
    }
  else if(IntDomain)
    {
    vtkSMIntVectorProperty* ivp;
    ivp = vtkSMIntVectorProperty::SafeDownCast(Property);

    unsigned int numElems = ivp->GetNumberOfElements();
    vtkSMExtentDomain* extDomain = vtkSMExtentDomain::SafeDownCast(IntDomain);
    
    for(unsigned int i=0; i<numElems; i++)
      {
      int which = i;
      if(extDomain)
        {
        which /= 2;
        }
      else
        {  
        // one min/max for all elements
        which = 0;
        }
      QList<QVariant> domain;
      int exists1, exists2;
      int min = IntDomain->GetMinimum(which, exists1);
      int max = IntDomain->GetMaximum(which, exists2);
      if(exists1 && exists2)  // what if one of them exists?
        {
        domain.push_back(min);
        domain.push_back(max);
        }
      domains.push_back(domain);
      }
    }

  return domains;
}

QVariant pqSMAdaptor::getMultipleElementProperty(vtkSMProperty* Property,
                                                 unsigned int Index)
{
  QVariant var;
  
  vtkSMDoubleVectorProperty* dvp = NULL;
  vtkSMIntVectorProperty* ivp = NULL;
  vtkSMIdTypeVectorProperty* idvp = NULL;
  vtkSMStringVectorProperty* svp = NULL;

  dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  idvp = vtkSMIdTypeVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(dvp && dvp->GetNumberOfElements() > Index)
    {
    var = dvp->GetElement(Index);
    }
  else if(ivp && ivp->GetNumberOfElements() > Index)
    {
    var = ivp->GetElement(Index);
    }
  else if(svp && svp->GetNumberOfElements() > Index)
    {
    var = svp->GetElement(Index);
    }
  else if(idvp && idvp->GetNumberOfElements() > Index)
    {
    var = idvp->GetElement(Index);
    }
  
  return var;
}

void pqSMAdaptor::setMultipleElementProperty(vtkSMProperty* Property, 
                                             unsigned int Index,
                                             QVariant Value)
{
  vtkSMDoubleVectorProperty* dvp;
  vtkSMIntVectorProperty* ivp;
  vtkSMIdTypeVectorProperty* idvp;
  vtkSMStringVectorProperty* svp;
  
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  idvp = vtkSMIdTypeVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(dvp)
    {
    bool ok = true;
    double v = Value.toDouble(&ok);
    if(ok)
      {
      dvp->SetElement(Index, v);
      }
    }
  else if(ivp)
    {
    bool ok = true;
    int v = Value.toInt(&ok);
    if(ok)
      {
      ivp->SetElement(Index, v);
      }
    }
  else if(svp)
    {
    QString v = Value.toString();
    if(!v.isNull())
      {
      svp->SetElement(Index, v.toAscii().data());
      }
    }
  else if(idvp)
    {
    bool ok = true;
    vtkIdType v;
#if defined(VTK_USE_64BIT_IDS)
    v = Value.toLongLong(&ok);
#else
    v = Value.toInt(&ok);
#endif
    if(ok)
      {
      idvp->SetElement(Index, v);
      }
    }
}

void pqSMAdaptor::setUncheckedMultipleElementProperty(vtkSMProperty* Property, 
                                     unsigned int Index, QVariant Value)
{
  vtkSMDoubleVectorProperty* dvp;
  vtkSMIntVectorProperty* ivp;
  vtkSMIdTypeVectorProperty* idvp;
  vtkSMStringVectorProperty* svp;
  
  dvp = vtkSMDoubleVectorProperty::SafeDownCast(Property);
  ivp = vtkSMIntVectorProperty::SafeDownCast(Property);
  idvp = vtkSMIdTypeVectorProperty::SafeDownCast(Property);
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(dvp && dvp->GetNumberOfElements() > Index)
    {
    bool ok = true;
    double v = Value.toDouble(&ok);
    if(ok)
      {
      dvp->SetUncheckedElement(Index, v);
      }
    }
  else if(ivp && ivp->GetNumberOfElements() > Index)
    {
    bool ok = true;
    int v = Value.toInt(&ok);
    if(ok)
      {
      ivp->SetUncheckedElement(Index, v);
      }
    }
  else if(svp && svp->GetNumberOfElements() > Index)
    {
    QString v = Value.toString();
    if(!v.isNull())
      {
      svp->SetUncheckedElement(Index, v.toAscii().data());
      }
    }
  else if(idvp && idvp->GetNumberOfElements() > Index)
    {
    bool ok = true;
    vtkIdType v;
#if defined(VTK_USE_64BIT_IDS)
    v = Value.toLongLong(&ok);
#else
    v = Value.toInt(&ok);
#endif
    if(ok)
      {
      idvp->SetUncheckedElement(Index, v);
      }
    }
  Property->UpdateDependentDomains();
}

QList<QVariant> pqSMAdaptor::getMultipleElementPropertyDomain(
                        vtkSMProperty* Property, unsigned int Index)
{
  QList<QVariant> domain;
  
  vtkSMDoubleRangeDomain* DoubleDomain = NULL;
  vtkSMIntRangeDomain* IntDomain = NULL;
  
  vtkSMDomainIterator* iter = Property->NewDomainIterator();
  iter->Begin();
  while(!iter->IsAtEnd())
    {
    vtkSMDomain* d = iter->GetDomain();
    if(!DoubleDomain)
      {
      DoubleDomain = vtkSMDoubleRangeDomain::SafeDownCast(d);
      }
    if(!IntDomain)
      {
      IntDomain = vtkSMIntRangeDomain::SafeDownCast(d);
      }
    iter->Next();
    }
  iter->Delete();

  int which = 0;
  vtkSMExtentDomain* extDomain = vtkSMExtentDomain::SafeDownCast(IntDomain);
  if(extDomain)
    {
    which = Index/2;
    }

  if(DoubleDomain)
    {
    int exists1, exists2;
    double min = DoubleDomain->GetMinimum(which, exists1);
    double max = DoubleDomain->GetMaximum(which, exists2);
    if(exists1 && exists2)  // what if one of them exists?
      {
      domain.push_back(min);
      domain.push_back(max);
      }
    }
  else if(IntDomain)
    {
    int exists1, exists2;
    int min = IntDomain->GetMinimum(which, exists1);
    int max = IntDomain->GetMaximum(which, exists2);
    if(exists1 && exists2)  // what if one of them exists?
      {
      domain.push_back(min);
      domain.push_back(max);
      }
    }

  return domain;
}

QString pqSMAdaptor::getFileListProperty(vtkSMProperty* Property)
{
  QString file;

  vtkSMStringVectorProperty* svp;
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(svp && svp->GetNumberOfElements() > 0)
    {
    file = svp->GetElement(0);
    }
  return file;
}

void pqSMAdaptor::setFileListProperty(vtkSMProperty* Property, QString Value)
{
  vtkSMStringVectorProperty* svp;
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(svp && svp->GetNumberOfElements() > 0)
    {
    if(!Value.isNull())
      {
      svp->SetElement(0, Value.toAscii().data());
      }
    }
}

void pqSMAdaptor::setUncheckedFileListProperty(vtkSMProperty* Property,
                                               QString Value)
{
  vtkSMStringVectorProperty* svp;
  svp = vtkSMStringVectorProperty::SafeDownCast(Property);

  if(svp && svp->GetNumberOfElements() > 0)
    {
    if(!Value.isNull())
      {
      svp->SetUncheckedElement(0, Value.toAscii().data());
      }
    }
  Property->UpdateDependentDomains();
}

QString pqSMAdaptor::getFieldSelectionMode(vtkSMProperty* prop)
{
  QString ret;
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  vtkSMEnumerationDomain* domain =
    vtkSMEnumerationDomain::SafeDownCast(prop->GetDomain("field_list"));
  
  if(Property && domain)
    {
    int which = QString(Property->GetElement(3)).toInt();
    int numEntries = domain->GetNumberOfEntries();
    for(int i=0; i<numEntries; i++)
      {
      if(domain->GetEntryValue(i) == which)
        {
        ret = domain->GetEntryText(i);
        break;
        }
      }
    }
  return ret;
}

void pqSMAdaptor::setFieldSelectionMode(vtkSMProperty* prop, 
                                             const QString& val)
{
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  vtkSMEnumerationDomain* domain =
    vtkSMEnumerationDomain::SafeDownCast(prop->GetDomain("field_list"));
  
  if(Property && domain)
    {
    int numEntries = domain->GetNumberOfEntries();
    for(int i=0; i<numEntries; i++)
      {
      if(val == domain->GetEntryText(i))
        {
        Property->SetElement(3, 
           QString("%1").arg(domain->GetEntryValue(i)).toAscii().data());
        break;
        }
      }
    }
}

void pqSMAdaptor::setUncheckedFieldSelectionMode(vtkSMProperty* prop, 
                                             const QString& val)
{
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  vtkSMEnumerationDomain* domain =
    vtkSMEnumerationDomain::SafeDownCast(prop->GetDomain("field_list"));
  
  if(Property && domain)
    {
    int numEntries = domain->GetNumberOfEntries();
    for(int i=0; i<numEntries; i++)
      {
      if(val == domain->GetEntryText(i))
        {
        Property->SetUncheckedElement(3, 
           QString("%1").arg(domain->GetEntryValue(i)).toAscii().data());
        break;
        }
      }
    Property->UpdateDependentDomains();
    }
}

QList<QString> pqSMAdaptor::getFieldSelectionModeDomain(vtkSMProperty* prop)
{
  QList<QString> types;

  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  vtkSMEnumerationDomain* domain =
    vtkSMEnumerationDomain::SafeDownCast(prop->GetDomain("field_list"));
  
  if(Property && domain)
    {
    int numEntries = domain->GetNumberOfEntries();
    for(int i=0; i<numEntries; i++)
      {
      types.append(domain->GetEntryText(i));
      }
    }
  return types;
}


QString pqSMAdaptor::getFieldSelectionScalar(vtkSMProperty* prop)
{
  QString ret;
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  
  if(Property)
    {
    ret = Property->GetElement(4);
    }
  return ret;
}

void pqSMAdaptor::setFieldSelectionScalar(vtkSMProperty* prop, 
                                              const QString& val)
{
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  
  if(Property)
    {
    Property->SetElement(4, val.toAscii().data());
    }
}

void pqSMAdaptor::setUncheckedFieldSelectionScalar(vtkSMProperty* prop, 
                                              const QString& val)
{
  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  
  if(Property)
    {
    Property->SetUncheckedElement(4, val.toAscii().data());
    Property->UpdateDependentDomains();
    }
}

QList<QString> pqSMAdaptor::getFieldSelectionScalarDomain(vtkSMProperty* prop)
{
  QList<QString> types;

  vtkSMStringVectorProperty* Property =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  vtkSMArrayListDomain* domain = prop ?
    vtkSMArrayListDomain::SafeDownCast(prop->GetDomain("array_list")) : 0;
  
  if(Property && domain)
    {
    int numEntries = domain->GetNumberOfStrings();
    for(int i=0; i<numEntries; i++)
      {
      types.append(domain->GetString(i));
      }
    }
  return types;
}

//-----------------------------------------------------------------------------
QList<QString> pqSMAdaptor::getDomainTypes(vtkSMProperty* property)
{
  QList<QString> types;
  if (property)
    {
    vtkSMDomainIterator* iter = property->NewDomainIterator();
    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
      {
      vtkSMDomain* d = iter->GetDomain();
      QString classname = d->GetClassName();
      if (!types.contains(classname))
        {
        types.push_back(classname);
        }
      }
    iter->Delete();
    }
  return types;
}
