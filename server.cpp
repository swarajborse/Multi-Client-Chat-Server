#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<string>
#include<mutex>
#include<map>
#include<thread>
#include<vector>
#include<algorithm>

using namespace std;

map<int,string>clients;
mutex client_mutex;
const size_t MAX_BUFFER = 8192;

bool sendAll(int sock, const string& msg){
    size_t totalSent = 0;
    while(totalSent < msg.size()){
        ssize_t sent = send(sock, msg.c_str() + totalSent,
                            msg.size() - totalSent, 0);
        if(sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

void broadcast(string message,int sender_socket){
    lock_guard<mutex> lock(client_mutex);
    vector<int> deadClients;
    for(auto& client : clients){
        if(client.first != sender_socket){
            if(!sendAll(client.first, message)){
                deadClients.push_back(client.first);
            }
        }
    }
    for(int fd : deadClients){
        clients.erase(fd);
    }
}
void handle_client(int clientSocket){
    char buffer[1024];
    string data;
    string username;
while(true){
   int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
   if(bytes<=0){
       close(clientSocket);
       return;
    }
    data.append(buffer, bytes);
    if(data.size() > MAX_BUFFER){
        string err = "Username too long. Disconnecting.\n";
        sendAll(clientSocket, err);
        close(clientSocket);
        return;
    }
    size_t pos = data.find('\n');
    if(pos != string::npos){
        username = data.substr(0, pos);
        data.erase(0, pos + 1);
        break;
    }

       
}

    // Validate username: strip control chars, enforce length, reject duplicates
    username.erase(remove_if(username.begin(), username.end(),
        [](unsigned char c){ return !isprint(c); }), username.end());

    if(username.empty() || username.size() > 32){
        string err = "Invalid username (must be 1-32 printable characters). Disconnecting.\n";
        sendAll(clientSocket, err);
        close(clientSocket);
        return;
    }

    {
        lock_guard<mutex> lock(client_mutex);
        for(auto& [fd, name] : clients){
            if(name == username){
                string err = "Username '" + username + "' is already taken. Disconnecting.\n";
                sendAll(clientSocket, err);
                close(clientSocket);
                return;
            }
        }
        clients[clientSocket] = username;
    }

  string joinMsg = username + " has joined the chat!\n";
  cout<<joinMsg;
    broadcast(joinMsg, clientSocket);
     size_t pos2;
        while((pos2=data.find('\n')) != string::npos){
            string msg = data.substr(0, pos2);
            string fullMsg = username + ": " + msg + "\n";
            cout<<fullMsg;
            broadcast(fullMsg, clientSocket);
            data.erase(0, pos2 + 1);
        }
    
    
while(true){
    int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
    if(bytes <= 0){
        cout<<username<<" disconnected"<<endl;
        {
            lock_guard<mutex> lock(client_mutex);
            clients.erase(clientSocket);
        }
        close(clientSocket);
        string leaveMsg = username + " has left the chat!\n";
        broadcast(leaveMsg, -1);
        break;
    
    }
    data.append(buffer, bytes);
    if(data.size() > MAX_BUFFER){
        cout << username << " exceeded buffer limit, disconnecting." << endl;
        {
            lock_guard<mutex> lock(client_mutex);
            clients.erase(clientSocket);
        }
        close(clientSocket);
        string leaveMsg = username + " was disconnected (flood).\n";
        broadcast(leaveMsg, -1);
        break;
    }
    size_t pos;
    while((pos = data.find('\n')) != string::npos){
        
        string msg = data.substr(0, pos);
        if(msg.empty()){
            data.erase(0, pos + 1);
            continue;

        } 
        string fullMsg = username + ": " + msg + "\n";  
        cout<<fullMsg;
        broadcast(fullMsg, clientSocket);
        data.erase(0, pos + 1);
    }

}
}
int main(){
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0){
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
        perror("bind");
        close(serverSocket);
        return 1;
    }
    if(listen(serverSocket, SOMAXCONN) < 0){
        perror("listen");
        close(serverSocket);
        return 1;
    }
    cout<<"Server is running on port 8080..."<<endl;
while(true){
    int clientSocket = accept(serverSocket,nullptr,nullptr);
    if(clientSocket < 0){
        perror("accept");
        continue;
    }
    thread t(handle_client, clientSocket);
    t.detach();
}
close(serverSocket);
return 0;

}








