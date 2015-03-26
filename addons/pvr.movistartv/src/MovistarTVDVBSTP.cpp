#include "MovistarTVDVBSTP.h"
#include "rapidxml/rapidxml.hpp"
#include <sstream>

using namespace ADDON;
using namespace rapidxml;

MovistarTVDVBSTP::MovistarTVDVBSTP(void)
{
}

MovistarTVDVBSTP::~MovistarTVDVBSTP(void)
{
}

bool MovistarTVDVBSTP::GetXmlFile(const std::string& address, unsigned short port, unsigned short payload_id, std::string& return_value)
{
	SOCKET sd;
	struct ip_mreq mreq;
	bool retval = false;

	if ((sd = CreateSocket(port)) == INVALID_SOCKET)
	{
		// CreateSocket() already logs the error.
		return false;
	}

	/* join multicast group */
	XBMC->Log(LOG_DEBUG, "Joining multicast group %s on port %d", address.c_str(), port);
	mreq.imr_multiaddr.s_addr = inet_addr(address.c_str());
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

#if defined(TARGET_WINDOWS)
	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
#else
	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0)
#endif
	{
		XBMC->Log(LOG_ERROR, "Can not join the multicast group %s", address.c_str());
		closesocket(sd);
		return false;
	}

	char buffer[2048];
	SDSMulticastDeliveryPacket packet;
	time_t start_loop = time(NULL);
	int bad_count = 0;
	int next_section = 0;
	return_value.clear();

	/* Loop */
	while (1)
	{
		ssize_t nBytes = recv(sd, (char *)&buffer, 2048, 0);
		if (nBytes <= 0)
		{
			XBMC->Log(LOG_ERROR, "Connection timeout");
			bad_count++;
			if (bad_count == 4)
			{
				XBMC->Log(LOG_ERROR, "Too many connection timeouts.");
				break;
			}
			continue;
		}

		/* Reset counter */
		bad_count = 0;

		/* Parse the header */
		if (!ParsePacket((const char *)&buffer, nBytes, packet))
		{
			XBMC->Log(LOG_ERROR, "Can not parse header");
			continue;
		}

		/* Check the payload ID */
		if (packet.header.payload_id == payload_id)
		{
			/* Payload ID is present; reset timeout */
			start_loop = time(NULL);
			//XBMC->Log(LOG_NOTICE, "PayloadID: %d expected section %d (%d of %d)", packet.header.payload_id, next_section, packet.header.section_number, packet.header.last_section_number);

			if (packet.header.section_number == next_section)
			{
				/* Get the payload */
				return_value.append(packet.payload);

				//XBMC->Log(LOG_NOTICE, "PayloadID: %d (%d of %d)", packet.header.payload_id, packet.header.section_number, packet.header.last_section_number);
				/* Have we finished? */
				if (packet.header.section_number == packet.header.last_section_number)
				{
					retval = true;
					break;
				}
				else 
				{
					next_section++;
				}
			}
		}

		/* Timeout set to 30 seconds */
		if (difftime(time(NULL), start_loop) > 30)
		{
			XBMC->Log(LOG_ERROR, "Timeout");
			break;
		}
	}

	/* leave multicast group */
#if defined(TARGET_WINDOWS)
	if (setsockopt(sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
#else
	if (setsockopt(sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0)
#endif
	{
		XBMC->Log(LOG_ERROR, "Can not leave the multicast group %s", address.c_str());
	}

	if (closesocket(sd) != 0)
	{
		XBMC->Log(LOG_ERROR, "can not close the socket.");
	}

	return retval;
}

SOCKET MovistarTVDVBSTP::CreateSocket(unsigned short port)
{
	int reuse = 1;
	SOCKET sd = INVALID_SOCKET;
	struct sockaddr_in addr;
#if defined(TARGET_WINDOWS)
	int timeout = 3000;
#else
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
#endif

	/* Create the socket */
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		XBMC->Log(LOG_ERROR, "Can not create UDP socket.");
		return INVALID_SOCKET;
	}

	/* allow multiple sockets to use the same PORT number */
#if defined(TARGET_WINDOWS)
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) 
#else
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0)
#endif
	{
		XBMC->Log(LOG_ERROR, "Can not reuse socket address");
		closesocket(sd);
		return INVALID_SOCKET;
	}

	/* Set timeout */
