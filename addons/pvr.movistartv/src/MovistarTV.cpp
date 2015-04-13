#include "MovistarTV.h"
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

	MovistarTVRPC *rpc = new MovistarTVRPC;

	if (!rpc->GetClientProfile(m_clientProfile))
	{
		delete rpc;
		return;
	}

	if (!rpc->GetPlatformProfile(m_platformProfile))
	{
		delete rpc;
		return;
	}

	delete rpc;

	/* Get the demarcation entry point */
	std::string dvb_entry_point_host = m_platformProfile.dvbConfig.dvbEntryPoint;
	int dvb_entry_point_port = 3937;
	size_t colonPos = m_platformProfile.dvbConfig.dvbEntryPoint.find(':');
	if (colonPos != std::string::npos)
	{
		std::string hostPart = m_platformProfile.dvbConfig.dvbEntryPoint.substr(0, colonPos);
		std::string portPart = m_platformProfile.dvbConfig.dvbEntryPoint.substr(colonPos + 1);

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
	std::vector<SDSMulticastDeliveryPacket> files;

	dvb_stp->setCallback(this);
	files = dvb_stp->GetAllXmlFiles(dvb_entry_point_host, dvb_entry_point_port);

	delete dvb_stp;
}

int MovistarTV::GetChannelsAmount(void)
{
	return m_channels.size();
}

bool MovistarTV::LoadChannels(void)
{
	PLATFORM::CLockObject lock(m_ChannelCacheMutex);

	int iUniqueChannelId = 1;
	int iUniqueChannelGroupId = 1;
	MovistarTVDVBSTP *dvb_stp = new MovistarTVDVBSTP;
	std::vector<DVBSTPPackage> dvbPackages;
	std::vector<DVBSTPSingleService> dvbServices;

	dvb_stp->setCallback(this);

	XBMC->Log(LOG_NOTICE, "Got %d offerings", m_offerings.size());
	/* Try to get the channels list */
	for (unsigned int iOfferingPtr = 0; iOfferingPtr < m_offerings.size(); iOfferingPtr++)
	{
		std::string raw_xml;
		std::string raw_xml_package;
		std::vector<SDSMulticastDeliveryPacket> files;
		MovistarTVAddress &address = m_offerings.at(iOfferingPtr);

		files = dvb_stp->GetAllXmlFiles(address.address, address.port);
	}

	delete dvb_stp;

	XBMC->Log(LOG_DEBUG, "Channels have been succesfully loaded");

	return true;
}

PVR_ERROR MovistarTV::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
	PLATFORM::CLockObject lock(m_mutex);

	if (m_channels.size() <= 0)
	{
		LoadChannels();
	}

	XBMC->Log(LOG_DEBUG, "Converting channels to KODI format");
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


void MovistarTV::ParseServiceProviderDiscovery(const std::string& data)
{
	/* Got the ServiceProviderDiscovery XML */
	/* Find my demarcation entry point */
	std::string dem_domain = "DEM_";
	std::ostringstream oss;
	oss << m_clientProfile.iDemarcation;
	dem_domain.append(oss.str());
	dem_domain.append(".");
	dem_domain.append(m_platformProfile.dvbConfig.dvbServiceProvider);

	XBMC->Log(LOG_NOTICE, "Demarcation domain is %s", dem_domain.c_str());

	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)data.c_str());
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
		}
	} // end FOR

	XBMC->Log(LOG_DEBUG, "Succesfully parsed Service Provider Discovery  Information");
}

