/*=========================================================================

   Program:   ParaView
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

========================================================================*/
#include "pqApplicationCore.h"

// ParaView Server Manager includes.
#include "vtkProcessModule.h"
#include "vtkProcessModuleConnectionManager.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkPVXMLElement.h"
#include "vtkSMArrayListDomain.h"
#include "vtkSMDataObjectDisplayProxy.h"
#include "vtkSMDoubleRangeDomain.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMPQStateLoader.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "QVTKWidget.h"

// Qt includes.
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QPointer>
#include <QtDebug>
#include <QSize>

// ParaView includes.
#include "pq3DWidgetFactory.h"
#include "pqOptions.h"
#include "pqPendingDisplayManager.h"
#include "pqPendingDisplayUndoElement.h"
#include "pqPipelineBuilder.h"
#include "pqPipelineDisplay.h"
#include "pqPipelineSource.h"
#include "pqReaderFactory.h"
#include "pqRenderModule.h"
#include "pqServer.h"
#include "pqServerManagerModel.h"
#include "pqServerManagerObserver.h"
#include "pqServerManagerSelectionModel.h"
#include "pqServerResources.h"
#include "pqServerStartups.h"
#include "pqSettings.h"
#include "pqSMAdaptor.h"
#include "pqUndoStack.h"
#include "pqWriterFactory.h"
#include "pqXMLUtil.h"

//-----------------------------------------------------------------------------
class pqApplicationCoreInternal
{
public:
  pqServerManagerObserver* ServerManagerObserver;
  pqServerManagerModel* ServerManagerModel;
  pqUndoStack* UndoStack;
  pqPipelineBuilder* PipelineBuilder;
  pq3DWidgetFactory* WidgetFactory;
  pqReaderFactory* ReaderFactory;
  pqWriterFactory* WriterFactory;
  pqServerManagerSelectionModel* SelectionModel;
  pqPendingDisplayManager* PendingDisplayManager;

  QString OrganizationName;
  QString ApplicationName;
  QPointer<pqServerResources> ServerResources;
  QPointer<pqServerStartups> ServerStartups;
  QPointer<pqSettings> Settings;
};


//-----------------------------------------------------------------------------
pqApplicationCore* pqApplicationCore::Instance = 0;

//-----------------------------------------------------------------------------
pqApplicationCore* pqApplicationCore::instance()
{
  return pqApplicationCore::Instance;
}

//-----------------------------------------------------------------------------
pqApplicationCore::pqApplicationCore(QObject* p/*=null*/)
  : QObject(p)
{
  this->Internal = new pqApplicationCoreInternal();

  this->Internal->ApplicationName = "ParaViewBasedApplication";
  this->Internal->OrganizationName = "Humanity";

  // *  Create pqServerManagerObserver first. This is the vtkSMProxyManager observer.
  this->Internal->ServerManagerObserver = new pqServerManagerObserver(this);

  // *  Create pqServerManagerModel.
  //    This is the representation builder for the ServerManager state.
  this->Internal->ServerManagerModel = new pqServerManagerModel(this);

  // *  Make signal-slot connections between ServerManagerObserver and ServerManagerModel.
  this->connect(this->Internal->ServerManagerObserver, this->Internal->ServerManagerModel);


  // *  Create the Undo/Redo stack.
  this->Internal->UndoStack = new pqUndoStack(false, this);

  // *  Create the pqPipelineBuilder. This is used to create pipeline objects.
  this->Internal->PipelineBuilder = new pqPipelineBuilder(this);
  this->Internal->PipelineBuilder->setUndoStack(this->Internal->UndoStack);

  if (!pqApplicationCore::Instance)
    {
    pqApplicationCore::Instance = this;
    }

  // * Create various factories.
  this->Internal->WidgetFactory = new pq3DWidgetFactory(this);
  this->Internal->ReaderFactory = new pqReaderFactory(this);
  this->Internal->WriterFactory = new pqWriterFactory(this);

  // * Setup the selection model.
  this->Internal->SelectionModel = new pqServerManagerSelectionModel(
    this->Internal->ServerManagerModel, this);
  
  this->Internal->PendingDisplayManager = new pqPendingDisplayManager(this);
}

//-----------------------------------------------------------------------------
pqApplicationCore::~pqApplicationCore()
{
  if (pqApplicationCore::Instance == this)
    {
    pqApplicationCore::Instance = 0;
    }
  delete this->Internal;
}

