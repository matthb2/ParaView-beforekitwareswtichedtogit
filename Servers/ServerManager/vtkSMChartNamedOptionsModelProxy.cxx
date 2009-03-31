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
#include "vtkSMChartNamedOptionsModelProxy.h"

#include "vtkObjectFactory.h"
#include "vtkQtChartNamedSeriesOptionsModel.h"
#include "vtkQtChartSeriesOptions.h"
#include "vtkQtChartTableRepresentation.h"
#include "vtkQtChartTableSeriesModel.h"
#include "vtkStringList.h"
#include "vtkSMStringVectorProperty.h"

#include <QPen>
#include <QColor>

class vtkSMChartNamedOptionsModelProxy::vtkInternals
{
public:
  typedef vtkstd::map<vtkstd::string, vtkQtChartSeriesOptions*> OptionsMap;
  vtkQtChartNamedSeriesOptionsModel* OptionsModel;
  OptionsMap Options;

  vtkInternals(): OptionsModel(0)
    {
    }

  ~vtkInternals()
    {
    delete this->OptionsModel;
    this->OptionsModel = 0;
    }

};

vtkCxxRevisionMacro(vtkSMChartNamedOptionsModelProxy, "$Revision$");
//----------------------------------------------------------------------------
vtkSMChartNamedOptionsModelProxy::vtkSMChartNamedOptionsModelProxy()
{
  this->Internals = new vtkInternals();
}

//----------------------------------------------------------------------------
vtkSMChartNamedOptionsModelProxy::~vtkSMChartNamedOptionsModelProxy()
{
  delete this->Internals;
  this->Internals = 0;
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::CreateObjects(
  vtkQtChartTableRepresentation* repr)
{
  this->Internals->OptionsModel = new vtkQtChartNamedSeriesOptionsModel(
    repr->GetSeriesModel(), 0);
}

//----------------------------------------------------------------------------
vtkQtChartNamedSeriesOptionsModel*
vtkSMChartNamedOptionsModelProxy::GetOptionsModel()
{
  return this->Internals->OptionsModel;
}

//----------------------------------------------------------------------------
vtkQtChartSeriesOptions* vtkSMChartNamedOptionsModelProxy::GetOptions(
  const char* name)
{
  vtkQtChartSeriesOptions* options =
    this->Internals->OptionsModel->getOptions(name);
  if (!options)
    {
    options = this->NewOptions();
    options->setParent(this->Internals->OptionsModel);
    this->Internals->OptionsModel->addOptions(name, options);
    }
  return options;
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::UpdatePropertyInformationInternal(
  vtkSMProperty* prop)
{
  vtkSMStringVectorProperty* svp =
    vtkSMStringVectorProperty::SafeDownCast(prop);
  if (svp && svp->GetInformationOnly())
    {
    vtkStringList* new_values = vtkStringList::New();
    int num_options = this->Internals->OptionsModel->getNumberOfOptions();
    const char* propname = this->GetPropertyName(prop);
    bool skip = false;
    for (int cc=0; cc < num_options; cc++)
      {
      QString name = this->Internals->OptionsModel->getSeriesName(cc);
      vtkQtChartSeriesOptions* options = this->GetOptions(name.toAscii().data());
      if (strcmp(propname, "VisibilityInfo") == 0)
        {
        new_values->AddString(name.toAscii().data());
        new_values->AddString(
          QString::number(options->isVisible()? 1 : 0).toAscii().data());
        }
      else if (strcmp(propname, "LineColorInfo") == 0)
        {
        new_values->AddString(name.toAscii().data());

        QPen pen = options->getPen();
        new_values->AddString(QString::number(pen.color().redF()).toAscii().data());
        new_values->AddString(QString::number(pen.color().greenF()).toAscii().data());
        new_values->AddString(QString::number(pen.color().blueF()).toAscii().data());
        }
      else if (strcmp(propname, "LineThicknessInfo") == 0)
        {
        new_values->AddString(name.toAscii().data());

        QPen pen = options->getPen();
        new_values->AddString(QString::number(pen.width()).toAscii().data());
        }
      else if (strcmp(propname, "LineStyleInfo") == 0)
        {
        new_values->AddString(name.toAscii().data());

        QPen pen = options->getPen();
        new_values->AddString(QString::number(
            static_cast<int>(pen.style())).toAscii().data());
        }
      else
        {
        skip = true;
        break;
        }
      }
    if (!skip)
      {
      svp->SetElements(new_values);
      }
    new_values->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::SetVisibility(
  const char* name, int visible)
{
  vtkQtChartSeriesOptions* options = this->GetOptions(name);
  options->setVisible(visible != 0);
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::SetLineColor(
  const char* name, double r, double g, double b)
{
  vtkQtChartSeriesOptions* options = this->GetOptions(name);
  QPen pen = options->getPen();
  pen.setColor(QColor::fromRgbF(r, g, b));
  options->setPen(pen);
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::SetLineThickness(
  const char* name, int value)
{
  vtkQtChartSeriesOptions* options = this->GetOptions(name);
  QPen pen = options->getPen();
  pen.setWidth(value);
  options->setPen(pen);
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::SetLineStyle(
  const char* name, int value)
{
  vtkQtChartSeriesOptions* options = this->GetOptions(name);
  value = value<0? 0 : value;
  value = value>4? 4 : value;
  QPen pen= options->getPen();
  pen.setStyle(static_cast<Qt::PenStyle>(value));
  options->setPen(pen);
}

//----------------------------------------------------------------------------
void vtkSMChartNamedOptionsModelProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


