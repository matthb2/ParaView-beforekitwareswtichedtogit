/*=========================================================================

   Program:   ParaQ
   Module:    $RCS $

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaQ is a free software; you can redistribute it and/or modify it
   under the terms of the ParaQ license version 1.1. 

   See License_v1.1.txt for the full ParaQ license.
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

#include "pqObjectLineChartWidget.h"
#include "pqServer.h"

#include <pqChartAxis.h>
#include <pqChartCoordinate.h>
#include <pqChartLabel.h>
#include <pqChartLegend.h>
#include <pqConnect.h>
#include <pqFileDialog.h>
#include <pqLineChart.h>
#include <pqLineChartWidget.h>
#include <pqLocalFileDialogModel.h>
#include <pqMarkerPen.h>
#include <pqLinePlot.h>

#include <QHBoxLayout>
#include <QPrinter>
#include <QPushButton>
#include <QVBoxLayout>

#include <vtkCellData.h>
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkSMInputProperty.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMCompoundProxy.h>
#include <vtkSphereSource.h>
#include <vtkUnstructuredGrid.h>

#include <vtkExodusReader.h>
#include <vtkProcessModule.h>

#include <vtkstd/algorithm>
#include <vtkstd/vector>

//////////////////////////////////////////////////////////////////////////////
// pqObjectLineChartWidget::pqImplementation

struct pqObjectLineChartWidget::pqImplementation
{
  pqImplementation() :
    SourceProxy(0),
    EventAdaptor(vtkEventQtSlotConnect::New()),
    VariableType(VARIABLE_TYPE_CELL)
  {
    QFont h1;
    h1.setBold(true);
    h1.setPointSize(12);
    h1.setStyleStrategy(QFont::PreferAntialias);
  
    QFont bold;
    bold.setBold(true);
    bold.setStyleStrategy(QFont::PreferAntialias);
    
    QFont italic;
    italic.setItalic(true);
    italic.setStyleStrategy(QFont::PreferAntialias);
    
    QFont bold_italic;
    bold_italic.setBold(true);
    bold_italic.setItalic(true);
    bold_italic.setStyleStrategy(QFont::PreferAntialias);
    
    this->LineChartWidget.setBackgroundColor(Qt::white);
    
    this->LineChartWidget.getTitle().setFont(h1);
    this->LineChartWidget.getTitle().setColor(Qt::black);
    
    this->LineChartWidget.getXAxis().setGridColor(Qt::lightGray);
    this->LineChartWidget.getXAxis().setAxisColor(Qt::darkGray);
    this->LineChartWidget.getXAxis().setTickLabelColor(Qt::darkGray);
    this->LineChartWidget.getXAxis().setTickLabelFont(italic);

    this->LineChartWidget.getXAxis().getLabel().setText("Time");
    this->LineChartWidget.getXAxis().getLabel().setFont(bold);
    
    this->LineChartWidget.getYAxis().setGridColor(Qt::lightGray);
    this->LineChartWidget.getYAxis().setAxisColor(Qt::darkGray);
    this->LineChartWidget.getYAxis().setTickLabelColor(Qt::darkGray);
    this->LineChartWidget.getYAxis().setTickLabelFont(italic);

    this->LineChartWidget.getYAxis().getLabel().setFont(bold);
    this->LineChartWidget.getYAxis().getLabel().setOrientation(pqChartLabel::VERTICAL);
    
    this->updateChart();
  }
  
  ~pqImplementation()
  {
    this->EventAdaptor->Delete();
  }
  
  void setProxy(vtkSMProxy* Proxy)
  {
    this->SourceProxy = Proxy;
    this->VariableName = QString();

    // TODO: hack -- figure out how compound proxies really fit in
    vtkSMCompoundProxy* cp = vtkSMCompoundProxy::SafeDownCast(Proxy);
    if(cp)
      {
        Proxy = NULL;
        for(int i=cp->GetNumberOfProxies(); Proxy == NULL && i>0; i--)
          {
          Proxy = vtkSMSourceProxy::SafeDownCast(cp->GetProxy(i-1));
          }
      }

    this->onInputChanged();
  }
  
  void setVariable(pqVariableType type, const QString& name)
  {
    this->VariableType = type;
    this->VariableName = name;
    this->updateChart();
  }
  
  void clear()
  {
    this->Elements.clear();
    this->updateChart();
  }
  
  void internalAddElements(vtkUnstructuredGrid* elements)
  {
    for(int i = 0; i != elements->GetCellData()->GetNumberOfArrays(); ++i)
      {
      const QString array_name = elements->GetCellData()->GetArrayName(i);
      if(array_name != "Id")
        continue;
      
      vtkDataArray* const data = elements->GetCellData()->GetArray(i);
      if(!data)
        break;
      
      for(int j = 0; j != elements->GetNumberOfCells(); ++j)
        {
        const unsigned long id = static_cast<unsigned long>(data->GetTuple1(j));
        if(!vtkstd::count(this->Elements.begin(), this->Elements.end(), id))
          {
          this->Elements.push_back(id);
          }
        }
      
      break;
      }
  }
  
  void addElements(vtkUnstructuredGrid* elements)
  {
    internalAddElements(elements);
    this->updateChart();
  }
  
  void setElements(vtkUnstructuredGrid* elements)
  {
    this->Elements.clear();
    internalAddElements(elements);
    this->updateChart();
  }
  
  void onInputChanged()
  {
    this->updateChart();
  }
  
  void addPlot(vtkExodusReader& Reader, const int ElementID, const QPen& Pen)
  {
    const int id = ElementID + 1; // Exodus expects one-based cell ids
    const char* type = this->VariableType == VARIABLE_TYPE_CELL ? "CELL" : "POINT";
    vtkFloatArray* const array = vtkFloatArray::New();
    Reader.GetTimeSeriesData(id, this->VariableName.toAscii().data(), type, array); 
    
    pqChartCoordinateList coordinates;
    for(vtkIdType i = 0; i != array->GetNumberOfTuples(); ++i)
      {
      double value = array->GetValue(i);
      coordinates.pushBack(pqChartCoordinate(static_cast<double>(i), value));
      }

    array->Delete();
      
    pqLinePlot* const plot = new pqLinePlot(new pqNullMarkerPen(Pen), coordinates);
    
    this->LineChartWidget.getLineChart().addData(plot);
    this->LineChartWidget.getLegend().addEntry(new pqNullMarkerPen(Pen), new pqChartLabel(QString("Element %1").arg(ElementID)));
  }
  
  void updateChart()
  {
    // Set the default (no data) appearance of the chart ...
    this->LineChartWidget.getLineChart().clearData();
    this->LineChartWidget.getTitle().setText("Time Plot (no data)");
    this->LineChartWidget.getXAxis().setVisible(true);
    this->LineChartWidget.getXAxis().setValueRange(0.0, 100.0);
    this->LineChartWidget.getYAxis().getLabel().setText("Value");
    this->LineChartWidget.getYAxis().setVisible(true);
    this->LineChartWidget.getYAxis().setValueRange(0.0, 100.0);
    this->LineChartWidget.getLegend().clear();

    if(this->VariableName.isEmpty())
      return;

    if(this->Elements.empty())
      return;
    
    if(!this->SourceProxy)
      return;

    const QString source_class = SourceProxy->GetVTKClassName();
    if(source_class != "vtkExodusReader" && source_class != "vtkPExodusReader")
      return;

    vtkProcessModule* const process_module = vtkProcessModule::GetProcessModule();
    vtkExodusReader* const reader = vtkExodusReader::SafeDownCast(
      process_module->GetObjectFromID(this->SourceProxy->GetID(0)));
    if(!reader)
      return;

    int array_id = -1;
    switch(this->VariableType)
      {
      case VARIABLE_TYPE_CELL:
        array_id = reader->GetCellArrayID(this->VariableName.toAscii().data());
        if(-1 == array_id)
          return;
        break;
      case VARIABLE_TYPE_NODE:
        array_id = reader->GetPointArrayID(this->VariableName.toAscii().data());
        if(-1 == array_id)
          return;
        break;
      }

    this->LineChartWidget.getTitle().setText(this->VariableName + " vs. Time");
    this->LineChartWidget.getXAxis().setVisible(true);
    this->LineChartWidget.getYAxis().setVisible(true);
    this->LineChartWidget.getYAxis().getLabel().setText(this->VariableName);

    this->LineChartWidget.getLegend().clear();
    unsigned long count = 0;
    for(vtkstd::vector<unsigned long>::reverse_iterator element = this->Elements.rbegin(); element != this->Elements.rend(); ++element, ++count)
      {
      const double hue = static_cast<double>(count) / static_cast<double>(Elements.size());
      const QColor color = QColor::fromHsvF(hue, 1.0, 1.0);
      addPlot(*reader, *element, QPen(color, 1.5));
      }
  }
  
  vtkSMProxy* SourceProxy;
  vtkEventQtSlotConnect* EventAdaptor;
  pqLineChartWidget LineChartWidget;
  pqVariableType VariableType;
  QString VariableName;
  vtkstd::vector<unsigned long> Elements;
};

/////////////////////////////////////////////////////////////////////////////////
// pqObjectLineChartWidget

pqObjectLineChartWidget::pqObjectLineChartWidget(QWidget *p) :
  QWidget(p),
  Implementation(new pqImplementation())
{
  QVBoxLayout* const vbox = new QVBoxLayout();
  vbox->setMargin(0);
  vbox->addWidget(&this->Implementation->LineChartWidget);
  this->setLayout(vbox);
}

pqObjectLineChartWidget::~pqObjectLineChartWidget()
{
  delete this->Implementation;
}

void pqObjectLineChartWidget::setServer(pqServer* /*server*/)
{
  this->setProxy(0);
}

void pqObjectLineChartWidget::setProxy(vtkSMProxy* proxy)
{
  this->Implementation->setProxy(proxy);
  
  if(proxy)
    {
    this->Implementation->EventAdaptor->Connect(
      proxy,
      vtkCommand::UpdateEvent,
      this,
      SLOT(onInputChanged(vtkObject*,unsigned long, void*, void*, vtkCommand*)));    
    }
}

void pqObjectLineChartWidget::setVariable(pqVariableType type, const QString& name)
{
  this->Implementation->setVariable(type, name);
}

void pqObjectLineChartWidget::clear()
{
  this->Implementation->clear();
}

void pqObjectLineChartWidget::addElements(vtkUnstructuredGrid* Elements)
{
  this->Implementation->addElements(Elements);
}

void pqObjectLineChartWidget::setElements(vtkUnstructuredGrid* Elements)
{
  this->Implementation->setElements(Elements);
}

void pqObjectLineChartWidget::onInputChanged(vtkObject*,unsigned long, void*, void*, vtkCommand*)
{
  this->Implementation->onInputChanged();
}