//-----------------------------------------------------------------------------
void pqApplicationCore::connect(pqServerManagerObserver* pdata, 
  pqServerManagerModel* smModel)
{
  QObject::connect(pdata, SIGNAL(sourceRegistered(QString, vtkSMProxy*)),
    smModel, SLOT(onAddSource(QString, vtkSMProxy*)));
  QObject::connect(pdata, SIGNAL(sourceUnRegistered(QString, vtkSMProxy*)),
    smModel, SLOT(onRemoveSource(QString, vtkSMProxy*)));
  QObject::connect(pdata, SIGNAL(connectionCreated(vtkIdType)),
    smModel, SLOT(onAddServer(vtkIdType)));
  QObject::connect(pdata, SIGNAL(connectionClosed(vtkIdType)),
    smModel, SLOT(onRemoveServer(vtkIdType)));
  QObject::connect(pdata, SIGNAL(renderModuleRegistered(QString, 
        vtkSMRenderModuleProxy*)),
    smModel, SLOT(onAddRenderModule(QString, vtkSMRenderModuleProxy*)));
  QObject::connect(pdata, SIGNAL(renderModuleUnRegistered(vtkSMRenderModuleProxy*)),
    smModel, SLOT(onRemoveRenderModule(vtkSMRenderModuleProxy*)));
  QObject::connect(pdata, 
    SIGNAL(displayRegistered(QString, vtkSMProxy*)),
    smModel, SLOT(onAddDisplay(QString, vtkSMProxy*)));
  QObject::connect(pdata, SIGNAL(displayUnRegistered(vtkSMProxy*)),
    smModel, SLOT(onRemoveDisplay(vtkSMProxy*)));
      
}

//-----------------------------------------------------------------------------
pqServerManagerObserver* pqApplicationCore::getServerManagerObserver()
{
  return this->Internal->ServerManagerObserver;
}

//-----------------------------------------------------------------------------
pqServerManagerModel* pqApplicationCore::getServerManagerModel()
{
  return this->Internal->ServerManagerModel;
}

//-----------------------------------------------------------------------------
pqUndoStack* pqApplicationCore::getUndoStack()
{
  return this->Internal->UndoStack;
}

//-----------------------------------------------------------------------------
pqPipelineBuilder* pqApplicationCore::getPipelineBuilder()
{
  return this->Internal->PipelineBuilder;
}

//-----------------------------------------------------------------------------
pq3DWidgetFactory* pqApplicationCore::get3DWidgetFactory()
{
  return this->Internal->WidgetFactory;
}

//-----------------------------------------------------------------------------
pqReaderFactory* pqApplicationCore::getReaderFactory()
{
  return this->Internal->ReaderFactory;
}

//-----------------------------------------------------------------------------
pqWriterFactory* pqApplicationCore::getWriterFactory()
{
  return this->Internal->WriterFactory;
}

//-----------------------------------------------------------------------------
pqServerManagerSelectionModel* pqApplicationCore::getSelectionModel()
{
  return this->Internal->SelectionModel;
}

pqPendingDisplayManager* pqApplicationCore::getPendingDisplayManager()
{
  return this->Internal->PendingDisplayManager;
}


//-----------------------------------------------------------------------------
void pqApplicationCore::removeSource(pqPipelineSource* source)
{
  if (!source)
    {
    qDebug() << "No source to remove.";
    return;
    }
  if (source->getNumberOfConsumers())
    {
    qDebug() << "Active source has consumers, cannot delete";
    return;
    }

  QList<pqRenderModule*> renModules = source->getRenderModules();

  // HACK: This will make sure that the panel for the source being
  // removed goes away before the source is deleted. Probably the selection
  // should also go into the undo stack, that way on undo, the GUI selection
  // can also be restored.
  emit this->sourceRemoved(source);
 
  this->getPipelineBuilder()->remove(source);

  foreach (pqRenderModule* ren, renModules)
    {
    ren->render();
    }
}


//-----------------------------------------------------------------------------
void pqApplicationCore::removeServer(pqServer* server)
{
  if (!server)
    {
    qDebug() << "No server to remove.";
    return;
    }

  this->getServerManagerModel()->beginRemoveServer(server);
  this->getPipelineBuilder()->deleteProxies(server);
  pqServer::disconnect(server);
  this->getServerManagerModel()->endRemoveServer();
}



