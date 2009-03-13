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
/*-------------------------------------------------------------------------
  Copyright 2008 Sandia Corporation.
  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
  the U.S. Government retains certain rights in this software.
-------------------------------------------------------------------------*/

#include "vtkQtListView.h"

#include <QItemSelection>
#include <QListView>

#include "vtkAlgorithm.h"
#include "vtkAlgorithmOutput.h"
#include "vtkConvertSelection.h"
#include "vtkDataRepresentation.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkQtTableModelAdapter.h"
#include "vtkSelection.h"
#include "vtkSelectionLink.h"
#include "vtkSelectionNode.h"
#include "vtkSmartPointer.h"
#include "vtkTable.h"

vtkCxxRevisionMacro(vtkQtListView, "$Revision$");
vtkStandardNewMacro(vtkQtListView);


//----------------------------------------------------------------------------
vtkQtListView::vtkQtListView()
{
  this->ListView = new QListView();
  this->ListAdapter = new vtkQtTableModelAdapter();
  this->ListView->setModel(this->ListAdapter);
  this->ListView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->Selecting = false;

  QObject::connect(this->ListView->selectionModel(), 
      SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)),
      this, 
      SLOT(slotSelectionChanged(const QItemSelection&,const QItemSelection&)));
}

//----------------------------------------------------------------------------
vtkQtListView::~vtkQtListView()
{
  if(this->ListView)
    {
    delete this->ListView;
    }
  delete this->ListAdapter;
}

//----------------------------------------------------------------------------
QWidget* vtkQtListView::GetWidget()
{
  return this->ListView;
}

//----------------------------------------------------------------------------
vtkQtAbstractModelAdapter* vtkQtListView::GetItemModelAdapter()
{
  return this->ListAdapter;
}

//----------------------------------------------------------------------------
void vtkQtListView::AddInputConnection( int vtkNotUsed(port), int vtkNotUsed(index),
  vtkAlgorithmOutput* conn, vtkAlgorithmOutput* vtkNotUsed(selectionConn))
{
  // Get a handle to the input data object. Note: For now
  // we are enforcing that the input data is a List.
  conn->GetProducer()->Update();
  vtkDataObject *d = conn->GetProducer()->GetOutputDataObject(0);
  vtkTable *table = vtkTable::SafeDownCast(d);

  // Enforce input
  if (!table)
    {
    vtkErrorMacro("vtkQtERMView requires a vtkList as input (for now)");
    return;
    }

  // Give the data object to the Qt List Adapters
  this->ListAdapter->SetVTKDataObject(table);

  // Now set the Qt Adapters (qt models) on the views
  this->ListView->update();

}

//----------------------------------------------------------------------------
void vtkQtListView::RemoveInputConnection(int vtkNotUsed(port), int vtkNotUsed(index),
  vtkAlgorithmOutput* conn, vtkAlgorithmOutput* vtkNotUsed(selectionConn))
{
  // Remove VTK data from the adapter
  conn->GetProducer()->Update();
  vtkDataObject *d = conn->GetProducer()->GetOutputDataObject(0);
  if (this->ListAdapter->GetVTKDataObject() == d)
    {
    this->ListAdapter->SetVTKDataObject(0);
    this->ListView->update();
    }
}

//----------------------------------------------------------------------------
void vtkQtListView::slotSelectionChanged(const QItemSelection& vtkNotUsed(s1), const QItemSelection& vtkNotUsed(s2))
{  
  this->Selecting = true;

  // Create index selection
  vtkSmartPointer<vtkSelection> selection =
    vtkSmartPointer<vtkSelection>::New();
  vtkSmartPointer<vtkSelectionNode> node =
    vtkSmartPointer<vtkSelectionNode>::New();
  node->SetContentType(vtkSelectionNode::INDICES);
  node->SetFieldType(vtkSelectionNode::VERTEX);
  vtkSmartPointer<vtkIdTypeArray> idarr =
    vtkSmartPointer<vtkIdTypeArray>::New();
  node->SetSelectionList(idarr);
  selection->AddNode(node);
  const QModelIndexList list = this->ListView->selectionModel()->selectedRows();
  
  // For index selection do this odd little dance with two maps :)
  for (int i = 0; i < list.size(); i++)
    {
    vtkIdType pid = this->ListAdapter->QModelIndexToPedigree(list.at(i));
    idarr->InsertNextValue(this->ListAdapter->PedigreeToId(pid));
    }  

  // Convert to the correct type of selection
  vtkDataObject* data = this->ListAdapter->GetVTKDataObject();
  vtkSmartPointer<vtkSelection> converted;
  converted.TakeReference(vtkConvertSelection::ToSelectionType(
    selection, data, this->SelectionType, this->SelectionArrayNames));
   
  // Call select on the representation
  this->GetRepresentation()->Select(this, converted);
  
  this->Selecting = false;
}

//----------------------------------------------------------------------------
void vtkQtListView::Update()
{
  vtkDataRepresentation* rep = this->GetRepresentation();
  if (!rep)
    {
    return;
    }

  // Make the data current
  vtkAlgorithm* alg = rep->GetInputConnection()->GetProducer();
  alg->Update();
  vtkDataObject *d = alg->GetOutputDataObject(0);
  this->ListAdapter->SetVTKDataObject(d);
  
  // Make the selection current
  if (this->Selecting)
    {
    // If we initiated the selection, do nothing.
    return;
    }

  vtkSelection* s = rep->GetSelectionLink()->GetSelection();
  vtkSmartPointer<vtkSelection> selection;
  selection.TakeReference(vtkConvertSelection::ToIndexSelection(s, d));
  QItemSelection list;
  vtkSelectionNode* node = selection->GetNode(0);
  if (node)
    {
    vtkIdTypeArray* arr = vtkIdTypeArray::SafeDownCast(node->GetSelectionList());
    if (arr)
      {
      for (vtkIdType i = 0; i < arr->GetNumberOfTuples(); i++)
        {
        vtkIdType id = arr->GetValue(i);
        QModelIndex index = 
          this->ListAdapter->PedigreeToQModelIndex(
          this->ListAdapter->IdToPedigree(id));
        list.select(index, index);
        }
      }
    }

  this->ListView->selectionModel()->select(list, 
    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  
  this->ListView->update();
}

//----------------------------------------------------------------------------
void vtkQtListView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

