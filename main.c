#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define BACKLOG 10

// Function to get MIME type based on file extension
const char* get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "text/plain";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".json") == 0) return "application/json";

    return "text/plain";
}

// Function to send HTTP error response
void send_error(int client_socket, int status_code, const char *message) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>%d %s</h1><p>%s</p></body></html>\n",
        status_code, message, status_code, message, message);

    send(client_socket, response, strlen(response), 0);
}

// Function to send a file
void send_file(int client_socket, const char *filepath) {
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        send_error(client_socket, 404, "Not Found");
        return;
    }

    // Get file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // Send headers
    char headers[BUFFER_SIZE];
    const char *mime_type = get_mime_type(filepath);
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type, (long)file_size);

    send(client_socket, headers, strlen(headers), 0);

    // Send file content
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

void handle_request(int client_socket, const char *request) {
    char method[16], path[256];

    // Parse request line
    if (sscanf(request, "%s %s", method, path) != 2) {
        send_error(client_socket, 400, "Bad Request");
        return;
    }

    printf("Request: %s %s\n", method, path);

    // Only handle GET requests
    if (strcmp(method, "GET") != 0) {
        send_error(client_socket, 501, "Not Implemented");
        return;
    }

    // Route handling
    if (strcmp(path, "/") == 0) {
        // Serve index.html or default page
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>C HTTP Server</title>\n"
            "    <style>\n"
            "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
            "        h1 { color: #333; }\n"
            "    </style>\n"
            "</head>\n"
            "<body>\n"
            "    <h1>Welcome to C HTTP Server</h1>\n"
            "    <p>Available routes:</p>\n"
            "    <ul>\n"
            "        <li><a href=\"/\">Home</a></li>\n"
            "        <li><a href=\"/about\">About</a></li>\n"
            "        <li><a href=\"/api/status\">API Status</a></li>\n"
            "    </ul>\n"
            "</body>\n"
            "</html>\n";

        send(client_socket, response, strlen(response), 0);
    }
    else if (strcmp(path, "/about") == 0) {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>About</h1><p>This is a simple HTTP server written in C.</p></body></html>\n";

        send(client_socket, response, strlen(response), 0);
    }
    else if (strcmp(path, "/api/status") == 0) {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"status\": \"ok\", \"message\": \"Server is running\"}\n";

        send(client_socket, response, strlen(response), 0);
    }
    else if (strncmp(path, "/static/", 8) == 0) {
        // Serve static files from ./static directory
        char filepath[512];
        snprintf(filepath, sizeof(filepath), ".%s", path);
        send_file(client_socket, filepath);
    }
    else {
        send_error(client_socket, 404, "Not Found");
    }
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};

    // Read client request
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        perror("read failed");
        close(client_socket);
        return;
    }

    handle_request(client_socket, buffer);
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("HTTP Server running on port %d\n", PORT);
    printf("Visit http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to stop the server\n\n");

    // Main loop
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        // Print client info
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(address.sin_port));

        handle_client(client_socket);
    }

    close(server_fd);
    return 0;
}
