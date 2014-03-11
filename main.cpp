// gnutella crawler
// Author: Zain Shamsi

#include <WinSock2.h>
#include "common.h"
#include <time.h>

string BuildHttpRequest(char* address, int port, char* path);
string BuildGnutellaRequest(DWORD IP, int port);
vector<string> ParseCommandLine(string original_string);
void ParseResponse(string orig_str, vector<Peer> *ultra_peers, vector<Peer> *leaf_peers, map<char*, int> *user_agent_map);
string ConnectToHost(SOCKET sock, sockaddr_in server, string request);
int ThreadRun(LPVOID thread_variables);

class Shared_Vars{
public:	
	HANDLE mutex;
	HANDLE semaphore;
	HANDLE eventQuit;	
	queue<Peer> bfs_queue;
	PeerSet ultrapeer_set, totalpeer_set;
	map<char*, int> user_agent_map;
	volatile int peer_limit;
	volatile int active_threads;
	volatile int extracted_count, fail_count, success_count;
};



int main(int argc, char* argv[]){
	WSADATA wsaData;
	vector<string> connection_info;
	vector<Peer> ultrapeer_vector;
	Shared_Vars vars;
	string response = "";
	char* address;
	char* path;
	int port;
	int num_threads;

	srand( (unsigned)time( NULL ) );

	if (argc < 2){
		printf("Usage is %s host[:port][/path] [# of threads] [ultrapeer limit] ", argv[0]);
		return 0;
	}
		
	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2,2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError ());
		WSACleanup();	
		return 0;
	}

	//PARSE COMMAND LINE
	if (argc < 3){
		printf("Running with default thread and ultrapeer values:\n1 thread\n1500 ultrapeers\n\n");	
		num_threads = 1;
		vars.peer_limit = 1500;
	}
	else {
		num_threads = atoi(argv[2]);
		vars.peer_limit = atoi(argv[3]);
		if (num_threads < 1 || vars.peer_limit < 1){
			printf("At least one thread and ultrapeer required!\n\n");
			return 0;
		}
	}
	connection_info = ParseCommandLine(argv[1]);
	address = (char*)connection_info[0].c_str();
	
	//set default or given port
	if (connection_info[1].empty()) port = 80;
	else port = atoi(connection_info[1].c_str());
	
	//set default or given path
	if (connection_info[2].empty()) path = "/";
	else path = (char*)(connection_info[2].c_str());

	if (DEBUG_ON){ cout << "Parsing results: " << endl << "host: " << address << endl
		<< "port: " << port << endl
		<< "path: " << path << endl
		<< endl;
	}

	//Connect and send request
	response = BuildHttpRequest(address, port, path);
	if (response.empty()){
		printf("Could not get response from webcache.\n");
		return 0;
	}
			
	ParseResponse(response, &ultrapeer_vector, &ultrapeer_vector, &vars.user_agent_map); //just need one vector, no leaves

	//THREAD SETUP
	// thread handles are stored here; they can be used to check status of threads, or kill them
	HANDLE *handles = new HANDLE [num_threads];

	//set up counters
	vars.active_threads = 0;
	vars.extracted_count = 0;
	vars.success_count = 0;
	vars.fail_count = 0;	
	// create a mutex for accessing critical sections; initial state = not locked
	vars.mutex = CreateMutex (NULL, 0, NULL);	
	// create a semaphore that counts the number of active threads; initial value = 0, max = 5
	vars.semaphore = CreateSemaphore (NULL, 0, 5, NULL);
	// create a quit event; manual reset, initial state = not signaled
	vars.eventQuit = CreateEvent (NULL, true, false, NULL);


	//Initial Insert into the BFSQueue and Peersets.
	vector<Peer>::iterator current;
	for (current=ultrapeer_vector.begin(); current < ultrapeer_vector.end(); current++){
		//Always add to total peers
		vars.totalpeer_set.Insert(*current);

		//and if ultrapeer already in set
		if (!vars.ultrapeer_set.Contains(*current)){ 
			//does not exist in set, add to queue and set
			vars.bfs_queue.push(*current);
			vars.ultrapeer_set.Insert(*current);
			ReleaseSemaphore(vars.semaphore, 1, NULL);
			if (DEBUG_ON) printf("Added Peer with port %d to BFSQueue and Set of UltraPeers\n", current->port);
		}
		else { if (DEBUG_ON) printf("Duplicate Peer found! %d:%d\n", current->IP, current->port); }		
	}
	printf("\nReceived initial list of %d ultrapeers from webcache.\n", vars.bfs_queue.size());

	//Split Threads
	for (int i = 0; i < num_threads; i++){
		handles[i] = CreateThread (NULL, 4096, (LPTHREAD_START_ROUTINE)ThreadRun, &vars, 0, NULL);	
		SetThreadPriority(handles[i], THREAD_PRIORITY_LOWEST);
	}

	//Print info every 5 secs until no more active threads
	int second_counter = 0;
	int total_extract = 0, last_extract = 0, peers_extracted;
	double total_extract_rate, current_extract_rate;
	int total_success =0, last_success = 0;
	double total_success_rate = 0, current_success_rate;
	printf("Started %d threads...\n\n", num_threads);
	while (WaitForSingleObject(vars.eventQuit, 5000) != WAIT_OBJECT_0){		
		second_counter += 5;
		total_extract = vars.extracted_count;
		total_success = vars.success_count;
		peers_extracted = total_extract - last_extract;

		//compute crawling speed metrics
		current_extract_rate = peers_extracted / 5.0;  //peers extracted in last 5 seconds / 5
		total_extract_rate = total_extract / (double)second_counter; //total peers extracted / total time

		//compute success rate metrics
		if (peers_extracted == 0) current_success_rate = 0;
		else current_success_rate = ((total_success - last_success) / (double)peers_extracted) * 100.0; //peers succeeeded in last 5 seconds / total peers extracted in last 5 seconds
		total_success_rate = (total_success / (double)total_extract) * 100.0; //peers succeeded / total peers extracted
		printf("[%ds] extracted %d, left %d, ", second_counter, total_extract, vars.bfs_queue.size());
		//printf("last extract %d, difference/5 %f\n", last_extract, (peers_extracted / 5.0));
		printf("rate %.0fpps(av %.0fpps), ", current_extract_rate, total_extract_rate); 
		printf("success %.0f%%(av %.0f%%)\n", current_success_rate, total_success_rate);
		last_extract = total_extract;
		last_success = total_success;	
	}		
	
	//Wait for threads to return
	printf("\n--->Quit Condition Reached!<---\nWaiting for all threads to end..\n");
	WaitForMultipleObjects(num_threads, handles, TRUE, INFINITE);

    // Close handles
    for(int i = 0; i < num_threads; i++) CloseHandle(handles[i]);
    CloseHandle(vars.mutex);
	CloseHandle(vars.semaphore);
	CloseHandle(vars.eventQuit);

	if (DEBUG_ON) printf("\n\nTotal Extracted: %d\nTotal Success: %d (%d%%)\nTotal Fail: %d\n\n", vars.extracted_count, vars.success_count, total_success_rate, vars.fail_count);

	printf("\nWriting the total peer set of size %d to file...\n\n", vars.totalpeer_set.Size());
	//write total_peerset to file
	vars.totalpeer_set.OutputSetToFile();

	//write user_agent statistics to file
	FILE* file;
	file = fopen("UserAgent.txt", "w");
	for (map<char*, int>::iterator i = vars.user_agent_map.begin(); i != vars.user_agent_map.end(); i++){	
		fprintf(file, "%s: %d\n", i->first, i->second);
	}
	fclose(file);

	// call cleanup for Winsock when done with everything and ready to exit program
	WSACleanup ();

	system("pause");
	return 0;
}

