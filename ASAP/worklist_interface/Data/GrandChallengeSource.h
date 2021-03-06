#ifndef __ASAP_DATA_GRANDCHALLENGESOURCE__
#define __ASAP_DATA_GRANDCHALLENGESOURCE__

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "DataTable.h"
#include "WorklistSourceInterface.h"
#include "../Networking/Django_Connection.h"
#include "../Misc/TemporaryDirectoryTracker.h"

namespace ASAP
{
	struct GrandChallengeURLInfo
	{
		std::wstring base_url;
		std::wstring worklist_addition;
		std::wstring patient_addition;
		std::wstring study_addition;
		std::wstring image_addition;
	};

	class GrandChallengeSource : public WorklistSourceInterface
	{
		public:
			GrandChallengeSource(const GrandChallengeURLInfo uri_info, TemporaryDirectoryTracker& temp_dir, const Django_Connection::Credentials credentials, const web::http::client::http_client_config& config = web::http::client::http_client_config());

			static GrandChallengeURLInfo GetStandardURI(const std::wstring base_url);
			WorklistSourceInterface::SourceType GetSourceType(void);

			size_t AddWorklistRecord(const std::string& title, const std::function<void(const bool)>& observer);
			size_t UpdateWorklistRecord(const std::string& worklist_index, const std::string title, const std::set<std::string> images, const std::function<void(const bool)>& observer);
			size_t DeleteWorklistRecord(const std::string& worklist_index, const std::function<void(const bool)>& observer);

			size_t GetWorklistRecords(const std::function<void(DataTable&, const int)>& receiver);
			size_t GetPatientRecords(const std::string& worklist_index, const std::function<void(DataTable&, const int)>& receiver);
			size_t GetStudyRecords(const std::string& patient_index, const std::function<void(DataTable&, const int)>& receiver);
			size_t GetImageRecords(const std::string& worklist_index, const std::string& study_index, const std::function<void(DataTable&, const int)>& receiver);

			size_t GetImageThumbnailFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer);
			size_t GetImageFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer);

			std::set<std::string> GetWorklistHeaders(const DataTable::FIELD_SELECTION selection = DataTable::FIELD_SELECTION::ALL);
			std::set<std::string> GetPatientHeaders(const DataTable::FIELD_SELECTION selection = DataTable::FIELD_SELECTION::ALL);
			std::set<std::string> GetStudyHeaders(const DataTable::FIELD_SELECTION selection = DataTable::FIELD_SELECTION::ALL);
			std::set<std::string> GetImageHeaders(const DataTable::FIELD_SELECTION selection = DataTable::FIELD_SELECTION::ALL);

			void CancelTask(size_t id);

		private:
			enum TableEntry { WORKLIST, PATIENT, STUDY, IMAGE };

			Django_Connection			m_connection_;
			GrandChallengeURLInfo		m_rest_uri_;
			std::vector<DataTable>		m_schemas_;
			TemporaryDirectoryTracker&	m_temporary_directory_;

			void RefreshTables_(void);
	};
}
#endif // __ASAP_DATA_GRANDCHALLENGESOURCE__