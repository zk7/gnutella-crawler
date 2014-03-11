// gnutella crawler
// Author: Zain Shamsi
#pragma once
#include "common.h"

#pragma pack(push)
#pragma pack(1)
class Peer{
public:
	DWORD IP;
	unsigned short port;	
	
	Peer(){}
	int ParseString(char* address_string){
		char new_string[25];
		int ip_length = 0;	
		int return_code;

		ip_length = strcspn(address_string, ":");	

		//Set IP
		return_code = strncpy_s(new_string,sizeof(new_string),address_string,ip_length);
		if (return_code != 0) return 1;
		IP = inet_addr(new_string);
		if (IP == INADDR_NONE) return 1;

		//Set Port
		return_code = strncpy_s(new_string,sizeof(new_string),address_string+ip_length+1,strlen(address_string));
		if (return_code != 0) return 1;
		port = atoi(new_string);

		return 0;
	}

	bool operator ()(Peer a, Peer b) {
		return memcmp(&a, &b, sizeof(Peer)) < 0;
	}
};
#pragma pack(pop)

class PeerSet{
public:
	void Insert(Peer x);
	bool Contains(Peer x);
	void OutputSetToFile();
	void ReverseLookup();
	int Size();
private:
	std::set<Peer, Peer> all_peers;
};