//-----------------------------------------------------------------------------
void pqApplicationCore::saveState(vtkPVXMLElement* rootElement)
{
  // * Save the Proxy Manager state.

  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
  // Eventually proxy manager will save state for each connection separately.
  // For now, we only have one connection, so simply save it.
  vtkPVXMLElement* smState = vtkPVXMLElement::New();
  smState->SetName("ServerManagerState");
  rootElement->AddNestedElement(smState);
  smState->Delete();

  pxm->SaveState(smState);
}

//-----------------------------------------------------------------------------
void pqApplicationCore::loadState(vtkPVXMLElement* rootElement, 
                                  pqServer* server,
                                  vtkSMStateLoader* arg_loader/*=NULL*/)
{
  if (!server)
    {
    return ;
    }

  vtkSmartPointer<vtkSMStateLoader> loader = arg_loader;
  if (!loader)
    {
    loader.TakeReference(vtkSMPQStateLoader::New());
    rootElement = pqXMLUtil::FindNestedElementByName(rootElement,
      "ServerManagerState");
    }

  if (vtkSMPQStateLoader* pqLoader = vtkSMPQStateLoader::SafeDownCast(loader))
    {
    pqLoader->SetUseExistingRenderModules(1);
    pqLoader->SetMultiViewRenderModuleProxy(server->GetRenderModule());
    }

  if (rootElement)
    {
    vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
    pxm->LoadState(rootElement, server->GetConnectionID(), loader);
    pxm->UpdateRegisteredProxies("sources", 0);
    pxm->UpdateRegisteredProxies("displays", 0);
    pxm->UpdateRegisteredProxies(0);
    }

  // Clear undo stack.
  this->Internal->UndoStack->Clear();

  emit this->stateLoaded();
}

pqServerResources& pqApplicationCore::serverResources()
{
  if(!this->Internal->ServerResources)
    {
    this->Internal->ServerResources = new pqServerResources();
    this->Internal->ServerResources->load(*this->settings());
    }
    
  return *this->Internal->ServerResources;
}

pqServerStartups& pqApplicationCore::serverStartups()
{
  if(!this->Internal->ServerStartups)
    {
    this->Internal->ServerStartups = new pqServerStartups();
    
    // Load default settings ...
    QFile file(QApplication::applicationDirPath() + "/default_servers.pvsc");
    if(file.exists())
      {
      QDomDocument xml;
      QString error_message;
      int error_line = 0;
      int error_column = 0;
      if(xml.setContent(&file, false, &error_message, &error_line, &error_column))
        {
        this->Internal->ServerStartups->load(xml);
        }
      else
        {
        qWarning() << "Error loading default_servers.pvsc: " << error_message << " line: " << error_line << " column: " << error_column;
        }
      }
    
    // Load user settings ...
    this->Internal->ServerStartups->load(*this->settings());
    }
    
  return *this->Internal->ServerStartups;
}

//-----------------------------------------------------------------------------
pqSettings* pqApplicationCore::settings()
{
  if ( !this->Internal->Settings )
    {
    if ( this->Internal->OrganizationName.isEmpty() ||
      this->Internal->ApplicationName.isEmpty() )
      {
      return 0;
      }
    pqOptions* options = pqOptions::SafeDownCast(
      vtkProcessModule::GetProcessModule()->GetOptions());
    if (options && options->GetDisableRegistry())
      {
      this->Internal->Settings = new pqSettings(this->Internal->OrganizationName,
        this->Internal->ApplicationName + ".DisabledRegistry");
      this->Internal->Settings->clear();
      }
    else
      {
      this->Internal->Settings = new pqSettings(this->Internal->OrganizationName,
        this->Internal->ApplicationName);
      }
    }
  return this->Internal->Settings;
}

//-----------------------------------------------------------------------------
void pqApplicationCore::setApplicationName(const QString& an)
{
  this->Internal->ApplicationName = an;
}

//-----------------------------------------------------------------------------
QString pqApplicationCore::applicationName()
{
  return this->Internal->ApplicationName;
}

//-----------------------------------------------------------------------------
void pqApplicationCore::setOrganizationName(const QString& on)
{
  this->Internal->OrganizationName = on;
}

//-----------------------------------------------------------------------------
QString pqApplicationCore::organizationName()
{
  return this->Internal->OrganizationName;
}