int ThreadRun(LPVOID thread_variables){
	Shared_Vars *vars = ((Shared_Vars*)thread_variables);
	Peer p;
	string response;
	vector<Peer> ultrapeer_vector;
	vector<Peer> leafpeer_vector;

	HANDLE arr[] = {vars->eventQuit, vars->semaphore};
	while (true){
		if (WaitForMultipleObjects(2, arr, false, INFINITE) == WAIT_OBJECT_0+1){
			//semaphore available
			
			//---LOCK-- Dequeue from BFS
			WaitForSingleObject(vars->mutex, INFINITE);
			vars->active_threads++;
			p = vars->bfs_queue.front();
			vars->bfs_queue.pop();
			vars->extracted_count++;
			ReleaseMutex(vars->mutex); //---RELEASE mutex

			//Get response string
			response = BuildGnutellaRequest(p.IP, p.port);
			
			if (!response.empty()){
				//Parse response and get vector of peers
				ParseResponse(response, &ultrapeer_vector, &leafpeer_vector, &vars->user_agent_map);

				//---LOCK---Add to Set and BFS	
				WaitForSingleObject(vars->mutex, INFINITE);
				vector<Peer>::iterator current;
				for (current=ultrapeer_vector.begin(); current < ultrapeer_vector.end(); current++){
					//Always add ultrapeers to total peers
					vars->totalpeer_set.Insert(*current);

					//check if ultrapeer already in set
					if (!vars->ultrapeer_set.Contains(*current)){ 
						//does not exist in set, add to queue and set
						vars->bfs_queue.push(*current);
						vars->ultrapeer_set.Insert(*current);	
						ReleaseSemaphore(vars->semaphore, 1, NULL);
						if (DEBUG_ON) printf("Added Peer with port %d to BFSQueue and Set of UltraPeers\n", current->port);
					}
					else { if (DEBUG_ON) printf("Duplicate Peer found! %d:%d\n", current->IP, current->port); }					
				}

				//Add all leaves to total set
				for (current = leafpeer_vector.begin(); current < leafpeer_vector.end(); current++){
					if (DEBUG_ON) printf("Added Peer with port %d to Set of Total Peers\n", current->port);
					vars->totalpeer_set.Insert(*current);
				}
				vars->success_count++;
				vars->active_threads--;
				//Check if		queue empty and no active threads			OR		reached peer limit
				if (vars->bfs_queue.size() == 0 && vars->active_threads ==0 || vars->extracted_count >= vars->peer_limit){
					SetEvent(vars->eventQuit);
				}
				ReleaseMutex(vars->mutex);		//----RELEASE Mutex
			}
			else{
				//Increment fail counter
				//----LOCK Mutex
				WaitForSingleObject(vars->mutex, INFINITE);
				vars->fail_count++;
				vars->active_threads--;
				//Check if		queue empty and no active threads			OR		reached peer limit
				if (vars->bfs_queue.size() == 0 && vars->active_threads ==0 || vars->extracted_count >= vars->peer_limit){
					SetEvent(vars->eventQuit);
				}
				ReleaseMutex(vars->mutex); //---RELEASE Mutex
			}
		}
		else{
			//eventQuit Signaled
			return 0;
		}
	}
}

