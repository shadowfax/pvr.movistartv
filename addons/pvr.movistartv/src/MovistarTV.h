#pragma once

#include <vector>
#include "platform/util/StdString.h"
#include "platform/threads/mutex.h"
#include "client.h"
#include "MovistarTVRPC.h"
#include "MovistarTVDVBSTP.h"

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

struct PVRMovistarTVChannelGroup
{
	int              iGroupId;
	std::string      strGroupName;
	std::vector<int> members;
};

struct MovistarTVAddress
{
	std::string address;
	unsigned short port;
};

class MovistarTV : IDVBSTPCallBack
{
	public:
		MovistarTV(void);
		virtual ~MovistarTV(void);

		virtual int GetChannelsAmount(void);
		virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
		virtual bool GetChannel(const PVR_CHANNEL &channel, PVRMovistarTVChannel &myChannel);

		virtual int GetChannelGroupsAmount(void);
		virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
		virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

		virtual void OnXmlReceived(const SDSMulticastDeliveryPacket& file);
	protected:
		virtual void bootstrap(void);
		virtual bool LoadChannels(void);

		void ParseServiceProviderDiscovery(const std::string& data);
		void ParseBroadcastDiscoveryInformation(const std::string& data);
		void ParsePackageDiscoveryInformation(const std::string& data);
		void ParseBCGDiscoveryInformation(const std::string& data);

	private:
		PLATFORM::CMutex                       m_ChannelCacheMutex;
		PLATFORM::CMutex                       m_mutex;
		MovistarTVRPCClientProfile             m_clientProfile;
		MovistarTVRPCPlatformProfile           m_platformProfile;
		std::vector<MovistarTVAddress>         m_offerings;
		std::vector<PVRMovistarTVChannel>      m_channels;
		std::vector<PVRMovistarTVChannelGroup> m_groups;
};