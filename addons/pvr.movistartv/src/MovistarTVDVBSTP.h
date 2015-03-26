#pragma once

#include "platform/util/StdString.h"
#include "client.h"
#include "generic_socket.h"

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Ver|Resrv|Enc|C|                Total_Segment_Size             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Payload ID    | Segment ID                    |Segment_Version|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Section_Number        | Last Section Number   |Compr|P|HDR_LEN|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                (Conditional) ServiceProviderID                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
:                (Optional) Private Header Data                 :
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
:                           payload                             :
|                                               +-+-+-+-+-+-+-+-+
|                                               |(Optional) CRC |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             (Optional) CRC (Cont)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct SDSMulticastDeliveryHeader
{
	unsigned int version : 2;
	unsigned int resrv : 3;
	unsigned int encryption : 2;
	unsigned int crc_flag : 1;
	unsigned int total_segment_size : 24;
	unsigned int payload_id : 8;
	unsigned int segment_id : 16;
	unsigned int segment_version : 8;
	unsigned int section_number : 12;
	unsigned int last_section_number : 12;
	unsigned int compression : 3;
	unsigned int provider_id_flag : 1;
	unsigned int hdr_len : 4;
};

struct DVBSTPTextualIdentifier
{
	std::string logoURL;
	std::string serviceName;
};

struct SDSMulticastDeliveryPacket
{
	SDSMulticastDeliveryHeader header;
	std::string                payload;
};

struct DVBSTPPackageService
{
	std::string serviceName;
	int         logicalChannelNumber;
};

struct DVBSTPPackage
{
	std::string id;
	std::string packageName;
	std::vector<DVBSTPPackageService> services;
};

struct DVBSTPIPMulticastAddress
{
	std::string    address;
	unsigned short port;
};

struct DVBSTPSingeServiceSIGenre
{
	//std::string href;
	std::string urn_name;
};

struct DVBSTPSingeServiceSI
{
	std::string name;
	std::string description;
	DVBSTPSingeServiceSIGenre genre;
};

struct DVBSTPSingleService
{
	DVBSTPIPMulticastAddress serviceLocation;
	DVBSTPTextualIdentifier  textualIdentifier;
	DVBSTPSingeServiceSI     serviceInfo;
};

class MovistarTVDVBSTP
{
	public:
		MovistarTVDVBSTP(void);
		virtual ~MovistarTVDVBSTP(void);

		virtual bool GetServiceProviderDiscovery(const std::string& address, unsigned short port, std::string& return_value);
		std::vector<DVBSTPSingleService> GetBroadcastDiscovery(const std::string& address, unsigned short port);
		std::vector<DVBSTPPackage> GetPackageDiscovery(const std::string& address, unsigned short port);
		void GetBCGDiscovery(const std::string& address, unsigned short port);
	protected:
		virtual bool GetXmlFile(const std::string& address, unsigned short port, unsigned short payload_id, std::string& return_value);
		SOCKET CreateSocket(unsigned short port);
		virtual bool ParsePacket(const char *data, size_t datalen, struct SDSMulticastDeliveryPacket& packet);

		/* Cache methods */
		void WriteCache(const std::string& filename, const std::string& content);

};