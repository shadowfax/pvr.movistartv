#include "MovistarTV.h"
#include "MovistarTVDVBSTP.h"
#include "rapidxml/rapidxml.hpp"
#include <sstream>

using namespace ADDON;
using namespace rapidxml;

MovistarTV::MovistarTV(void)
{
	bootstrap();
}

MovistarTV::~MovistarTV(void)
{
	m_groups.clear();
	m_channels.clear();
}

void MovistarTV::bootstrap(void)
{
	PLATFORM::CLockObject lock(m_mutex);

	MovistarTVRPCPlatformProfile platform_profile;
	MovistarTVRPC *rpc = new MovistarTVRPC;

	if (!rpc->GetClientProfile(m_clientProfile))
	{
		delete rpc;
		return;
	}

	if (!rpc->GetPlatformProfile(platform_profile))
	{
		delete rpc;
		return;
	}

	delete rpc;

	/* Get the demarcation entry point */
	std::string dvb_entry_point_host = platform_profile.dvbConfig.dvbEntryPoint;
	int dvb_entry_point_port = 3937;
	size_t colonPos = platform_profile.dvbConfig.dvbEntryPoint.find(':');
	if (colonPos != std::string::npos)
	{
		std::string hostPart = platform_profile.dvbConfig.dvbEntryPoint.substr(0, colonPos);
		std::string portPart = platform_profile.dvbConfig.dvbEntryPoint.substr(colonPos + 1);

		std::istringstream ss(portPart);
		int port = 0;
		ss >> port;

		if (!ss.fail())
		{
			// hostname in hostPart, port in port
			// could check port >= 0 and port < 65536 here
			dvb_entry_point_host = hostPart;
			dvb_entry_point_port = port;
		}
	}

	/* Now we go multicast */
	std::string raw_xml;
	MovistarTVDVBSTP *dvb_stp = new MovistarTVDVBSTP;

	if (!dvb_stp->GetServiceProviderDiscovery(dvb_entry_point_host, dvb_entry_point_port, raw_xml))
	{
		delete dvb_stp;
		return;
	}

	delete dvb_stp;

	/* Got the ServiceProviderDiscovery XML */
	/* Find my demarcation entry point */
	std::string dem_domain = "DEM_";
	std::ostringstream oss;
	oss << m_clientProfile.iDemarcation;
	dem_domain.append(oss.str());
	dem_domain.append(".");
	dem_domain.append(platform_profile.dvbConfig.dvbServiceProvider);

	XBMC->Log(LOG_DEBUG, "Demarcation domain is %s", dem_domain.c_str());

	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)raw_xml.c_str());
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse ServiceProviderDiscovery XML: %s", p.what());
		return;
	}

	xml_node<> *pRootElement = xmlDoc.first_node("ServiceDiscovery");
	if (!pRootElement)
	{
		XBMC->Log(LOG_ERROR, "Invalid SD&S XML: no <ServiceDiscovery> tag found");
		return;
	}

	xml_node<> *pMainElement = pRootElement->first_node("ServiceProviderDiscovery");
	if (!pMainElement)
	{
		XBMC->Log(LOG_ERROR, "Invalid SD&S XML: no <ServiceProviderDiscovery> tag found");
		return;
	}

	bool demarcation_found = false;

	xml_node<> *pServiceProviderNode = NULL;
	for (pServiceProviderNode = pMainElement->first_node("ServiceProvider"); pServiceProviderNode; pServiceProviderNode = pServiceProviderNode->next_sibling("ServiceProvider"))
	{
		std::string domain_name;
		xml_attribute<> *pAttribute = pServiceProviderNode->first_attribute("DomainName");
		if (pAttribute == NULL)
		{
			XBMC->Log(LOG_ERROR, "Invalid SD&S XML: no 'DomainName' attribute inside <ServiceProvider> tag");
			return;
		}
		domain_name = pAttribute->value();

		if (dem_domain == domain_name) 
		{
			demarcation_found = true;

			xml_node<> *pOfferingNode = pServiceProviderNode->first_node("Offering");
			if (!pOfferingNode)
			{
				XBMC->Log(LOG_ERROR, "No offerings for your service provider\n");
				return;
			}
			for (pOfferingNode; pOfferingNode; pOfferingNode = pOfferingNode->next_sibling("Offering"))
			{
				xml_node<> *pPushNode = pOfferingNode->first_node("Push");
				if (!pPushNode)
				{
					XBMC->Log(LOG_ERROR, "No <Push> tag found for the offering\n");
					/* Do not exits, maybe there is another offer */
				}
				else {
					for (pPushNode; pPushNode; pPushNode = pPushNode->next_sibling("Push"))
					{
						MovistarTVAddress push_address;
						push_address.port = 3937;

						xml_attribute<> *pPushAddressAttribute = pPushNode->first_attribute("Address");
						if (pPushAddressAttribute == NULL)
						{
							continue;
						}
						push_address.address = pPushAddressAttribute->value();

						xml_attribute<> *pPushPortAttribute = pPushNode->first_attribute("Port");
						if (pPushPortAttribute == NULL)
						{
							push_address.port = 3937;
						}
						else
						{
							/*push_offering.port = std::stoi(pPushPortAttribute->value());*/
							std::string port_value = pPushPortAttribute->value();

							std::istringstream ss(port_value);
							int port = 0;
							ss >> port;

							if (!ss.fail())
							{
								// hostname in hostPart, port in port
								// could check port >= 0 and port < 65536 here
								push_address.port = port;
							}
						}

						XBMC->Log(LOG_DEBUG, "Added offering address for your demarcation: %s:%d", push_address.address.c_str(), push_address.port);
						m_offerings.push_back(push_address);
					}
				}
			}

			/* exit for loop */
			break;
		}
	} // end FOR
}

