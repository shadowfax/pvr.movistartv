#pragma once
#include "platform/util/StdString.h"
#include "client.h"
#include <json/json.h>

struct MovistarTVRPCClientProfile
{
	int iDemarcation;
	std::vector<std::string> tvPackages;
};

struct MovistarTVRPCPlatformProfile
{
	struct MovistarTVRPCDVBConfig {
		std::string dvbServiceProvider;
		std::string dvbEntryPoint;
	} dvbConfig;
};

class MovistarTVRPC
{
	public:
		MovistarTVRPC(void);
		virtual ~MovistarTVRPC(void);

		virtual bool GetClientProfile(MovistarTVRPCClientProfile& profile);
		virtual bool GetPlatformProfile(MovistarTVRPCPlatformProfile& profile);

	protected:
		virtual bool GetData(const std::string& url, std::string& return_value);
		virtual bool GetJsonData(const std::string& url, Json::Value& return_value);
};