/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
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
#include "pqCPExportStateWizard.h"
#include <QWizardPage>
#include <QPointer>
#include <QtDebug>

#include "pqPipelineFilter.h"
#include "pqApplicationCore.h"
#include "pqServerManagerModel.h"
#include "pqPythonDialog.h"
#include "pqPythonManager.h"
#include "pqFileDialog.h"

extern const char* cp_export_py;

// HACK.
static QPointer<pqCPExportStateWizard> ActiveWizard;

class pqCPExportStateWizardPage2 : public QWizardPage
{
  pqCPExportStateWizard::pqInternals* Internals;
public:
  pqCPExportStateWizardPage2(QWidget* _parent=0)
    : QWizardPage(_parent)
    {
    this->Internals = ::ActiveWizard->Internals;
    }

  virtual void initializePage();

  virtual bool isComplete() const;

  void emitCompleteChanged()
    { emit this->completeChanged(); }
};

class pqCPExportStateWizardPage3: public QWizardPage
{
  pqCPExportStateWizard::pqInternals* Internals;
public:
  pqCPExportStateWizardPage3(QWidget* _parent=0)
    : QWizardPage(_parent)
    {
    this->Internals = ::ActiveWizard->Internals;
    }

  virtual void initializePage();
};


#include "ui_ExportStateWizard.h"

class pqCPExportStateWizard::pqInternals : public Ui::ExportStateWizard
{
};

//-----------------------------------------------------------------------------
void pqCPExportStateWizardPage2::initializePage()
{
  this->Internals->simulationInputs->clear();
  this->Internals->allInputs->clear();
  QList<pqPipelineSource*> sources =
    pqApplicationCore::instance()->getServerManagerModel()->
    findItems<pqPipelineSource*>();
  foreach (pqPipelineSource* source, sources)
    {
    if (qobject_cast<pqPipelineFilter*>(source))
      {
      continue;
      }
    this->Internals->allInputs->addItem(source->getSMName());
    }
}

//-----------------------------------------------------------------------------
bool pqCPExportStateWizardPage2::isComplete() const
{
  return this->Internals->simulationInputs->count() > 0;
}


//-----------------------------------------------------------------------------
void pqCPExportStateWizardPage3::initializePage()
{
  this->Internals->nameWidget->clearContents();
  this->Internals->nameWidget->setRowCount(
    this->Internals->simulationInputs->count());
  for (int cc=0; cc < this->Internals->simulationInputs->count(); cc++)
    {
    QListWidgetItem* item = this->Internals->simulationInputs->item(cc);
    QString text = item->text();
    this->Internals->nameWidget->setItem(cc, 0, new QTableWidgetItem(text));
    this->Internals->nameWidget->setItem(cc, 1, new QTableWidgetItem(text));
    QTableWidgetItem* tableItem = this->Internals->nameWidget->item(cc, 1);
    tableItem->setFlags(tableItem->flags()|Qt::ItemIsEditable);


    tableItem = this->Internals->nameWidget->item(cc, 0);
    tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
    }
}

//-----------------------------------------------------------------------------
pqCPExportStateWizard::pqCPExportStateWizard(
  QWidget *parentObject, Qt::WindowFlags parentFlags)
: Superclass(parentObject, parentFlags)
{
  ::ActiveWizard = this;
  this->Internals = new pqInternals();
  this->Internals->setupUi(this);
  ::ActiveWizard = NULL;

  QObject::connect(this->Internals->allInputs, SIGNAL(itemSelectionChanged()),
    this, SLOT(updateAddRemoveButton()));
  QObject::connect(this->Internals->simulationInputs, SIGNAL(itemSelectionChanged()),
    this, SLOT(updateAddRemoveButton()));
  QObject::connect(this->Internals->addButton, SIGNAL(clicked()),
    this, SLOT(onAdd()));
  QObject::connect(this->Internals->removeButton, SIGNAL(clicked()),
    this, SLOT(onRemove()));
}

//-----------------------------------------------------------------------------
pqCPExportStateWizard::~pqCPExportStateWizard()
{
  delete this->Internals;
}

//-----------------------------------------------------------------------------
void pqCPExportStateWizard::updateAddRemoveButton()
{
  this->Internals->addButton->setEnabled(
    this->Internals->allInputs->selectedItems().size() > 0);
  this->Internals->removeButton->setEnabled(
    this->Internals->simulationInputs->selectedItems().size() > 0);
}

//-----------------------------------------------------------------------------
void pqCPExportStateWizard::onAdd()
{
  foreach (QListWidgetItem* item, this->Internals->allInputs->selectedItems())
    {
    QString text = item->text();
    this->Internals->simulationInputs->addItem(text);
    delete this->Internals->allInputs->takeItem(
      this->Internals->allInputs->row(item));
    }
  dynamic_cast<pqCPExportStateWizardPage2*>(this->currentPage())->emitCompleteChanged();
}

//-----------------------------------------------------------------------------
void pqCPExportStateWizard::onRemove()
{
  foreach (QListWidgetItem* item, this->Internals->simulationInputs->selectedItems())
    {
    QString text = item->text();
    this->Internals->allInputs->addItem(text);
    delete this->Internals->simulationInputs->takeItem(
      this->Internals->simulationInputs->row(item));
    }
  dynamic_cast<pqCPExportStateWizardPage2*>(this->currentPage())->emitCompleteChanged();
}

//-----------------------------------------------------------------------------
bool pqCPExportStateWizard::validateCurrentPage()
{
  if (!this->Superclass::validateCurrentPage())
    {
    return false;
    }

  if (this->nextId() != -1)
    {
    // not yet done with the wizard.
    return true;
    }

  QString filters ="ParaView Python State Files (*.py);;All files (*)";

  pqFileDialog file_dialog (NULL, this,
    tr("Save Server State:"), QString(), filters);
  file_dialog.setObjectName("ExportCoprocessingStateFileDialog");
  file_dialog.setFileMode(pqFileDialog::AnyFile);
  if (!file_dialog.exec())
    {
    return false;
    }

  QString filename = file_dialog.getSelectedFiles()[0];

  // Last Page, export the state.
  pqPythonManager* manager = qobject_cast<pqPythonManager*>(
    pqApplicationCore::instance()->manager("PYTHON_MANAGER"));
  pqPythonDialog* dialog = 0;
  if (manager)
    {
    dialog = manager->pythonShellDialog();
    }
  if (!dialog)
    {
    qCritical("Failed to locate Python dialog. Cannot save state.");
    return true;
    }

  QString sim_inputs_map;
  for (int cc=0; cc < this->Internals->nameWidget->rowCount(); cc++)
    {
    QTableWidgetItem* item0 = this->Internals->nameWidget->item(cc, 0);
    QTableWidgetItem* item1 = this->Internals->nameWidget->item(cc, 1);
    sim_inputs_map +=
      QString(" '%1' : '%2',").arg(item0->text()).arg(item1->text());
    }
  // remove last ","
  sim_inputs_map.chop(1);
  
  QString export_rendering = "True";
  if (this->Internals->ignoreRendering->isChecked())
    {
    export_rendering = "False";
    }
  QString command =
    QString(cp_export_py).arg(export_rendering).arg(sim_inputs_map).arg(filename);
  dialog->runString(command);
  return true;
}
