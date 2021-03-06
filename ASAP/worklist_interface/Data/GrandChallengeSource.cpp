#include "GrandChallengeSource.h"

#include <algorithm>
#include <codecvt>
#include <stdexcept>
#include <locale>
#include <system_error>
#include <cstdio>

#include "../Misc/StringConversions.h"
#include "../Networking/HTTP_File_Download.h"
#include "../Serialization/JSON.h"

// TODO: Grand Challenge doesn't offer schema's for it's API yet, which is why it's being loaded
//		 through actual records currently. Once schema's are made available through the API, the
//		 refresh tables function should be refactored.

namespace ASAP
{
	GrandChallengeSource::GrandChallengeSource(const GrandChallengeURLInfo uri_info, TemporaryDirectoryTracker& temp_dir, const Django_Connection::Credentials credentials, const web::http::client::http_client_config& config)
		: m_connection_(uri_info.base_url, Django_Connection::AuthenticationType::TOKEN, credentials, config), m_rest_uri_(uri_info), m_schemas_(4), m_temporary_directory_(temp_dir)
	{
		RefreshTables_();
	}

	WorklistSourceInterface::SourceType GrandChallengeSource::GetSourceType(void)
	{
		return WorklistSourceInterface::SourceType::FULL_WORKLIST;
	}

	GrandChallengeURLInfo GrandChallengeSource::GetStandardURI(const std::wstring base_url)
	{
		return { base_url, L"api/v1/worklists/", L"api/v1/patients/", L"api/v1/studies/", L"api/v1/cases/images/" };
	}

	size_t GrandChallengeSource::AddWorklistRecord(const std::string& title, const std::function<void(const bool)>& observer)
	{
		std::wstringstream body;
		body << L"{ \"title\": \"" << Misc::StringToWideString(title) << "\", \"images\": [] }";

		web::http::http_request request(web::http::methods::POST);
		request.set_request_uri(L"/" + m_rest_uri_.worklist_addition);
		request.set_body(body.str(), L"application/json");

		return m_connection_.QueueRequest(request, [observer](web::http::http_response& response)
		{
			observer(response.status_code() == web::http::status_codes::Created);
		});
	}

	size_t GrandChallengeSource::UpdateWorklistRecord(const std::string& worklist_index, const std::string title, const std::set<std::string> images, const std::function<void(const bool)>& observer)
	{
		std::wstringstream body;
		body << L"{ \"title\": \"" << Misc::StringToWideString(title) << "\", \"images\": [ ";

		for (auto it = images.begin(); it != images.end(); ++it)
		{
			body << L"\"" << Misc::StringToWideString(*it) << L"\"";
			if (it != --images.end())
			{
				body << L",";
			}
		}
		body << L" ] }";

		std::wstringstream url;
		url << L"/" << m_rest_uri_.worklist_addition << Misc::StringToWideString(worklist_index) << L"/";

		web::http::http_request request(web::http::methods::PATCH);
		request.set_request_uri(url.str());
		request.set_body(body.str(), L"application/json");

		return m_connection_.QueueRequest(request, [observer](web::http::http_response& response)
		{
			observer(response.status_code() == web::http::status_codes::OK);
		});
	}

	size_t GrandChallengeSource::DeleteWorklistRecord(const std::string& worklist_index, const std::function<void(const bool)>& observer)
	{
		std::wstringstream url;
		url << L"/" << m_rest_uri_.worklist_addition << Misc::StringToWideString(worklist_index) << L"/";

		web::http::http_request request(web::http::methods::DEL);
		request.set_request_uri(url.str());

		return m_connection_.QueueRequest(request, [observer](web::http::http_response& response)
		{
			observer(response.status_code() == web::http::status_codes::NoContent);
		});
	}

	size_t GrandChallengeSource::GetWorklistRecords(const std::function<void(DataTable&, const int)>& receiver)
	{
		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(L"/" + m_rest_uri_.worklist_addition);

		return m_connection_.QueueRequest(request, [this, receiver](web::http::http_response& response)
		{
			// Parses the worklists into a data table.
			DataTable worklists(this->m_schemas_[TableEntry::WORKLIST]);
			int error_code = JSON::ResponseToTable(this->m_connection_, response, worklists);
			receiver(worklists, error_code);
		});
	}

