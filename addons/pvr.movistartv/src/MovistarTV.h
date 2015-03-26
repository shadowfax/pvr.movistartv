#pragma once

#include <vector>
#include "platform/util/StdString.h"
#include "platform/threads/mutex.h"
#include "client.h"
#include "MovistarTVRPC.h"

struct PVRMovistarTVChannel
{
	bool                    bIsRadio;
	int                     iUniqueId;
	int                     iChannelNumber;
	int                     iSubChannelNumber;
	int                     iEncryptionSystem;
	std::string             strChannelName;
	std::string             strIconPath;
	std::string             strStreamURL;
	/* DVB */
	std::string	            serviceName;
	/* std::vector<PVRDemoEpgEntry> epg; */
};

struct MovistarTVAddress
{
	std::string address;
	unsigned short port;
};

class MovistarTV
{
	public:
		MovistarTV(void);
		virtual ~MovistarTV(void);

		virtual int GetChannelsAmount(void);
		virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
		virtual bool GetChannel(const PVR_CHANNEL &channel, PVRMovistarTVChannel &myChannel);

	protected:
		virtual void bootstrap(void);
		virtual bool LoadChannels(void);

	private:
		PLATFORM::CMutex m_mutex;
		MovistarTVRPCClientProfile        m_clientProfile;
		std::vector<MovistarTVAddress>    m_offerings;
		std::vector<PVRMovistarTVChannel> m_channels;
		
};