pqServer* pqApplicationCore::createServer(const pqServerResource& resource)
{
  // Create a modified version of the resource that only contains server information
  const pqServerResource server_resource = resource.schemeHostsPorts();

  // See if the server is already created.
  pqServerManagerModel *smModel = this->getServerManagerModel();
  pqServer *server = smModel->getServer(server_resource);
  if(!server)
    {
    // TEMP: ParaView only allows one server connection. Remove this
    // code when it supports multiple server connections.
    if(smModel->getNumberOfServers() > 0)
      {
      this->removeServer(smModel->getServerByIndex(0));
      }

    // Based on the server resource, create the correct type of server ...
    vtkProcessModule *pm = vtkProcessModule::GetProcessModule();
    vtkIdType id = vtkProcessModuleConnectionManager::GetNullConnectionID();
    if(server_resource.scheme() == "builtin")
      {
      id = pm->ConnectToSelf();
      }
    else if(server_resource.scheme() == "cs")
      {
      id = pm->ConnectToRemote(
        resource.host().toAscii().data(),
        resource.port(11111));
      }
    else if(server_resource.scheme() == "csrc")
      {
      qWarning() << "Server reverse connections not supported yet\n";
      }
    else if(server_resource.scheme() == "cdsrs")
      {
      id = pm->ConnectToRemote(
        server_resource.dataServerHost().toAscii().data(),
        server_resource.dataServerPort(11111),
        server_resource.renderServerHost().toAscii().data(),
        server_resource.renderServerPort(22221));
      }
    else if(server_resource.scheme() == "cdsrsrc")
      {
      qWarning() << "Data server/render server reverse connections not supported yet\n";
      }
    else
      {
      qCritical() << "Unknown server type: " << server_resource.scheme() << "\n";
      }

    if(id != vtkProcessModuleConnectionManager::GetNullConnectionID())
      {
      if(server_resource.scheme() != "builtin")
        {
        // Synchronize options with the server.
        // TODO: This again will work more reliably once we have separate
        // PVOptions per connection.
        pm->SynchronizeServerClientOptions(id);
        }

      server = smModel->getServer(id);
      server->setResource(server_resource);
//      emit this->serverCreated(server);
      }
    }

  return server;
}

static void SetDefaultInputArray(vtkSMProxy* Proxy, const char* PropertyName)
{
  if(vtkSMStringVectorProperty* const array =
    vtkSMStringVectorProperty::SafeDownCast(
      Proxy->GetProperty(PropertyName)))
    {
    Proxy->UpdateVTKObjects();
  
    QList<QVariant> domain = 
      pqSMAdaptor::getEnumerationPropertyDomain(array);

    for(int i = 0; i != domain.size(); ++i)
      {
      const QString name = domain[i].toString();
      array->SetElement(4, name.toAscii().data());
      array->UpdateDependentDomains();
      break;
      }
    }
}

pqPipelineSource* pqApplicationCore::createFilterForSource(const QString& xmlname,
                                        pqPipelineSource* input)
{
  if (!input)
    {
    qDebug() << "No source/filter active. Cannot createFilterForSource.";
    return NULL;
    }

  this->getUndoStack()->BeginUndoSet(QString("Create ") + xmlname);

  pqPipelineSource* filter = this->getPipelineBuilder()->createSource(
    "filters", xmlname.toStdString().c_str(), 
    input->getServer(), NULL);

  if(filter)
    {
    this->getPipelineBuilder()->addConnection(input, filter);

    // As a special-case, set a default implicit function for new Cut filters
    if(xmlname == "Cut")
      {
      this->Internal->UndoStack->BeginUndoSet("Set CutConnection");
      
      if(vtkSMDoubleVectorProperty* const contours =
        vtkSMDoubleVectorProperty::SafeDownCast(
          filter->getProxy()->GetProperty("ContourValues")))
        {
        contours->SetNumberOfElements(1);
        contours->SetElement(0, 0.0);
        }
        
      this->getUndoStack()->EndUndoSet();
      }

    // As a special-case, set a default point source for new StreamTracer filters
    if(xmlname == "StreamTracer")
      {
      this->Internal->UndoStack->BeginUndoSet("Set Point Source");
      vtkSMProxyProperty* sourceProperty = vtkSMProxyProperty::SafeDownCast(
        filter->getProxy()->GetProperty("Source"));
      if(sourceProperty && sourceProperty->GetNumberOfProxies() > 0)
        {
        vtkSMProxy* const point_source = sourceProperty->GetProxy(0);
        if(vtkSMIntVectorProperty* const number_of_points =
          vtkSMIntVectorProperty::SafeDownCast(
            point_source->GetProperty("NumberOfPoints")))
          {
          number_of_points->SetNumberOfElements(1);
          number_of_points->SetElement(0, 100);
          }
        point_source->UpdateVTKObjects();
        }
        
      this->getUndoStack()->EndUndoSet();
      }
      
    emit this->sourceCreated(filter);

    // As a special-case, set the default contour for new Contour filters
    if(xmlname == "Contour")
      {
      double min_value = 0.0;
      double max_value = 0.0;

      SetDefaultInputArray(filter->getProxy(), "SelectInputScalars");

      if(vtkSMDoubleVectorProperty* const contours =
        vtkSMDoubleVectorProperty::SafeDownCast(
          filter->getProxy()->GetProperty("ContourValues")))
        {
        if(vtkSMDoubleRangeDomain* const domain =
          vtkSMDoubleRangeDomain::SafeDownCast(
            contours->GetDomain("scalar_range")))
          {
          int min_exists = 0;
          min_value = domain->GetMinimum(0, min_exists);
          
          int max_exists = 0;
          max_value = domain->GetMaximum(0, max_exists);
          }

        contours->SetNumberOfElements(1);
        contours->SetElement(0, (min_value + max_value) * 0.5);
        }
      }

    // As a special-case, set a default point source for new StreamTracer filters
    if(xmlname == "StreamTracer")
      {
      SetDefaultInputArray(filter->getProxy(), "SelectInputVectors");
      }
    }
    
  this->getUndoStack()->EndUndoSet();
  
  return filter;
}