#if defined(TARGET_WINDOWS)
	if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int)) < 0) {
#else
	if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(struct timeval)) < 0) {
#endif
		XBMC->Log(LOG_ERROR, "Can not set the socket's receive timeout.");
		closesocket(sd);
		return INVALID_SOCKET;
	}

	/* Bind to port */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		XBMC->Log(LOG_ERROR, "Can not bnid to port %d", port);
		closesocket(sd);
		return INVALID_SOCKET;
	}

	return sd;
}

bool MovistarTVDVBSTP::ParsePacket(const char *data, size_t datalen, struct SDSMulticastDeliveryPacket& packet)
{
	memset(&packet.header, 0, sizeof(SDSMulticastDeliveryHeader));

	/* header */
	packet.header.version = ((int)*data >> 6) & 0x03;
	packet.header.encryption = ((int)*data >> 1) & 0x03;
	packet.header.crc_flag = (int)*data & 0x03;
	packet.header.total_segment_size = (((unsigned int)(*(data + 1)) & 0xFF) << 16) + (((unsigned int)(*(data + 2)) &0xFF) << 8) + ((unsigned int)(*(data + 3) & 0xFF));
	packet.header.payload_id = (unsigned int)(*(data + 4) &0xFF);
	packet.header.segment_id = (((unsigned int)(*(data + 5) & 0xFF)) << 8) + ((unsigned int)(*(data + 6)) &0xFF);

	packet.header.section_number = (((unsigned int)(*(data + 8)) & 0xFF) << 4) | (((unsigned int)(*(data + 9)) >> 4) & 0x0F);
	// Data 10 returns 12 bytes not just 4!
	packet.header.last_section_number = (((unsigned int)(*(data + 9)) & 0x0f) << 8) + ((unsigned int)(*(data + 10) & 0xFF));

	packet.header.compression = ((unsigned int)(*(data + 11)) >> 6) & 0x03;
	packet.header.provider_id_flag = ((unsigned int)(*(data + 11)) >> 4) & 0x01;
	packet.header.hdr_len = (unsigned int)(*(data + 11)) & 0x0F;

	/* payload */
	size_t payload_offset = 12;
	size_t payload_length = datalen - (12 + packet.header.hdr_len);
	if (packet.header.crc_flag) {
		payload_length -= 4;
	}
	if (packet.header.provider_id_flag) {
		payload_length -= 4;
		payload_offset += 4;
	}
	payload_offset += packet.header.hdr_len;

	char * char_payload = (char *)malloc(payload_length + 1);
	memset(char_payload, 0, payload_length + 1);
	memcpy(char_payload, data + payload_offset, payload_length);
	//strncpy(char_payload, (const char *)*data + payload_offset, payload_length);
	packet.payload = std::string(char_payload);
	free(char_payload);

	return true;
}

bool MovistarTVDVBSTP::GetServiceProviderDiscovery(const std::string& address, unsigned short port, std::string& return_value)
{
	bool retval = GetXmlFile(address, port, 1, return_value);

	if (retval)
	{
		WriteCache("ServiceProviderDiscovery.xml", return_value);
	}

	return retval;
}

