#include "MovistarTVRPC.h"

using namespace ADDON;

MovistarTVRPC::MovistarTVRPC(void)
{
}

MovistarTVRPC::~MovistarTVRPC(void)
{
}

bool MovistarTVRPC::GetClientProfile(MovistarTVRPCClientProfile& profile)
{
	Json::Value response;

	if (!GetJsonData("http://172.26.22.23:2001/appserver/mvtv.do?action=getClientProfile", response)) {
		XBMC->Log(LOG_ERROR, "Failed to get the client profile.");
		return false;
	}

	/* Demarcation */
	Json::Value demarcation = response.get("demarcation", NULL);
	if (!demarcation.isInt()) 
	{
		XBMC->Log(LOG_DEBUG, "Client demarcation is expected to be an integer.");
		return false;
	}
	profile.iDemarcation = demarcation.asInt();

	/* TV Packages */
	Json::Value tv_packages = response.get("tvPackages", NULL);
	if (!tv_packages.isString())
	{
		XBMC->Log(LOG_DEBUG, "Client tvPackages is expected to be an string.");
		return false;
	}

	char *tv_packages_token;

	tv_packages_token = strtok((char *)tv_packages.asCString(), "|");
	while (tv_packages_token != NULL)
	{
		profile.tvPackages.push_back(std::string((const char*)tv_packages_token));
		tv_packages_token = strtok(NULL, "|");
	}

	return true;
}

bool MovistarTVRPC::GetPlatformProfile(MovistarTVRPCPlatformProfile& profile)
{
	Json::Value response;

	if (!GetJsonData("http://172.26.22.23:2001/appserver/mvtv.do?action=getPlatformProfile", response)) {
		XBMC->Log(LOG_ERROR, "Failed to get the platform profile.");
		return false;
	}

	/* DVB Config */
	Json::Value dvb_config = response.get("dvbConfig", NULL);
	if (dvb_config.type() != Json::objectValue) {
		XBMC->Log(LOG_ERROR, "Unknown response format. dvbConfig.");
		return false;
	}

	Json::Value dvb_service_provider = dvb_config.get("dvbServiceProvider", "imagenio.es");
	if (!dvb_service_provider.isString()) {
		XBMC->Log(LOG_ERROR, "Unknown response format. dvbServiceProvider.");
		return false;
	}
	profile.dvbConfig.dvbServiceProvider = dvb_service_provider.asString();

	Json::Value dvb_entry_point = dvb_config.get("dvbEntryPoint", NULL);
	if (!dvb_entry_point.isString()) {
		XBMC->Log(LOG_ERROR, "Unknown response format. dvbEntryPoint.");
		return false;
	}
	profile.dvbConfig.dvbEntryPoint = dvb_entry_point.asString();

	return true;
}

bool MovistarTVRPC::GetData(const std::string& url, std::string& return_value)
{
	bool retval = false;

	XBMC->Log(LOG_DEBUG, "URL: %s\n", url.c_str());

	void* hFile = XBMC->OpenFileForWrite(url.c_str(), 0);
	if (hFile != NULL)
	{
		int rc = XBMC->WriteFile(hFile, "", 0);
		if (rc >= 0)
		{
			std::string result;
			result.clear();
			char buffer[1024];
			while (XBMC->ReadFileString(hFile, buffer, 1023))
				result.append(buffer);

			return_value = result;
			retval = true;
		}
		else
		{
			XBMC->Log(LOG_ERROR, "can not write to %s", url.c_str());
		}

		XBMC->CloseFile(hFile);
	}
	else
	{
		XBMC->Log(LOG_ERROR, "can not open %s for write", url.c_str());
	}

	return retval;
}


bool MovistarTVRPC::GetJsonData(const std::string& url, Json::Value& return_value)
{
	std::string str_response;

	if (!GetData(url, str_response)) 
	{
		return false;
	}

	if (str_response.length() == 0) 
	{
		XBMC->Log(LOG_ERROR, "Empty response");
		return false;
	}

	Json::Value json_response;
	Json::Reader reader;
	if (!reader.parse(str_response, json_response))
	{
		XBMC->Log(LOG_ERROR, "Failed to parse: %s\n", str_response.substr(0, 512).c_str());
		return false;
	}

	if (json_response.type() != Json::objectValue) 
	{
		XBMC->Log(LOG_ERROR, "Unknown JSON response format. Expected Json::objectValue\n");
		return false;
	}

	Json::Value result_code = json_response.get("resultCode", -1);
	if (!result_code.isInt()) 
	{
		XBMC->Log(LOG_ERROR, "Unknown response format. resultCode.");
		return false;
	}

	Json::Value result_text = json_response.get("resultText", "Missing resultText");
	if (!result_text.isString()) 
	{
		XBMC->Log(LOG_ERROR, "Unknown response format. resultText.");
		return false;
	}

	if (0 != result_code.asInt())
	{
		XBMC->Log(LOG_ERROR, "JSON Error: %s", result_text.asCString());
		return false;
	}
	else 
	{
		XBMC->Log(LOG_DEBUG, "JSON Result: %s", result_text.asCString());
	}
	
	Json::Value result_data = json_response.get("resultData", NULL);
	if (result_data.type() != Json::objectValue) {
		XBMC->Log(LOG_ERROR, "Unknown response format. resultData.");
		return false;
	}

	return_value = result_data;

	return true;
}