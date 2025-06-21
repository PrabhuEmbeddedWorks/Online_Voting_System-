// VotingClient.cpp
#include <iostream>         // For input and output
#include <string>           // For using std::string
#include <unistd.h>         // For read(), write(), close() - POSIX functions
#include <arpa/inet.h>      // For socket address structures and inet_pton
#include <termios.h>        // For controlling terminal input (used for hidden password input)
using namespace std;

#define PORT 8080           // Define the port to connect to the server

// Class for securely reading passwords (without echoing on terminal)
class Password {
public:
    static string getHidden(const string& prompt = "Enter password: ") {
        cout << prompt;                // Show password prompt
        termios oldt, newt;            // Terminal settings
        string password;

        tcgetattr(STDIN_FILENO, &oldt);     // Get current terminal settings
        newt = oldt;                         // Copy settings
        newt.c_lflag &= ~ECHO;              // Disable echo (no character display)
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Apply new settings immediately
        getline(cin >> ws, password);       // Read password securely
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore original terminal settings
        cout << endl;
        return password;                    // Return the entered password
    }
};

// Class representing the client-side logic for the voting system
class VotingClient {
public:
    void sendRequest(const string& message) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);  // Create TCP socket

        sockaddr_in serv_addr{};                     // Structure to hold server address
        serv_addr.sin_family = AF_INET;              // IPv4
        serv_addr.sin_port = htons(PORT);            // Convert port to network byte order

        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);  // Convert IP address from text to binary
        connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr));  // Connect to the server

        send(sock, message.c_str(), message.length(), 0);  // Send message to server

        char buffer[2048] = {0};                 // Buffer to store server response
        read(sock, buffer, 2048);                // Read response from server
        cout << "\nServer: " << buffer << endl;  // Display server response

        close(sock);                             // Close the socket connection
    }

    void run() {
        int choice;           // Menu choice input
        string uname, pwd, cname;  // Username, password, candidate name

        while (true) {  // Loop for menu operations
            cout << "\n--- Online Voting Client ---\n";
            cout << "1. Register\n2. Login and Vote\n3. View Results (Admin)\n0. Exit\nEnter choice: ";
            cin >> choice;  // Read user choice

            switch (choice) {
                case 1:
                    cout << "Enter username: ";
                    cin >> uname;                                            // Get username
                    pwd = Password::getHidden("Enter password: ");          // Get password securely
                    sendRequest("REGISTER " + uname + " " + pwd);           // Send REGISTER request
                    break;

                case 2:
                    cout << "Enter username: ";
                    cin >> uname;                                            // Get username
                    pwd = Password::getHidden("Enter password: ");          // Get password
                    sendRequest("LOGIN " + uname + " " + pwd);              // Send LOGIN request
                    cout << "\nEnter candidate to vote (Alice/Bob/Charlie): ";
                    cin >> cname;                                           // Get candidate choice
                    sendRequest("VOTE " + uname + " " + cname);             // Send VOTE request
                    break;

                case 3:
                    pwd = Password::getHidden("Enter admin password: ");    // Get admin password
                    sendRequest("RESULTS " + pwd);                          // Send RESULTS request
                    break;

                case 0:
                    cout << "Exiting client...\n";  // Exit message
                    return;                         // Exit the program

                default:
                    cout << "Invalid choice.\n";    // Invalid option handler
            }
        }
    }
};

int main() {
    VotingClient client;  // Create a client object
    client.run();         // Run the client menu and interaction
    return 0;             // Exit main
}