std::vector<DVBSTPSingleService> MovistarTVDVBSTP::GetBroadcastDiscovery(const std::string& address, unsigned short port)
{
	std::vector<DVBSTPSingleService> serviceList;
	std::string response;

	if (!GetXmlFile(address, port, 2, response))
	{ 
		XBMC->Log(LOG_ERROR, "Unable fetch BroadcastDiscovery XML");
		return serviceList;
	}

	WriteCache("BroadcastDiscovery.xml", response);

	/* Process the XML data */
	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)response.c_str());
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse BroadcastDiscovery XML: %s", p.what());
		return serviceList;
	}

	xml_node<> *pServiceDiscoveryRootElement = xmlDoc.first_node("ServiceDiscovery");
	if (!pServiceDiscoveryRootElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <ServiceDiscovery> tag");
		return serviceList;
	}

	xml_node<> *pBroadcastDiscoveryElement = pServiceDiscoveryRootElement->first_node("BroadcastDiscovery");
	if (!pBroadcastDiscoveryElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <BroadcastDiscovery> tag");
		return serviceList;
	}

	xml_node<> *pServiceListElement = pBroadcastDiscoveryElement->first_node("ServiceList");
	if (!pServiceListElement)
	{
		XBMC->Log(LOG_ERROR, "BroadcastDiscovery has no <ServiceList> tag");
		return serviceList;
	}

	xml_node<> *pSingleServiceNode = NULL;
	for (pSingleServiceNode = pServiceListElement->first_node("SingleService"); pSingleServiceNode; pSingleServiceNode = pSingleServiceNode->next_sibling("SingleService"))
	{
		DVBSTPSingleService singleService;

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
		singleService.serviceLocation.address = pIPMulticastAddressAddressAttribute->value();

		xml_attribute<> *pIPMulticastAddressPortAttribute = pIPMulticastAddressNode->first_attribute("Port");
		if (pIPMulticastAddressPortAttribute == NULL)
		{
			singleService.serviceLocation.port = 8208;
		}
		else 
		{
			std::string strIPMulticastPort = pIPMulticastAddressPortAttribute->value();
			std::istringstream ss(strIPMulticastPort);
			ss >> singleService.serviceLocation.port;
			if (ss.fail())
			{
				XBMC->Log(LOG_ERROR, "<IPMulticastAddress> port attribute is not an integer");
				continue;
			}
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
			singleService.textualIdentifier.logoURL = pLogoURIAttribute->value();
		}

		xml_attribute<> *pServiceNameAttribute = pTextualIdentifierElement->first_attribute("ServiceName");
		if (pServiceNameAttribute == NULL)
		{
			XBMC->Log(LOG_ERROR, "BroadcastDiscovery <TextualIdentifier> has no ServiceName attribute");
			continue;
		}
		singleService.textualIdentifier.serviceName = pServiceNameAttribute->value();

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
		singleService.serviceInfo.name = pSINameNode->value();

		xml_node<> *pSIDescriptionNode = pSIElement->first_node("Description");
		if (pSIDescriptionNode)
		{
			singleService.serviceInfo.description = pSIDescriptionNode->value();
		}
		
		/* Add the singleService*/
		serviceList.push_back(singleService);
	}

	XBMC->Log(LOG_DEBUG, "Got %d services from BroadcastDiscovery.xml", serviceList.size());
	return serviceList;
}

