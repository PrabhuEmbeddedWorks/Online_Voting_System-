#include <iostream>          // for input/output operations
#include <string>            // for using std::string
#include <cstring>           // for C-style string operations
#include <sqlite3.h>         // SQLite3 database library
#include <openssl/sha.h>     // OpenSSL SHA256 functions
#include <iomanip>           // for std::setw and std::setfill (used in hash formatting)
#include <sstream>           // for std::stringstream
#include <netinet/in.h>      // for sockaddr_in and network constants
#include <unistd.h>          // for read, write, close system calls

#define PORT 8080             // Port number on which server will listen
using namespace std;

// Function to compute SHA256 hash of a string
string sha256(const string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input.c_str(), input.size(), hash); // compute hash
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << hex << setw(2) << setfill('0') << (int)hash[i]; // convert bytes to hex
    return ss.str(); // return hex string
}

// Function to setup SQLite3 database and initialize tables
void setupDatabase(sqlite3* &db) {
    int rc = sqlite3_open("voting.db", &db); // open (or create) database file
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        exit(1);
    }

    // Create users and candidates tables if they don't exist
    const char* usersTable = "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password_hash TEXT NOT NULL, voted INTEGER DEFAULT 0);";
    const char* candidatesTable = "CREATE TABLE IF NOT EXISTS candidates (name TEXT PRIMARY KEY, votes INTEGER DEFAULT 0);";

    sqlite3_exec(db, usersTable, nullptr, nullptr, nullptr);
    sqlite3_exec(db, candidatesTable, nullptr, nullptr, nullptr);

    // Insert default candidates if not already present
    sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Alice', 0);", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Bob', 0);", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "INSERT OR IGNORE INTO candidates VALUES ('Charlie', 0);", nullptr, nullptr, nullptr);
}

// Handles incoming client request
void handleClient(int new_socket, sqlite3* db) {
    char buffer[1024] = {0};                     // buffer for client message
    read(new_socket, buffer, 1024);              // read request from client
    string request(buffer);                      // convert to C++ string
    stringstream response;                       // buffer to store server's response

    // Handle registration
    if (request.find("REGISTER") == 0) {
        string uname, pwd;
        sscanf(buffer, "REGISTER %s %s", buffer, buffer + 512); // split into uname and pwd
        uname = string(buffer);
        pwd = string(buffer + 512);

        string hash = sha256(pwd); // hash the password
        string sql = "INSERT INTO users VALUES ('" + uname + "', '" + hash + "', 0);";

        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK)
            response << "Registration successful.";
        else
            response << "Username already exists.";

    // Handle login
    } else if (request.find("LOGIN") == 0) {
        string uname, pwd;
        sscanf(buffer, "LOGIN %s %s", buffer, buffer + 512);
        uname = string(buffer);
        pwd = string(buffer + 512);

        string sql = "SELECT password_hash FROM users WHERE username='" + uname + "';";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            string storedHash = (const char*)sqlite3_column_text(stmt, 0);
            if (storedHash == sha256(pwd))
                response << "Login success. Candidates: Alice, Bob, Charlie.";
            else
                response << "Invalid password.";
        } else {
            response << "Username not found.";
        }
        sqlite3_finalize(stmt);

    // Handle voting
    } else if (request.find("VOTE") == 0) {
        string uname, cname;
        sscanf(buffer, "VOTE %s %s", buffer, buffer + 512);
        uname = string(buffer);
        cname = string(buffer + 512);

        string check = "SELECT voted FROM users WHERE username='" + uname + "';";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, check.c_str(), -1, &stmt, nullptr);

        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            // Update vote count and mark user as voted
            sqlite3_exec(db, ("UPDATE candidates SET votes = votes + 1 WHERE name='" + cname + "';").c_str(), nullptr, nullptr, nullptr);
            sqlite3_exec(db, ("UPDATE users SET voted = 1 WHERE username='" + uname + "';").c_str(), nullptr, nullptr, nullptr);
            response << "Vote successful.";
        } else {
            response << "Vote failed or already voted.";
        }
        sqlite3_finalize(stmt);

    // Handle admin result viewing
    } else if (request.find("RESULTS") == 0) {
        string pwd;
        sscanf(buffer, "RESULTS %s", buffer);
        pwd = string(buffer);

        if (sha256(pwd) == sha256("admin123")) {
            sqlite3_stmt* stmt;
            string sql = "SELECT name, votes FROM candidates;";
            sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            response << "--- Results ---\n";
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                response << (const char*)sqlite3_column_text(stmt, 0) << ": " << sqlite3_column_int(stmt, 1) << " votes\n";
            }
            sqlite3_finalize(stmt);
        } else {
            response << "Unauthorized access.";
        }
    } else {
        response << "Invalid request.";
    }

    // Send response to client and close socket
    send(new_socket, response.str().c_str(), response.str().size(), 0);
    close(new_socket);
}

// Main function to setup server and listen for connections
int main() {
    sqlite3* db;
    setupDatabase(db); // Initialize database

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // Create socket
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)); // Reuse port

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address)); // Bind socket to port
    listen(server_fd, 3); // Listen for connections

    cout << "Server running on port " << PORT << "...\n";
    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen); // Accept client
        handleClient(new_socket, db); // Process client request
    }

    sqlite3_close(db); // Close database
    return 0;
}