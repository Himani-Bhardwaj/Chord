#include "nodeserver.h"
#include "util.h"

pthread_mutex_t lock0;

void *event(void *fd){
	struct threaddetails *nn = (struct threaddetails *)fd;

	int clientsockfd = nn->socketfd;
	Node *args = nn->currentnode;
	
	char buffer[1024];
	memset(buffer,'\0',1024);
	recv(clientsockfd,buffer,sizeof(buffer), 0);
	string input;
	int i=0;
	while(buffer[i] != '\0'){
		input = input + buffer[i];
		i++;
	}
	// cout << "command recv " << input << endl;
	vector<string> command = splitcommand(input);
	// recv command is "findsuccessor nodeid"
	
	if(command[0] == "findsuccessor"){
		long long int rid = atoi(command[1].c_str());
		long long int result = args->findsuccessor(rid);
		if(result == -1){

			pair<string,long long int> ipport = args->successordetail();
			int newsockfd = newconnection(ipport.first,to_string(ipport.second));
			string commandtosend = "findsuccessor " + to_string(rid); // command to be send to listner "findsuccessor nodeid"
			char buffer[255];
			memset(buffer,'\0',sizeof(buffer));
			send(newsockfd,commandtosend.c_str(),commandtosend.size(),0);
			recv(newsockfd,buffer,sizeof(buffer), 0);
			string msg = "";
			int j=0;
			while(buffer[j] != '\0'){
				msg = msg + buffer[j];
				j++;
			}
			// cout << "msg from neighbour " << msg << endl;
			send(clientsockfd,msg.c_str(),msg.size(),0);
		}
		else{
					
			string r = to_string(result);
			pair<string,long long int> otherdetails = args->successordetail();
			r = r + " " + otherdetails.first + " " + to_string(otherdetails.second);
			// send is "nodeid ip port"
			send(clientsockfd,r.c_str(),r.size(),0);	
		}
				
	}

	else if(command[0] == "givepredecessor"){
		string msg="";

		pair<string,long long int> predetails = args->predecessordetail();
		long long int predid = args->predecessorid();

		msg = msg + to_string(predid) + " " + predetails.first + " " + to_string(predetails.second);
		send(clientsockfd,msg.c_str(),msg.size(),0);
	}

	else if(command[0] == "notify"){

		long long int n = args->getid();
		long long int n1 = atoi(command[3].c_str());

		long long int p = args->predecessorid(); 

		bool condition = false;
		// cout<<"In nofitfy values "<<n <<" "<<n1<<" "<<p<<endl;
		if(p == -1){
			// cout<<"predecessor is -1"<<endl;
			condition = true;
		}
		else{
			// n' belongsto (p,n)

			if(p == n && n1==p){
				// cout<<"p1"<<endl;
				condition = false;
			}
			else if(p < n){
				if(p < n1 && n1 < n){
					// cout<<"p2"<<endl;
					condition = true;
				}
				else{
					// cout<<"p3"<<endl;
					condition = false;
				}
			}
			else if(p > n){
				if(!(n <= n1 && n1<=p)){
					// cout<<"p4"<<endl;
					condition = true;
				}
				else{
					// cout<<"p5"<<endl;
					condition = false;
				}
			}
			else{
				 condition=true;
			}
		}

		if(condition){
			// cout<<"predecessor Updated"<<endl;

			long long int prev = p;
			pthread_mutex_lock(&lock0); 
			args->predecessor(command[1],atoi(command[2].c_str()),n1);
			pthread_mutex_unlock(&lock0); 

			long long int newpred = args->predecessorid();
			pair<string,long long int> predipport = args->predecessordetail();

			if(p != newpred && p !=-1 && newpred!=-1){

				vector<pair<string,long long int>> result = args->getdata();
				vector<pair<string,long long int>> newresult;

				for(unsigned int i=0;i<result.size();i++)
				{
					long long int requestid = result[i].second;
					string data = result[i].first;
					bool condition = false;

					// cout<<"Updating "<<p<<" "<<newpred<<" "<<requestid<<endl;

					if(prev < newpred){
						if(prev < requestid && requestid <= newpred){
							condition = true;
						}
						else{
							condition = false;
						}
					}
					else{
						if(!(newpred < requestid && requestid <= prev)){
							condition = true;
						}
						else{
							condition = false;
						}
					}
					if(condition){
						// cout<<"deletedata calling "<<endl;
						args->deletedata(data);
						newresult.push_back(make_pair(data,requestid));
					}
				}

				int newsockfd = newconnection(predipport.first,to_string(predipport.second));
				string startmsg = "startdownload";
				// cout << "Start download msg sent " << endl;
				send(newsockfd,startmsg.c_str(),startmsg.size(),0); // start msg sended to newpredecessor
				string ack="done";

				char buffer[100];
				memset(buffer,'\0',sizeof(buffer));
				recv(newsockfd,buffer,sizeof(buffer),0);

				// cout <<"ok msg recv " <<  buffer << endl;
				for(unsigned int i=0;i<newresult.size();i++)
				{
					string msg = newresult[i].first + " " + to_string(newresult[i].second); // msg = "string id"
					// cout << "data transfer of " << msg << endl;
					char buffer[100];
					memset(buffer,'\0',sizeof(buffer));
					send(newsockfd,msg.c_str(),msg.size(),0);
					recv(newsockfd,buffer,sizeof(buffer),0);
				}
				send(newsockfd,ack.c_str(),ack.size(),0);
				// cout << "done msg sent " << endl;
			} 
		}
	}
	else if(command[0] == "upload"){
		string ack = "File Uploaded to Server";
		send(clientsockfd,ack.c_str(),ack.size(),0);

		long long int requestid = gethash(command[1]);
		long long int p = args->predecessorid();
		long long int n = args->getid();
		// id (- (n,s]
		// id (p,n]
		bool condition = false;
		if(n == p){
			condition = true;
		}
		else if(p < n){
			if(p < requestid && requestid <= n){
				condition = true;
			}
			else{
				condition = false;
			}
		}
		else{
			if(!(n < requestid && requestid <= p)){
				condition = true;
			}
			else{
				condition = false;
			}
		}

		if(condition){
			args->storedata(requestid,command[1]);
		}
		else{

			//send request to successor
			pair<string,long long int> ipport = args->successordetail();
			int newsockfd = newconnection(ipport.first,to_string(ipport.second));
			string msg = "upload " + command[1];
			send(newsockfd,msg.c_str(),msg.size(),0);

		}	
	}
	else if(command[0] == "search"){
		bool found = args->search(command[1]); // search string in database 

		if(found){
			string msg = args->getip() + " " + to_string(args->getnodeportno());
			send(clientsockfd,msg.c_str(),msg.size(),0);
		}
		else{
			// same request to successor
			pair<string,long long int> ipport = args->successordetail();
			int newsockfd = newconnection(ipport.first,to_string(ipport.second));
			string msg = "search " + command[1];
			send(newsockfd,msg.c_str(),msg.size(),0);

			char buffer[200];
			memset(buffer,'\0',sizeof(buffer));
			recv(newsockfd,buffer,sizeof(buffer), 0);
			close(newsockfd);

			send(clientsockfd,buffer,sizeof(buffer),0);// send loction to client;
		}
	}

	else if(command[0] == "startdownload"){

		string ok = "ok";
		send(clientsockfd,ok.c_str(),ok.size(),0);
		// cout << "starting download" << endl;
		string ack = "done";
		while(true){

			char buffer[1024];
			memset(buffer,'\0',sizeof(buffer));

			recv(clientsockfd,buffer,sizeof(buffer),0);
			// cout << "msg recv " << buffer << endl;
			int i=0;
			string msg="";
			while(buffer[i] != '\0'){
				msg = msg + buffer[i];
				i++;
			}
			if(msg == "done") break;
			vector<string> c = splitcommand(msg); // msg will "string id"
			args->storedata(atoi(c[1].c_str()),c[0]); 
			send(clientsockfd,ack.c_str(),ack.size(),0);
		}
	}

	return NULL;

}