void MovistarTV::ParseBroadcastDiscoveryInformation(const std::string& data)
{
	std::vector<DVBSTPSingleService> serviceList;

	/* Process the XML data */
	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)data.c_str());
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse BroadcastDiscovery XML: %s", p.what());
		return;
	}

	xml_node<> *pServiceDiscoveryRootElement = xmlDoc.first_node("ServiceDiscovery");
	if (!pServiceDiscoveryRootElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <ServiceDiscovery> tag");
		return;
	}

	xml_node<> *pBroadcastDiscoveryElement = pServiceDiscoveryRootElement->first_node("BroadcastDiscovery");
	if (!pBroadcastDiscoveryElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <BroadcastDiscovery> tag");
		return;
	}

	xml_node<> *pServiceListElement = pBroadcastDiscoveryElement->first_node("ServiceList");
	if (!pServiceListElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <ServiceList> tag");
		return;
	}

	xml_node<> *pSingleServiceNode = NULL;
	for (pSingleServiceNode = pServiceListElement->first_node("SingleService"); pSingleServiceNode; pSingleServiceNode = pSingleServiceNode->next_sibling("SingleService"))
	{
		std::string channelName;
		std::string serviceName;
		std::string streamUrl = "rtp://@";
		std::string iconPath = "http://172.26.22.23:2001/appclient/incoming/epg/";

		xml_node<> *pServiceLocationElement = pSingleServiceNode->first_node("ServiceLocation");
		if (!pServiceLocationElement)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <ServiceLocation> tag");
			continue;
		}

		xml_node<> *pIPMulticastAddressNode = pServiceLocationElement->first_node("IPMulticastAddress");
		if (!pIPMulticastAddressNode)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <IPMulticastAddress> tag");
			continue;
		}

		xml_attribute<> *pIPMulticastAddressAddressAttribute = pIPMulticastAddressNode->first_attribute("Address");
		if (pIPMulticastAddressAddressAttribute == NULL)
		{
			XBMC->Log(LOG_ERROR, "BroadcastServiceDiscovery <IPMulticastAddress> tag has no address attribute");
			continue;
		}
		streamUrl.append(pIPMulticastAddressAddressAttribute->value());

		xml_attribute<> *pIPMulticastAddressPortAttribute = pIPMulticastAddressNode->first_attribute("Port");
		if (pIPMulticastAddressPortAttribute == NULL)
		{
			streamUrl.append(":8208");
		}
		else
		{
			streamUrl.append(":");
			streamUrl.append(pIPMulticastAddressPortAttribute->value());
		}

		xml_node<> *pTextualIdentifierElement = pSingleServiceNode->first_node("TextualIdentifier");
		if (!pTextualIdentifierElement)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <TextualIdentifier> tag");
			continue;
		}

		xml_attribute<> *pLogoURIAttribute = pTextualIdentifierElement->first_attribute("logoURI");
		if (pLogoURIAttribute != NULL)
		{
			iconPath.append(pLogoURIAttribute->value() );
		}
		else 
		{
			iconPath.clear();
		}

		xml_attribute<> *pServiceNameAttribute = pTextualIdentifierElement->first_attribute("ServiceName");
		if (pServiceNameAttribute == NULL)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery <TextualIdentifier> has no ServiceName attribute");
			continue;
		}
		serviceName = pServiceNameAttribute->value();

		xml_node<> *pSIElement = pSingleServiceNode->first_node("SI");
		if (!pSIElement)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <SI> tag");
			continue;
		}

		xml_node<> *pSINameNode = pSIElement->first_node("Name");
		if (!pSINameNode)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery SingleService has no <Name> tag");
			continue;
		}
		channelName = pSINameNode->value();

		//xml_node<> *pSIDescriptionNode = pSIElement->first_node("Description");
		//if (pSIDescriptionNode)
		//{
		//	singleService.serviceInfo.description = pSIDescriptionNode->value();
		//}

		//xml_node<> *pSIGenreElement = pSIElement->first_node("Genre");
		//if (pSIGenreElement)
		//{
		//	xml_node<> *pSIGenreNameNode = pSIGenreElement->first_node("urn:Name");
		//	if (pSIGenreNameNode)
		//	{
		//		singleService.serviceInfo.genre.urn_name = pSIGenreNameNode->value();
		//	}
		//}

		/* Search in channels for this ServiceName */
		bool bChannelExists = false;
		for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
		{
			PVRMovistarTVChannel &thisChannel = m_channels.at(iChannelPtr);

			if (thisChannel.serviceName == serviceName)
			{
				bChannelExists = true;

				/* Update channel data */
				thisChannel.strChannelName = channelName;
				thisChannel.strIconPath = iconPath;
				thisChannel.strStreamURL = streamUrl;
			}
		}

		if (!bChannelExists)
		{
			/* Create a new channel */
			PVRMovistarTVChannel newChannel;

			newChannel.serviceName = serviceName;
			newChannel.bIsRadio = false;
			newChannel.strChannelName = channelName;
			newChannel.iUniqueId = m_channels.size() + 1;
			newChannel.iEncryptionSystem = 0;
			newChannel.iSubChannelNumber = 0;
			newChannel.strIconPath = iconPath;
			newChannel.strStreamURL = streamUrl;

			m_channels.push_back(newChannel);
		}
	}
}

