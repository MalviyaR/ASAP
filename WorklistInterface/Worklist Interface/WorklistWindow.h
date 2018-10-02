#pragma once

#include <memory>
#include <QtWidgets/QMainWindow>
#include <QStandardItemModel>

#include "Data/DjangoDataAcquisition.h"
#include "Data/DataTable.h"
#include "ui_WorklistWindowLayout.h"

#include "../../ASAP/pathologyworkstation.h"

namespace ASAP::Worklist::GUI
{
	class WorklistWindow : public QMainWindow
	{
		Q_OBJECT

		public:
			explicit WorklistWindow(Data::DjangoDataAcquisition* data_acquisition, QWidget* parent = 0);

			void SetWorklistItems(const DataTable& items, QStandardItemModel* model);
			void SetPatientsItems(const DataTable& items, QStandardItemModel* model);
			void SetStudyItems(const DataTable& items, QStandardItemModel* model);
			void SetImageItems(const DataTable& items, QStandardItemModel* model);

			PathologyWorkstation* workstation_window;

		private:
			std::unique_ptr<Ui::WorklistWindowLayout>		m_ui_;
			std::unique_ptr<Data::DjangoDataAcquisition>	m_data_acquisition_;

			QStandardItemModel* m_images_model_;
			QStandardItemModel* m_patients_model_;
			QStandardItemModel* m_studies_model_;
			QStandardItemModel* m_worklist_model_;

			QIcon CreateIcon_(const std::string absolute_filepath);
			void SetHeaders_(std::vector<std::string> headers, QStandardItemModel* model, QAbstractItemView* view);
			void SetSlots_(void);

		private slots:
			void OnWorklistClear_(QModelIndex index, int row, int column);
			void OnPatientsClear_(QModelIndex index, int row, int column);
			void OnStudyClear_(QModelIndex index, int row, int column);

			void OnWorklistSelect_(QModelIndex index);
			void OnPatientSelect_(QModelIndex index);
			void OnStudySelect_(QModelIndex index);
			void OnImageSelect_(QModelIndex index);
	};
}