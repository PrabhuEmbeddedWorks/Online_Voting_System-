#include <iostream>       // For standard input/output
#include <string>         // For using std::string
#include <cstring>        // For memset and C-style strings
#include <unistd.h>       // For close(), read(), and write()
#include <arpa/inet.h>    // For inet_pton(), sockaddr_in, htons()
#include <termios.h>      // For hiding password input in terminal

#define PORT 8080         // Server will listen on this port
using namespace std;

// 🔐 Function to get hidden password input (no echo in terminal)
string getHiddenPassword(const string& prompt = "Enter password: ") {
    cout << prompt;
    termios oldt, newt;
    string password;

    // Save old terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Disable echo
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Read input (skip leading whitespace)
    getline(cin >> ws, password);

    // Restore terminal echo
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    cout << endl;

    return password;
}

// 📤 Function to send request to server and print response
void sendRequest(const string& message) {
    int sock = 0;                         // Socket descriptor
    struct sockaddr_in serv_addr;        // Server address structure
    char buffer[2048] = {0};             // Buffer to receive server response

    // Create a socket (IPv4, TCP)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Socket creation error" << endl;
        return;
    }

    // Set server IP and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);    // Convert port to network byte order

    // Convert IP from text to binary (localhost: 127.0.0.1)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address / Address not supported" << endl;
        return;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection Failed" << endl;
        return;
    }

    // Send message to server
    send(sock, message.c_str(), message.length(), 0);

    // Read response from server
    read(sock, buffer, 2048);

    // Display server's response
    cout << "\nServer: " << buffer << endl;

    // Close the socket
    close(sock);
}

// 🧠 Main client-side interaction loop
int main() {
    int choice;
    string uname, pwd, cname;

    while (true) {
        // Display main menu
        cout << "\n--- Online Voting Client ---\n";
        cout << "1. Register\n2. Login and Vote\n3. View Results (Admin)\n0. Exit\nEnter choice: ";
        cin >> choice;

        switch (choice) {
            case 1: // 👤 Registration
                cout << "Enter username: ";
                cin >> uname;
                pwd = getHiddenPassword("Enter password: ");
                sendRequest("REGISTER " + uname + " " + pwd);
                break;

            case 2: // 🔐 Login and 🗳️ Vote
                cout << "Enter username: ";
                cin >> uname;
                pwd = getHiddenPassword("Enter password: ");
                sendRequest("LOGIN " + uname + " " + pwd);

                // Vote input immediately after login
                cout << "\nEnter candidate to vote (Alice/Bob/Charlie): ";
                cin >> cname;
                sendRequest("VOTE " + uname + " " + cname);
                break;

            case 3: // 🧑‍💼 Admin: View results
                pwd = getHiddenPassword("Enter admin password: ");
                sendRequest("RESULTS " + pwd);
                break;

            case 0: // 🚪 Exit
                cout << "Exiting client...\n";
                return 0;

            default: // ❌ Invalid input
                cout << "Invalid choice. Try again.\n";
        }
    }
}