pqPipelineSource* pqApplicationCore::createSourceOnServer(const QString& xmlname,
  pqServer* server)
{
  if (!server)
    {
    qDebug() << "No server specified. "
      << "Cannot createSourceOnServer.";
    return 0;
    }

  this->getUndoStack()->BeginUndoSet(QString("Create ") + xmlname);

  pqPipelineSource* source = this->getPipelineBuilder()->createSource(
    "sources", xmlname.toStdString().c_str(), 
    server, NULL);

  emit this->sourceCreated(source);
  this->getUndoStack()->EndUndoSet();
  
  return source;
}

pqPipelineSource* pqApplicationCore::createCompoundFilter(
                         const QString& name,
                         pqServer* server,
                         pqPipelineSource* input)
{
  this->getUndoStack()->BeginUndoSet(QString("Create ") + name);

  pqPipelineSource* source = this->getPipelineBuilder()->createSource(
    NULL, name.toStdString().c_str(), 
    server, NULL);
  
  vtkSMProperty* inputProperty = NULL;

  if(source)
    {
    inputProperty = source->getProxy()->GetProperty("Input");
    }

  if(inputProperty && input == NULL)
    {
    this->removeSource(source);
    source = NULL;
    qWarning() << "Cannot create custom filter without active input source.";
    }
  else if(inputProperty && input)
    {
    this->getPipelineBuilder()->addConnection(input, source);
    }

  if(source)
    {
    source->getProxy()->UpdateVTKObjects();
    emit this->sourceCreated(source);
    }

  this->getUndoStack()->EndUndoSet();
  return source;
}


pqPipelineSource* pqApplicationCore::createReaderOnServer(
               const QString& filename,
               pqServer* server)
{
  if (!server)
    {
    qDebug() << "No active server. Cannot create reader.";
    return 0;
    }
  
  this->getUndoStack()->BeginUndoSet(QString("Create reader for ") + filename);

  pqPipelineSource* reader= this->getReaderFactory()->createReader(filename, 
                                                                   server);
  if (!reader)
    {
    return NULL;
    }

  vtkSMProxy* proxy = reader->getProxy();
  pqSMAdaptor::setElementProperty(proxy->GetProperty("FileName"), 
    filename);
  pqSMAdaptor::setElementProperty(proxy->GetProperty("FilePrefix"),
    filename);
  pqSMAdaptor::setElementProperty(proxy->GetProperty("FilePattern"),
    filename);
  proxy->UpdateVTKObjects();

  emit this->sourceCreated(reader);
  this->getUndoStack()->EndUndoSet();
  return reader;
}

void pqApplicationCore::render()
{
  unsigned int numRenderModules =
    this->getServerManagerModel()->getNumberOfRenderModules();
  for(unsigned int i=0; i<numRenderModules; i++)
    {
    pqRenderModule* renModule =
      this->getServerManagerModel()->getRenderModule(i);
    renModule->render();
    }
}


