// gnutella crawler
// Author: Zain Shamsi

#include "common.h"

void ParseResponse(string orig_str, vector<Peer> *ultra_peers, vector<Peer> *leaf_peers, map<char*, int> *user_agent_map){
	char* token;
	char* str = (char*)orig_str.c_str();
	char* delimiters = " \r\n"; //delimiters are space, newline
	char* context_main;

	//clear vectors
	ultra_peers->clear();
	leaf_peers->clear();

	token = strtok_s(str, delimiters, &context_main);	

	if (strstr(token, "HTTP") != NULL){
		//Parse HTTP response		
		token = strtok_s(NULL, delimiters, &context_main);
		if (strcmp(token,"200") != 0){ //Response not 200 OK
			if (DEBUG_ON) printf("Response returned error code: %s\n\n", token);
			return;
		}
		delimiters = "\r\n"; //run through output line by line
		token = strtok_s(NULL, delimiters, &context_main);
		while (token != NULL){
			if (strspn(token, "1234567890.:") == strlen(token)){ //if contains only numbers, then ip
				//printf("%s\n", token); //Print to console

				//Create Peer
				Peer *p = new Peer();
				if (p->ParseString(token) == 0) //If parser fails, we throw away this peer				 
					ultra_peers->push_back(*p);
				delete p;
			}
			token = strtok_s(NULL, delimiters, &context_main);
		}		
	}
	else{
		//Parse Gnutella response
		//Ignoring check for 200 OK! As said in class, hosts might return a number of reponses.
		delimiters = "\r\n"; //run through output line by line
		token = strtok_s(NULL, delimiters, &context_main);
		while (token != NULL){
			if (strstr(token, "Peers") != NULL || strstr(token,"X-Try-Ultrapeers") != NULL){ //parse "Peers:" line
				char* context_sub;
				char* sub_token;
				
				delimiters = " :";
				strtok_s(token, delimiters, &context_sub); //run parse on token substring
				delimiters = " ,\r\n";				
				
				//Parse the IP List
				sub_token = strtok_s(NULL, delimiters, &context_sub);
				while (sub_token != NULL){					
					Peer *p = new Peer();
					if (p->ParseString(sub_token) == 0) //If parser fails, we throw away this peer
						ultra_peers->push_back(*p);
					delete p;
					sub_token = strtok_s(NULL, delimiters, &context_sub);
				}
				delimiters = "\r\n";
			}
			if (strstr(token, "Leaves") != NULL){ //parse "Leaves:" line
				char* context_sub;
				char* sub_token;

				delimiters = " :";
				strtok_s(token, delimiters, &context_sub); //run parse on token substring
				delimiters = " ,\r\n";				
				
				//Parse the IP List
				sub_token = strtok_s(NULL, delimiters, &context_sub);
				while (sub_token != NULL){					
					Peer *p = new Peer();
					if (p->ParseString(sub_token) == 0) //If parser fails, we throw away this peer
						leaf_peers->push_back(*p);
					delete p;
					sub_token = strtok_s(NULL, delimiters, &context_sub);
				}
				delimiters = "\r\n";
			}
			if (strstr(token, "User-Agent") != NULL){ //parse User-Agent line
				char* context_sub;
				char* sub_token;
				
				delimiters = " :";
				strtok_s(token, delimiters, &context_sub); //run parse on token substring
				delimiters = " /1234567890.\r\n";				
				
				//Parse the next token for user-agent name
				sub_token = strtok_s(NULL, delimiters, &context_sub);
				if (strstr(sub_token, "LimeWire") != NULL) (*user_agent_map)["LimeWire"] += 1;
				else if (strstr(sub_token, "FrostWire") != NULL) (*user_agent_map)["FrostWire"] += 1;
				else if (strstr(sub_token, "Frosty") != NULL) (*user_agent_map)["Frosty"] += 1;
				else if (strstr(sub_token, "morph") != NULL) (*user_agent_map)["Morpheus"] += 1;
				else if (strstr(sub_token, "BearShare") != NULL) (*user_agent_map)["BearShare"] += 1;
				else (*user_agent_map)["Other"] += 1;
				delimiters = "\r\n";
			}
			token = strtok_s(NULL, delimiters, &context_main);
		}
	}

	return;
}

vector<string> ParseCommandLine(string original_string){
	vector<string> return_values;
	int col_position, slash_position;
	int end = original_string.length();

	if (DEBUG_ON) cout << "Now parsing string " << original_string << endl;

	if (original_string.empty()){
		printf("String to be parsed is empty\n");
		return return_values;
	}

	col_position = original_string.find(":");
	slash_position = original_string.find("/");

	if (slash_position < col_position){
		printf("Invalid string!\n");
		return return_values;
	}

	//If string looks like host:8080/path
	if (col_position > 0){ 
		return_values.push_back(original_string.substr(0, col_position)); //store host before ':'
		if (slash_position > 0){
			return_values.push_back(original_string.substr(col_position+1, slash_position-col_position-1)); //store port between ':' and '/'
			return_values.push_back(original_string.substr(slash_position, end));
		}
		else{
			return_values.push_back(original_string.substr(col_position+1, end)); //else store port between ':' and '\0'
			return_values.push_back(""); //insert empty string for path
		}
	}
	else if (slash_position > 0){ //If string looks like host/path
			return_values.push_back(original_string.substr(0, slash_position)); //store host before '/'
			return_values.push_back(""); //empty string for port
			return_values.push_back(original_string.substr(slash_position, end)); //store path after '/'
		}
	else{ //If string is just host
		return_values.push_back(original_string);
		return_values.push_back("");
		return_values.push_back("");
	}

	return return_values;
}