	size_t GrandChallengeSource::GetPatientRecords(const std::string& worklist_index, const std::function<void(DataTable&, const int)>& receiver)
	{
		std::wstringstream url;
		url << L"/" << m_rest_uri_.patient_addition;
		
		if (!worklist_index.empty())
		{
			url << L"?worklist=" << Misc::StringToWideString(worklist_index);
		}

		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(url.str());

		return m_connection_.QueueRequest(request,[this, receiver](web::http::http_response& response)
		{
			DataTable& patient_schema = this->m_schemas_[TableEntry::PATIENT];
			DataTable patients(this->m_schemas_[TableEntry::PATIENT]);

			// Parses the patients into a data table.
			int error_code = JSON::ResponseToTable(this->m_connection_, response, patients);

			// TODO: Remove once Grand Challenge supports schema's
			if (patients.Size() > 0 && patient_schema.GetColumnCount() == 0)
			{
				this->RefreshTables_();
			}
			
			receiver(patients, error_code);
		});
	}

	size_t GrandChallengeSource::GetStudyRecords(const std::string& patient_index, const std::function<void(DataTable&, const int)>& receiver)
	{
		std::wstringstream url;
		url << L"/" << m_rest_uri_.study_addition << L"?patient=" << Misc::StringToWideString(patient_index);

		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(url.str());

		return m_connection_.QueueRequest(request, [this, receiver](web::http::http_response& response)
		{
			DataTable& study_schema = this->m_schemas_[TableEntry::STUDY];
			DataTable studies(this->m_schemas_[TableEntry::STUDY]);

			// Parses the studies into a data table.
			int error_code = JSON::ResponseToTable(this->m_connection_, response, studies);

			// TODO: Remove once Grand Challenge supports schema's
			if (studies.Size() > 0 && study_schema.GetColumnCount() == 0)
			{
				this->RefreshTables_();
			}

			receiver(studies, error_code);
		});
	}

	size_t GrandChallengeSource::GetImageRecords(const std::string& worklist_index, const std::string& study_index, const std::function<void(DataTable&, int)>& receiver)
	{
		std::wstringstream url;
		url << L"/" << m_rest_uri_.image_addition;

		// TODO: clean this up.
		if (!worklist_index.empty() || !study_index.empty())
		{
			url << L"?";

			if (!study_index.empty())
			{
				url << L"study=" << Misc::StringToWideString(study_index);
			}
			if (!worklist_index.empty())
			{
				if (!study_index.empty())
				{
					url << L"&";
				}

				url << L"worklist=" << Misc::StringToWideString(worklist_index);
			}
		}

		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(url.str());

		return m_connection_.QueueRequest(request, [this, receiver](web::http::http_response& response)
		{
			DataTable images(this->m_schemas_[TableEntry::IMAGE]);
			int error_code = JSON::ResponseToFilteredTable(this->m_connection_, response, images, std::vector<std::string>({ "pk", "name"}));
			receiver(images, error_code);
		});
	}