int MovistarTV::GetChannelsAmount(void)
{
	return m_channels.size();
}

bool MovistarTV::LoadChannels(void)
{
	PLATFORM::CLockObject lock(m_mutex);

	int iUniqueChannelId = 1;
	int iUniqueChannelGroupId = 1;
	MovistarTVDVBSTP *dvb_stp = new MovistarTVDVBSTP;
	std::vector<DVBSTPPackage> dvbPackages;
	std::vector<DVBSTPSingleService> dvbServices;

	/* Try to get the channels list */
	for (unsigned int iOfferingPtr = 0; iOfferingPtr < m_offerings.size(); iOfferingPtr++)
	{
		std::string raw_xml;
		std::string raw_xml_package;

		MovistarTVAddress &address = m_offerings.at(iOfferingPtr);

		// TODO: The following line retrieves EPG information (Links to other multicast addresses)
		// dvb_stp->GetBCGDiscovery(address.address, address.port);

		dvbPackages = dvb_stp->GetPackageDiscovery(address.address, address.port);
		XBMC->Log(LOG_DEBUG, "Got %d DVB Packages", dvbPackages.size());

		dvbServices = dvb_stp->GetBroadcastDiscovery(address.address, address.port);

		XBMC->Log(LOG_DEBUG, "Start parsing DVB services");
		for (unsigned int iDVBServicePtr = 0; iDVBServicePtr < dvbServices.size(); iDVBServicePtr++)
		{
			DVBSTPSingleService &singleService = dvbServices.at(iDVBServicePtr);

			std::string strStreamURL = "rtp://@";
			strStreamURL.append(singleService.serviceLocation.address);
			strStreamURL.append(":");
			std::ostringstream convert;
			convert << singleService.serviceLocation.port;
			strStreamURL.append(convert.str());

			std::string strIconPath = "http://172.26.22.23:2001/appclient/incoming/epg/";
			strIconPath.append(singleService.textualIdentifier.logoURL);

			PVRMovistarTVChannel channel;

			/* Get the logical channel number */
			bool serviceFoundInPackage = false;
			for (unsigned int iClientPackagePtr = 0; iClientPackagePtr < m_clientProfile.tvPackages.size(); iClientPackagePtr++)
			{
				for (unsigned int iDVBPackagePtr = 0; iDVBPackagePtr < dvbPackages.size(); iDVBPackagePtr++)
				{
					DVBSTPPackage &package = dvbPackages.at(iDVBPackagePtr);

					for (unsigned int iDVBPackageServicePtr = 0; iDVBPackageServicePtr < package.services.size(); iDVBPackageServicePtr++)
					{
						DVBSTPPackageService &service = package.services.at(iDVBPackageServicePtr);

						if (service.serviceName == singleService.textualIdentifier.serviceName)
						{
							channel.iChannelNumber = service.logicalChannelNumber;
							serviceFoundInPackage = true;
							break;
						}
					}

					if (serviceFoundInPackage)
					{
						break;
					}
				}

				if (serviceFoundInPackage)
				{
					break;
				}
			}

			channel.iUniqueId = iUniqueChannelId;
			channel.bIsRadio = false;
			if (!serviceFoundInPackage)
			{
				channel.iChannelNumber = iUniqueChannelId;
			}
			channel.iSubChannelNumber = 0;
			channel.strChannelName = singleService.serviceInfo.name;
			channel.strStreamURL = strStreamURL;
			channel.iEncryptionSystem = 0;
			channel.strIconPath = strIconPath;

			m_channels.push_back(channel);

			iUniqueChannelId++;

			/* Channel groups */
			bool channel_group_found = false;
			for (unsigned int iChannelGroupPtr = 0; iChannelGroupPtr < m_groups.size(); iChannelGroupPtr++)
			{
				PVRMovistarTVChannelGroup &channelGroup = m_groups.at(iChannelGroupPtr);

				if (channelGroup.strGroupName == singleService.serviceInfo.genre.urn_name)
				{
					channelGroup.members.push_back(channel.iUniqueId);
					channel_group_found = true;
				}

			}

			if (!channel_group_found) 
			{
				PVRMovistarTVChannelGroup channelGroup;

				channelGroup.iGroupId = iUniqueChannelGroupId;
				channelGroup.strGroupName = singleService.serviceInfo.genre.urn_name;
				channelGroup.members.push_back(channel.iUniqueId);

				m_groups.push_back(channelGroup);

				iUniqueChannelGroupId++;
			}
		}
	}

	delete dvb_stp;

	return true;
}