void* NodeServer(void* pointer){
	Node *args = (Node *)pointer;
	// cout<<"server"<<endl;
	// args->nodedetails();

	string ip = args->getip();
	long long int portno = args->getnodeportno();

	cout << "Listner is on " << ip << " " << portno << endl;
	int sockfd;
	struct sockaddr_in serveraddress , clientaddress;
	sockfd = socket(AF_INET,SOCK_STREAM,0);// socket created
	if(sockfd < 0){
		cout << "error opening socket" << endl;
		return NULL;
	}

	bzero((char *)&serveraddress,sizeof(serveraddress));
	serveraddress.sin_family = AF_INET;
	serveraddress.sin_addr.s_addr = INADDR_ANY;
	serveraddress.sin_port= htons(portno);
    serveraddress.sin_addr.s_addr = inet_addr(ip.c_str()); 
	
	//bind and start listning
	if(bind(sockfd,(struct sockaddr *)&serveraddress, sizeof(serveraddress)) < 0){
		cout << "error bind socket" << endl;
	}
	listen(sockfd,5);

	int clientsockfd[100];
	int sockcount=0;

	while(1)
	{
		socklen_t clilen = sizeof(clientaddress);
		clientsockfd[sockcount] = accept(sockfd,(struct sockaddr *)&clientaddress,&clilen);
		//do all the task given to listener
		struct threaddetails td;
		td.currentnode = args;
		td.socketfd = clientsockfd[sockcount];
		pthread_t th;
		pthread_create(&th,NULL,event,(void *)&td);
		pthread_detach(th);
		sockcount++;
	}
}