string BuildHttpRequest(char* address, int port, char* path){	
	string request = "";

	//build our request
	request.append("GET ");
	request.append(path);
	request.append("?client=gnuc0.1&hostfile=1 HTTP/1.0\r\nHost: ");
	request.append(address);
	request.append("\r\nUser-agent: ZGNU_CRAWLER/1.0\r\nConnection: close\r\n\r\n");

	if (DEBUG_ON) cout << "The formed request is: " << endl << request << endl << endl;

	// open a TCP socket
	SOCKET sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		if (DEBUG_ON) printf ("socket() generated error %d\n", WSAGetLastError ());
		WSACleanup ();	
		return "";
	}

	// structure used in DNS lookups
	struct hostent *remote; 

	// structure for connecting to server
	struct sockaddr_in server;

	// first assume that the string is an IP address
	DWORD IP = inet_addr (address);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname (address)) == NULL)
		{
			if (DEBUG_ON) printf ("Invalid string: neither FQDN, nor IP address\n");
			return "";
		}
		else // take the first IP address and copy into sin_addr
			memcpy ((char *)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}

	// setup the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons (port);		// host-to-network flips the byte order	

	return ConnectToHost(sock, server, request);
}

string BuildGnutellaRequest(DWORD IP, int port){
	string request = "";

	//build our request
	request.append("GNUTELLA CONNECT/0.6\r\n");
	request.append("User-agent: ZGNU_CRAWLER/1.0\r\n");
	request.append("X-Ultrapeer: False\r\n");
	request.append("Crawler: 0.1\r\n\r\n");

	//if (DEBUG_ON) cout << endl << "The formed request is: " << endl << request << endl;

	// open a TCP socket
	SOCKET sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		if (DEBUG_ON) printf ("socket() generated error %d\n", WSAGetLastError ());
		WSACleanup ();	
		return "";
	}

	// structure for connecting to server
	struct sockaddr_in server;

	if (IP == INADDR_NONE)
	{
		if (DEBUG_ON) printf ("Invalid IP received\n\n");
		return "";
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}

	// setup the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons (port);		// host-to-network flips the byte order	

	return ConnectToHost(sock, server, request);
}

