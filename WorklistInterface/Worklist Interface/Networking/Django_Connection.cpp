#include "Django_Connection.h"

#include <stdexcept>

namespace ASAP::Worklist::Networking
{
	Django_Connection::Django_Connection(const std::wstring base_uri, const AUTHENTICATION_TYPE authentication_type, const Credentials credentials)
		: HTTP_Connection(base_uri), m_authentication_(authentication_type), m_credentials_(credentials), m_status_(UNAUTHENTICATED)
	{
		SetupConnection_();
	}

	Django_Connection::Credentials Django_Connection::CreateCredentials(const std::wstring token)
	{
		Credentials credentials;
		credentials.insert({ "token", token });
		return credentials;
	}

	Django_Connection::Credentials Django_Connection::CreateCredentials(const std::wstring username, const std::wstring password)
	{
		Credentials credentials;
		credentials.insert({ "username", username });
		credentials.insert({ "password", password });
		return credentials;
	}

	void Django_Connection::SetCredentials(const Credentials credentials)
	{
		m_access_mutex$.lock();
		CancelAllTasks$();
		m_credentials_ = credentials;
		SetupConnection_();
		m_access_mutex$.unlock();
	}

	Django_Connection::AUTHENTICATION_STATUS Django_Connection::GetAuthenticationStatus(void)
	{
		return m_status_;
	}

	/// <summary>
	/// Allows the connection to handle the request, and returns the information to the passed observer function.
	/// </summary>
	size_t Django_Connection::QueueRequest(const web::http::http_request& request, std::function<void(web::http::http_response&)> observer)
	{
		return HTTP_Connection::QueueRequest(request, observer);
	}

	/// <summary>
	/// Sends the request through the internal client and returns the async task handling the request.
	/// </summary>
	pplx::task<web::http::http_response> Django_Connection::SendRequest(const web::http::http_request& request)
	{
		return HTTP_Connection::SendRequest(request);
	}

	void Django_Connection::ModifyRequest_(web::http::http_request& request)
	{
		if (m_authentication_ == TOKEN)
		{
			request.headers().add(L"Authorization", m_credentials_.find("token")->second);
		}
		else if (m_authentication_ == SESSION)
		{

		}
	}

	void Django_Connection::SetupConnection_()
	{
		if (m_authentication_ == TOKEN)
		{
			auto token = m_credentials_.find("token");
			if (token == m_credentials_.end())
			{
				throw std::runtime_error("No token information available.");
			}

			web::http::http_request token_test(web::http::methods::GET);
			ModifyRequest_(token_test);
			token_test.set_request_uri(L"api/v1/");

			bool valid_token = false;
			HTTP_Connection::SendRequest(token_test).then([](const web::http::http_response& response)
			{
			}).wait();

			if (valid_token)
			{
				m_status_ = AUTHENTICATION_STATUS::AUTHENTICATED;
			}
			else
			{
				m_status_ = AUTHENTICATION_STATUS::INVALID_CREDENTIALS;
			}
		}
		else if (m_authentication_ == SESSION)
		{

		}

		m_status_ = AUTHENTICATED;
	}
}