	size_t GrandChallengeSource::GetImageThumbnailFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer)
	{
		/*std::wstringstream url;
		url << L"/" << m_rest_uri_.image_addition << L"/" << Misc::StringToWideString(image_index) << L"/";

		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(url.str());
		*/
		receiver(boost::filesystem::path());
		observer(100);
		return 0;
	}

	size_t GrandChallengeSource::GetImageFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer)
	{
		std::wstringstream url;
		url << L"/" << m_rest_uri_.image_addition << Misc::StringToWideString(image_index) << L"/";

		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(url.str());

		return m_connection_.QueueRequest(request, [receiver, observer, connection=&m_connection_, temp_dir=&m_temporary_directory_](web::http::http_response& response)
		{
			// Acquires the URL to the file.
			if (response.status_code() == web::http::status_codes::OK)
			{
				web::json::value json(response.extract_json().get());

				std::wstring file_name	= json.at(L"name").serialize();
				std::wstring file_uri	= json.at(L"files").as_array()[0].at(L"file").serialize();
				file_name.erase(std::remove(file_name.begin(), file_name.end(), L'"'), file_name.end());
				file_uri.erase(std::remove(file_uri.begin(), file_uri.end(), L'"'), file_uri.end());

				// Creates a request to acquire the file.
				web::http::http_request image_file_request(web::http::methods::GET);
				image_file_request.set_request_uri(file_uri);

				// Acquire the image or return an empty path if it fails.
				try
				{
					web::http::http_response image_file_response = connection->SendRequest(image_file_request).get();

					// Download the image, link the status to the observer and return the results.
					receiver(HTTP_File_Download(image_file_response, temp_dir->GetAbsolutePath(), Misc::WideStringToString(file_name), observer));
				}
				catch (const std::exception& e)
				{
					// TODO: implement method to reveal errors to user.
					receiver(boost::filesystem::path());
				}
			}
		});
	}

	std::set<std::string> GrandChallengeSource::GetWorklistHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return m_schemas_[TableEntry::WORKLIST].GetColumnNames(selection);
	}

	std::set<std::string> GrandChallengeSource::GetPatientHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return m_schemas_[TableEntry::PATIENT].GetColumnNames(selection);
	}

	std::set<std::string> GrandChallengeSource::GetStudyHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return m_schemas_[TableEntry::STUDY].GetColumnNames(selection);
	}

	std::set<std::string> GrandChallengeSource::GetImageHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return m_schemas_[TableEntry::IMAGE].GetColumnNames(selection);
	}

	void GrandChallengeSource::CancelTask(size_t id)
	{
		m_connection_.CancelTask(id);
	}

	void GrandChallengeSource::RefreshTables_(void)
	{
		// Acquires Patients schema.
		web::http::http_request request(web::http::methods::GET);
		request.set_request_uri(L"/" + m_rest_uri_.patient_addition);

		m_connection_.SendRequest(request).then([this](web::http::http_response& response)
		{
			JSON::ResponseToTable(this->m_connection_, response, m_schemas_[TableEntry::PATIENT]);
			m_schemas_[TableEntry::PATIENT].Clear();
		}).wait();

		// Acquires the Studies schema
		request = web::http::http_request(web::http::methods::GET);
		request.set_request_uri(L"/" + m_rest_uri_.study_addition);

		m_connection_.SendRequest(request).then([this](web::http::http_response& response)
		{
			JSON::ResponseToTable(this->m_connection_, response, m_schemas_[TableEntry::STUDY]);
			m_schemas_[TableEntry::STUDY].Clear();
		}).wait();

		// Acquires Worklists schema.
		request = web::http::http_request(web::http::methods::OPTIONS);
		request.set_request_uri(L"/" + m_rest_uri_.worklist_addition);

		DataTable* datatable(&m_schemas_[TableEntry::WORKLIST]);
		m_connection_.SendRequest(request).then([datatable](web::http::http_response& response)
		{
			JSON::OptionsResponseToTableSchema(response, *datatable);
		}).wait();

		// The image table supplies the OPTIONS request differently, hence why it'll be treated differently.
		m_schemas_[TableEntry::IMAGE] = DataTable({ "id", "title" });

		// Defines which fields the user should be able to see. Only required for Patient and Study records
		// TODO: Clean this up
		std::set<std::string> patient_headers(m_schemas_[TableEntry::PATIENT].GetColumnNames());
		for (const std::string& header : patient_headers)
		{
			if (header != "name")
			{
				m_schemas_[TableEntry::PATIENT].SetColumnAsInvisible(header);
			}
		}

		std::set<std::string> study_headers(m_schemas_[TableEntry::STUDY].GetColumnNames());
		for (const std::string& header : study_headers)
		{
			if (header != "name")
			{
				m_schemas_[TableEntry::STUDY].SetColumnAsInvisible(header);
			}
		}
	}
}