std::vector<DVBSTPPackage> MovistarTVDVBSTP::GetPackageDiscovery(const std::string& address, unsigned short port)
{
	std::vector<DVBSTPPackage> packages;
	std::string response;

	if (!GetXmlFile(address, port, 5, response))
	{
		XBMC->Log(LOG_ERROR, "Unable to fetch PackageDiscovery XML");
		return packages;
	}

	WriteCache("PackageDiscovery.xml", response);

	/* Process the XML data */
	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>((char *)response.c_str());
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse PackageDiscovery XML: %s", p.what());
		return packages;
	}

	xml_node<> *pServiceDiscoveryRootElement = xmlDoc.first_node("ServiceDiscovery");
	if (!pServiceDiscoveryRootElement)
	{
		XBMC->Log(LOG_ERROR, "PackageDiscovery has no <ServiceDiscovery> tag");
		return packages;
	}

	xml_node<> *pPackageDiscoveryElement = pServiceDiscoveryRootElement->first_node("PackageDiscovery");
	if (!pPackageDiscoveryElement)
	{
		XBMC->Log(LOG_ERROR, "PackageDiscovery has no <PackageDiscovery> tag");
		return packages;
	}

	xml_node<> *pPackageNode = NULL;
	for (pPackageNode = pPackageDiscoveryElement->first_node("Package"); pPackageNode; pPackageNode = pPackageNode->next_sibling("Package"))
	{
		DVBSTPPackage package;

		xml_attribute<> *pPackageIdAttribute = pPackageNode->first_attribute("Id");
		if (pPackageIdAttribute != NULL)
		{
			package.id = pPackageIdAttribute->value();
		}

		xml_node<> *pPackageNameNode = pPackageNode->first_node("PackageName");
		if (!pPackageNameNode)
		{
			XBMC->Log(LOG_ERROR, "Package has no <PackageName> tag");
			continue;
		}
		package.packageName = pPackageNameNode->value();

		xml_node<> *pServiceNode = NULL;
		for (pServiceNode = pPackageNode->first_node("Service"); pServiceNode; pServiceNode = pServiceNode->next_sibling("Service"))
		{
			DVBSTPPackageService service;

			xml_node<> *pTextualIdNode = pServiceNode->first_node("TextualID");
			if (!pPackageNameNode)
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
			service.serviceName = pTextualIdServiceNameAttribute->value();

			xml_node<> *pLogicalChannelNumberNode = pServiceNode->first_node("LogicalChannelNumber");
			if (!pLogicalChannelNumberNode)
			{
				XBMC->Log(LOG_ERROR, "Service has no <LogicalChannelNumber> tag");
				continue;
			}
			std::string strLogicalChannelNumber = pLogicalChannelNumberNode->value();

			std::istringstream ss(strLogicalChannelNumber);
			ss >> service.logicalChannelNumber;
			if (ss.fail())
			{
				XBMC->Log(LOG_ERROR, "<LogicalChannelNumber> is not an integer");
				continue;
			}

			/* Finally add it */
			package.services.push_back(service);
		}


		/* Finally add it */
		if (package.services.size() > 0)
		{
			packages.push_back(package);
		}
	}

	XBMC->Log(LOG_DEBUG, "Got %d packages from PackageDiscovery.xml", packages.size());
	return packages;
}
void MovistarTVDVBSTP::GetBCGDiscovery(const std::string& address, unsigned short port)
{
	std::string response;

	if (!GetXmlFile(address, port, 6, response))
	{
		XBMC->Log(LOG_ERROR, "Unable to fetch BCGDiscovery XML");
		return;
	}

	WriteCache("BCGDiscovery.xml", response);

}

void MovistarTVDVBSTP::WriteCache(const std::string& filename, const std::string& content)
{
	std::string cache_directory = g_strClientPath;
	if (cache_directory.at(cache_directory.size() - 1) != PATH_SEPARATOR_CHAR)
		cache_directory.append(PATH_SEPARATOR_STRING);
	cache_directory.append("cache");


	if (XBMC->DirectoryExists(cache_directory.c_str()) || XBMC->CreateDirectory(cache_directory.c_str()))
	{
		void *file;
		std::string cache_file = cache_directory;
		cache_file.append(PATH_SEPARATOR_STRING);
		cache_file.append(filename.c_str());

		if (!(file = XBMC->OpenFileForWrite(cache_file.c_str(), true)))
		{
			XBMC->Log(LOG_ERROR, "%s: Failed to create cache file: %s", __FUNCTION__, cache_file.c_str());
			return;
		}

		XBMC->WriteFile(file, (void *)content.c_str(), content.length());
		XBMC->CloseFile(file);
	}
	else
	{
		XBMC->Log(LOG_ERROR, "%s: Failed to create cache directory: %s", __FUNCTION__, cache_directory.c_str());
		return;
	}

}