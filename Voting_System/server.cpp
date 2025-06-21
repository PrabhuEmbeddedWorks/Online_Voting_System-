// VotingServer.cpp
#include <iostream>       // Standard I/O stream
#include <string>         // For std::string
#include <sstream>        // For std::stringstream to build responses
#include <iomanip>        // For formatting hex output
#include <cstring>        // For string functions like memset, strcmp
#include <sqlite3.h>      // SQLite C/C++ library
#include <openssl/sha.h>  // For SHA256 hashing
#include <netinet/in.h>   // For socket address structures
#include <unistd.h>       // For POSIX functions: read(), write(), close()
using namespace std;

#define PORT 8080         // Server port

class Database {
    sqlite3* db;  // Pointer to the SQLite database connection

public:
    Database() {
        if (sqlite3_open("voting.db", &db)) {  // Open or create 'voting.db'
            cerr << "Database open error: " << sqlite3_errmsg(db) << endl;
            exit(1);  // Exit if DB fails to open
        }

        const char* users = "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password_hash TEXT NOT NULL, voted INTEGER DEFAULT 0);";  // Users table
        const char* candidates = "CREATE TABLE IF NOT EXISTS candidates (name TEXT PRIMARY KEY, votes INTEGER DEFAULT 0);";  // Candidates table
        sqlite3_exec(db, users, nullptr, nullptr, nullptr);  // Create users table
        sqlite3_exec(db, candidates, nullptr, nullptr, nullptr);  // Create candidates table
        sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Alice', 0);", nullptr, nullptr, nullptr);  // Insert Alice if not exists
        sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Bob', 0);", nullptr, nullptr, nullptr);    // Insert Bob if not exists
        sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Charlie', 0);", nullptr, nullptr, nullptr);  // Insert Charlie if not exists
    }

    ~Database() {
        sqlite3_close(db);  // Close the DB on destruction
    }

    sqlite3* get() {
        return db;  // Return raw DB pointer for queries
    }
};

class Hash {
public:
    static string sha256(const string& input) {
        unsigned char hash[SHA256_DIGEST_LENGTH];  // Array to store the hash
        SHA256((const unsigned char*)input.c_str(), input.size(), hash);  // Compute hash

        stringstream ss;  // For hex string output
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            ss << hex << setw(2) << setfill('0') << (int)hash[i];  // Convert each byte to hex

        return ss.str();  // Return hashed string
    }
};

class VotingServer {
    Database db;  // Create a DB object

public:
    void run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);  // Create TCP socket
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));  // Set socket options

        sockaddr_in address{};  // Zero-initialize
        address.sin_family = AF_INET;  // IPv4
        address.sin_addr.s_addr = INADDR_ANY;  // Accept all connections
        address.sin_port = htons(PORT);  // Host to network short

        bind(server_fd, (sockaddr*)&address, sizeof(address));  // Bind socket
        listen(server_fd, 3);  // Listen for up to 3 clients

        cout << "Server running on port " << PORT << "...\n";

        while (true) {
            socklen_t addrlen = sizeof(address);
            int new_socket = accept(server_fd, (sockaddr*)&address, &addrlen);  // Accept new client
            handleClient(new_socket);  // Handle client interaction
        }
    }

    void handleClient(int clientSocket) {
        char buffer[1024] = {0};  // Receive buffer
        read(clientSocket, buffer, 1024);  // Read from client
        string request(buffer);  // Convert to string
        stringstream response;  // Prepare response stream

        if (request.find("REGISTER") == 0) {
            handleRegister(request, response);  // Handle registration
        } else if (request.find("LOGIN") == 0) {
            handleLogin(request, response);  // Handle login
        } else if (request.find("VOTE") == 0) {
            handleVote(request, response);  // Handle voting
        } else if (request.find("RESULTS") == 0) {
            handleResults(request, response);  // Handle results access
        } else {
            response << "Invalid request.";  // Fallback error
        }

        send(clientSocket, response.str().c_str(), response.str().size(), 0);  // Send response
        close(clientSocket);  // Close client socket
    }

    void handleRegister(const string& req, stringstream& res) {
        char uname[512], pwd[512];
        sscanf(req.c_str(), "REGISTER %s %s", uname, pwd);  // Parse username and password
        string hash = Hash::sha256(pwd);  // Hash the password

        string sql = "INSERT INTO users VALUES ('" + string(uname) + "', '" + hash + "', 0);";  // SQL insert query
        if (sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK)
            res << "Registration successful.";  // Success message
        else
            res << "Username already exists.";  // Duplicate username
    }

    void handleLogin(const string& req, stringstream& res) {
        char uname[512], pwd[512];
        sscanf(req.c_str(), "LOGIN %s %s", uname, pwd);  // Parse login credentials

        string sql = "SELECT password_hash FROM users WHERE username='" + string(uname) + "';";  // Query to check user
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);  // Prepare statement

        if (sqlite3_step(stmt) == SQLITE_ROW) {  // If user found
            string storedHash = (const char*)sqlite3_column_text(stmt, 0);  // Get stored hash
            if (storedHash == Hash::sha256(pwd))  // Compare hashes
                res << "Login success. Candidates: Alice, Bob, Charlie.";  // Valid credentials
            else
                res << "Invalid password.";  // Wrong password
        } else {
            res << "Username not found.";  // No such user
        }
        sqlite3_finalize(stmt);  // Clean up
    }

    void handleVote(const string& req, stringstream& res) {
        char uname[512], cname[512];
        sscanf(req.c_str(), "VOTE %s %s", uname, cname);  // Parse voter and candidate

        string check = "SELECT voted FROM users WHERE username='" + string(uname) + "';";  // Check if already voted
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db.get(), check.c_str(), -1, &stmt, nullptr);  // Prepare statement

        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {  // If not yet voted
            sqlite3_exec(db.get(), ("UPDATE candidates SET votes = votes + 1 WHERE name='" + string(cname) + "';").c_str(), nullptr, nullptr, nullptr);  // Increment vote
            sqlite3_exec(db.get(), ("UPDATE users SET voted = 1 WHERE username='" + string(uname) + "';").c_str(), nullptr, nullptr, nullptr);  // Mark user as voted
            res << "Vote successful.";  // Success message
        } else {
            res << "Vote failed or already voted.";  // Fail case
        }
        sqlite3_finalize(stmt);  // Finalize
    }

    void handleResults(const string& req, stringstream& res) {
        char pwd[512];
        sscanf(req.c_str(), "RESULTS %s", pwd);  // Extract password

        if (Hash::sha256(pwd) == Hash::sha256("admin123")) {  // Admin password check
            sqlite3_stmt* stmt;
            string sql = "SELECT name, votes FROM candidates;";  // Query all results
            sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
            res << "--- Results ---\n";  // Print header
            while (sqlite3_step(stmt) == SQLITE_ROW) {  // Iterate rows
                res << (const char*)sqlite3_column_text(stmt, 0) << ": " << sqlite3_column_int(stmt, 1) << " votes\n";  // Display each result
            }
            sqlite3_finalize(stmt);  // Finalize
        } else {
            res << "Unauthorized access.";  // Wrong admin password
        }
    }
};

int main() {
    VotingServer server;  // Create server object
    server.run();  // Start server
    return 0;  // Exit main
}