string ConnectToHost(SOCKET sock, sockaddr_in server, string request){	
	int result;
	const int buffer_size = 512;
	char rec_buffer[buffer_size];
	string response = "";
	fd_set fd;
	struct timeval tv;
	int timeout = 30, timeout_counter = 0;		

	if (DEBUG_ON) printf ("\nTrying to connect to %s...", inet_ntoa (server.sin_addr));

	// connect to the server on port 80
	if (connect (sock, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		if (DEBUG_ON) printf ("\nConnection error: %d\n", WSAGetLastError ());
		return response;
	}

	if (DEBUG_ON) printf ("\nSuccessfully connected to %s on port %d\n\n", inet_ntoa (server.sin_addr), htons(server.sin_port));

	// send request
	result = send(sock, request.c_str(), request.length(), 0);
	if (result == SOCKET_ERROR)
	{
		if (DEBUG_ON) printf ("\nAn error occured while sending request: %d\n", WSAGetLastError ());		
		closesocket (sock);
		return response;
	}

	if (DEBUG_ON) cout << "Socket send() returned: " << result << endl;

	//set up the select function args
	FD_ZERO(&fd);
	FD_SET(sock, &fd);
	tv.tv_sec = timeout;
	tv.tv_usec = timeout * 1000;

	// Receive until the peer closes the connection or error
    do {
		//Use select to make sure we dont block in case connection didnt close and no more data
		if (select(0, &fd, NULL, NULL, &tv) > 0){ 
			result = recv(sock, rec_buffer, buffer_size-1, 0);
			if ( result > 0 ){
				if (DEBUG_ON) printf("Bytes received: %d\n", result);
				rec_buffer[result] = '\0';
				response.append(rec_buffer, result);
				memset(rec_buffer, '\0', buffer_size);
			}
			else if ( result == 0 ){
				if (DEBUG_ON) printf("Connection closed\n\n");
				break;
			}
			else{
				if (DEBUG_ON) printf("recv failed with error: %d\n\n", WSAGetLastError());
				break;
			}
		}
		else{
			timeout_counter++;
			if (timeout_counter > 1) break;
		}
    } while(true);

	if (DEBUG_ON) cout << "Response: " << endl << response << endl;

	// close the socket to this server; open again for the next one
	closesocket (sock);

	return response;
}