PVR_ERROR MovistarTV::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
	if (m_channels.size() <= 0)
	{
		LoadChannels();
	}

	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRMovistarTVChannel &mvtvChannel = m_channels.at(iChannelPtr);

		PVR_CHANNEL xbmcChannel;
		memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

		xbmcChannel.iUniqueId = mvtvChannel.iUniqueId;
		xbmcChannel.bIsRadio = mvtvChannel.bIsRadio;
		xbmcChannel.iChannelNumber = mvtvChannel.iChannelNumber;
		xbmcChannel.iSubChannelNumber = mvtvChannel.iSubChannelNumber;
		strncpy(xbmcChannel.strChannelName, mvtvChannel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
		strncpy(xbmcChannel.strStreamURL, mvtvChannel.strStreamURL.c_str(), sizeof(xbmcChannel.strStreamURL) - 1);
		xbmcChannel.iEncryptionSystem = 0;
		strncpy(xbmcChannel.strIconPath, mvtvChannel.strIconPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
		xbmcChannel.bIsHidden = false;

		PVR->TransferChannelEntry(handle, &xbmcChannel);
	}

	return PVR_ERROR_NO_ERROR;
}

bool MovistarTV::GetChannel(const PVR_CHANNEL &channel, PVRMovistarTVChannel &myChannel)
{
	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRMovistarTVChannel &thisChannel = m_channels.at(iChannelPtr);
		if (thisChannel.iUniqueId == (int)channel.iUniqueId)
		{
			myChannel.iUniqueId = thisChannel.iUniqueId;
			myChannel.bIsRadio = thisChannel.bIsRadio;
			myChannel.iChannelNumber = thisChannel.iChannelNumber;
			myChannel.iSubChannelNumber = thisChannel.iSubChannelNumber;
			myChannel.iEncryptionSystem = thisChannel.iEncryptionSystem;
			myChannel.strChannelName = thisChannel.strChannelName;
			myChannel.strIconPath = thisChannel.strIconPath;
			myChannel.strStreamURL = thisChannel.strStreamURL;

			return true;
		}
	}
	return false;
}

int MovistarTV::GetChannelGroupsAmount(void)
{
	return m_groups.size();
}

PVR_ERROR MovistarTV::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
	if (m_channels.size() <= 0)
	{
		LoadChannels();
	}

	for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
	{
		PVRMovistarTVChannelGroup &group = m_groups.at(iGroupPtr);
		
		PVR_CHANNEL_GROUP xbmcGroup;
		memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

		xbmcGroup.bIsRadio = bRadio;
		strncpy(xbmcGroup.strGroupName, group.strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

		PVR->TransferChannelGroup(handle, &xbmcGroup);
	}

	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR MovistarTV::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
	for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
	{
		PVRMovistarTVChannelGroup &myGroup = m_groups.at(iGroupPtr);
		if (!strcmp(myGroup.strGroupName.c_str(), group.strGroupName))
		{
			for (unsigned int iChannelPtr = 0; iChannelPtr < myGroup.members.size(); iChannelPtr++)
			{
				int iId = myGroup.members.at(iChannelPtr) - 1;
				if (iId < 0 || iId >(int)m_channels.size() - 1)
					continue;

				PVRMovistarTVChannel &channel = m_channels.at(iId);
				PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
				memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

				strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
				xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
				xbmcGroupMember.iChannelNumber = channel.iChannelNumber;

				PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
			}
		}
	}

	return PVR_ERROR_NO_ERROR;
}