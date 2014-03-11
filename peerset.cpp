// gnutella crawler
// Author: Zain Shamsi

#include "Peer.h"

void PeerSet::Insert(Peer x){
	all_peers.insert(x);
}

bool PeerSet::Contains(Peer x){
	return all_peers.find(x) != all_peers.end();
}

int PeerSet::Size(){
	return all_peers.size();
}

void PeerSet::OutputSetToFile(){
	FILE* file;
	set<Peer, Peer>::iterator current;
	struct in_addr IP_struct;

	printf("\nOpening file PeerSet.txt\n");
	file = fopen("PeerSet.txt", "w");
	for (current = all_peers.begin(); current != all_peers.end(); current++){		
		IP_struct.S_un.S_addr = current->IP;
		fprintf(file, "%s:%d\n", inet_ntoa(IP_struct), current->port);
	}

	fclose(file);
	printf("Done writing to file.\n");
	return;
}