void MovistarTV::ParsePackageDiscoveryInformation(const std::string& data)
{
	/* Process the XML data */
	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)data.c_str());
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse PackageDiscovery XML: %s", p.what());
		return;
	}

	xml_node<> *pServiceDiscoveryRootElement = xmlDoc.first_node("ServiceDiscovery");
	if (!pServiceDiscoveryRootElement)
	{
		XBMC->Log(LOG_ERROR, "PackageDiscovery has no <ServiceDiscovery> tag");
		return;
	}

	xml_node<> *pPackageDiscoveryElement = pServiceDiscoveryRootElement->first_node("PackageDiscovery");
	if (!pPackageDiscoveryElement)
	{
		XBMC->Log(LOG_ERROR, "PackageDiscovery has no <PackageDiscovery> tag");
		return;
	}

	xml_node<> *pPackageNode = NULL;
	for (pPackageNode = pPackageDiscoveryElement->first_node("Package"); pPackageNode; pPackageNode = pPackageNode->next_sibling("Package"))
	{
		//xml_attribute<> *pPackageIdAttribute = pPackageNode->first_attribute("Id");
		//if (pPackageIdAttribute != NULL)
		//{
		//	package.id = pPackageIdAttribute->value();
		//}

		//xml_node<> *pPackageNameNode = pPackageNode->first_node("PackageName");
		//if (!pPackageNameNode)
		//{
		//	XBMC->Log(LOG_ERROR, "Package has no <PackageName> tag");
		//	continue;
		//}
		//package.packageName = pPackageNameNode->value();

		xml_node<> *pServiceNode = NULL;
		for (pServiceNode = pPackageNode->first_node("Service"); pServiceNode; pServiceNode = pServiceNode->next_sibling("Service"))
		{
			std::string serviceName;
			int logicalChannelNumber;

			xml_node<> *pTextualIdNode = pServiceNode->first_node("TextualID");
			if (!pTextualIdNode)
			{
				XBMC->Log(LOG_ERROR, "Service has no <TextualID> tag");
				continue;
			}
			xml_attribute<> *pTextualIdServiceNameAttribute = pTextualIdNode->first_attribute("ServiceName");
			if (pTextualIdServiceNameAttribute == NULL)
			{
				XBMC->Log(LOG_ERROR, "<TextualID> tag has no ServiceName attribute");
				continue;
			}
			serviceName = pTextualIdServiceNameAttribute->value();

			xml_node<> *pLogicalChannelNumberNode = pServiceNode->first_node("LogicalChannelNumber");
			if (!pLogicalChannelNumberNode)
			{
				XBMC->Log(LOG_ERROR, "Service has no <LogicalChannelNumber> tag");
				continue;
			}
			std::string strLogicalChannelNumber = pLogicalChannelNumberNode->value();

			std::istringstream ss(strLogicalChannelNumber);
			ss >> logicalChannelNumber;
			if (ss.fail())
			{
				XBMC->Log(LOG_ERROR, "<LogicalChannelNumber> is not an integer");
				continue;
			}

			/* Search in channels for this ServiceName */
			bool bChannelExists = false;
			for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
			{
				PVRMovistarTVChannel &thisChannel = m_channels.at(iChannelPtr);

				if (thisChannel.serviceName == serviceName)
				{
					bChannelExists = true;

					/* Update channel data */
					thisChannel.iChannelNumber = logicalChannelNumber;
				}
			}

			if (!bChannelExists)
			{
				/* Create a new channel */
				PVRMovistarTVChannel newChannel;

				newChannel.serviceName = serviceName;
				newChannel.bIsRadio = false;
				newChannel.iUniqueId = m_channels.size() + 1;
				newChannel.iEncryptionSystem = 0;
				newChannel.iSubChannelNumber = 0;
				newChannel.iChannelNumber = logicalChannelNumber;

				m_channels.push_back(newChannel);
			}
		}
	}
}

void MovistarTV::OnXmlReceived(const SDSMulticastDeliveryPacket& file)
{
	switch (file.header.payload_id)
	{
		case 1:	/* SD&S Service Provider Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S Service Provider Discovery Information");
			ParseServiceProviderDiscovery(file.payload);
			break;
		case 2: /* SD&S Broadcast Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S Broadcast Discovery Information");
			ParseBroadcastDiscoveryInformation(file.payload);
			break;
		case 3: /* SD&S COD Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S COD Discovery Information");
			break;
		case 4: /* SD&S Services from other SPs */
			XBMC->Log(LOG_NOTICE, "SD&S Services from other SPs");
			break;
		case 5: /* SD&S Package Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S Package Discovery Information");
			ParsePackageDiscoveryInformation(file.payload);
			break;
		case 6: /* SD&S BCG Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S BCG Discovery Information");
			break;
		case 7: /* SD&S Regionalisation Discovery Information */
			XBMC->Log(LOG_NOTICE, "SD&S Regionalisation Discovery Information");
			break;
		case 177: /* CDS XML download session description */
			XBMC->Log(LOG_NOTICE, "CDS XML download session description");
			break;
		case 178: /* RMS-FUS Firmware Update Announcements */
			XBMC->Log(LOG_NOTICE, "RMS-FUS Firmware Update Announcements");
			break;
		case 193: /* Application Discovery Information */
			XBMC->Log(LOG_NOTICE, "Application Discovery Information");
			break;

		default:
			if ((file.header.payload_id >= 165) && (file.header.payload_id <= 175))
			{
				XBMC->Log(LOG_NOTICE, "BCG Payload ID values");
			}
			else
			{
				XBMC->Log(LOG_NOTICE, "Received payload ID %d", file.header.payload_id);
			}
	}
}