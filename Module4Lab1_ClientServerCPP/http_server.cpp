
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace fs = std::filesystem;

const int NUM_THREADS = 10;
const size_t MAX_REQUEST = 8192;

std::queue<int> task_queue;
std::mutex queue_mutex;
std::mutex output_mutex;
std::condition_variable queue_cv;

bool stop_pool = false;

// Send an entire response over a TCP socket.
bool send_all(int client_fd, const std::string &data)
{
    size_t total_sent = 0;

    while (total_sent < data.size())
    {
        ssize_t sent = send(
            client_fd,
            data.data() + total_sent,
            data.size() - total_sent,
            MSG_NOSIGNAL);

        if (sent < 0 && errno == EINTR)
        {
            continue;
        }

        if (sent <= 0)
        {
            return false;
        }

        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

// Identify the HTTP Content-Type from the file extension.
std::string get_content_type(const std::string &path)
{
    std::string extension = fs::path(path).extension().string();

    if (extension == ".html")
    {
        return "text/html";
    }

    if (extension == ".css")
    {
        return "text/css";
    }

    if (extension == ".png")
    {
        return "image/png";
    }

    if (extension == ".js")
    {
        return "application/javascript";
    }

    if (extension == ".txt")
    {
        return "text/plain";
    }

    return "application/octet-stream";
}

// Decode percent-encoded characters in a URL.
bool decode_url(const std::string &input, std::string &output)
{
    output.clear();

    for (size_t i = 0; i < input.size(); i++)
    {
        if (input[i] == '%')
        {
            if (i + 2 >= input.size())
            {
                return false;
            }

            char hex[3] = {
                input[i + 1],
                input[i + 2],
                '\0'};

            char *end = nullptr;
            long value = std::strtol(hex, &end, 16);

            if (end != hex + 2 || value == 0)
            {
                return false;
            }

            output += static_cast<char>(value);
            i += 2;
        }
        else
        {
            if (input[i] == '\0')
            {
                return false;
            }

            output += input[i];
        }
    }

    return true;
}

// Resolve the requested path safely inside www/.
bool resolve_path(
    const std::string &url,
    fs::path &result)
{
    std::string path = url;

    // Ignore query parameters.
    size_t query_pos = path.find('?');

    if (query_pos != std::string::npos)
    {
        path = path.substr(0, query_pos);
    }

    if (path.empty() || path[0] != '/')
    {
        return false;
    }

    std::string decoded;

    if (!decode_url(path, decoded))
    {
        return false;
    }

    if (decoded.find('\\') != std::string::npos)
    {
        return false;
    }

    if (decoded.empty() || decoded[0] != '/')
    {
        return false;
    }

    if (decoded == "/")
    {
        decoded = "/index.html";
    }

    std::error_code ec;

    fs::path root = fs::canonical("www", ec);

    if (ec)
    {
        return false;
    }

    // Remove the initial slash to make a relative path.
    fs::path relative = decoded.substr(1);

    fs::path requested = fs::weakly_canonical(
        root / relative,
        ec);

    if (ec)
    {
        return false;
    }

    // Ensure the resolved path stays within www/.
    fs::path difference = requested.lexically_relative(root);

    if (difference.empty() || difference.is_absolute())
    {
        return false;
    }

    for (const auto &component : difference)
    {
        if (component == "..")
        {
            return false;
        }
    }

    result = requested;
    return true;
}

// Read a requested file, including binary images.
bool get_file_content(
    const fs::path &path,
    std::string &content)
{
    std::error_code ec;

    if (!fs::is_regular_file(path, ec) || ec)
    {
        return false;
    }

    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();

    if (file.bad())
    {
        return false;
    }

    content = stream.str();
    return true;
}

// Send an HTTP response to a connected client.
void send_response(
    int client_fd,
    int status,
    const std::string &content,
    const std::string &content_type)
{
    std::string status_text =
        (status == 200) ? "200 OK" : "404 Not Found";

    std::string response =
        "HTTP/1.1 " + status_text + "\r\n";

    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += content;

    if (!send_all(client_fd, response))
    {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "Failed to send response" << std::endl;
    }
}

// Read, parse, and handle a client HTTP request.
void handle_http_client(int client_fd)
{
    std::string request;
    char buffer[1024];

    while (request.find("\r\n\r\n") == std::string::npos)
    {
        ssize_t bytes_read = recv(
            client_fd,
            buffer,
            sizeof(buffer),
            0);

        if (bytes_read < 0 && errno == EINTR)
        {
            continue;
        }

        if (bytes_read <= 0)
        {
            close(client_fd);
            return;
        }

        request.append(buffer, bytes_read);

        if (request.size() > MAX_REQUEST)
        {
            close(client_fd);
            return;
        }
    }

    // Parse the request line.
    std::istringstream request_stream(request);

    std::string method;
    std::string url;
    std::string version;

    request_stream >> method >> url >> version;

    {
        std::lock_guard<std::mutex> lock(output_mutex);

        std::cout << "Request: "
                  << method << " " << url
                  << std::endl;
    }

    // Only HTTP GET is supported in this lab.
    if (method != "GET" || version.rfind("HTTP/", 0) != 0)
    {
        send_response(
            client_fd,
            404,
            "<h1>404 Not Found</h1>",
            "text/html");

        close(client_fd);
        return;
    }

    fs::path file_path;

    // Reject invalid and unsafe paths.
    if (!resolve_path(url, file_path))
    {
        send_response(
            client_fd,
            404,
            "<h1>404 Not Found</h1>",
            "text/html");

        close(client_fd);
        return;
    }

    std::string content;

    // Attempt to read the requested file.
    if (!get_file_content(file_path, content))
    {
        send_response(
            client_fd,
            404,
            "<h1>404 Not Found</h1>",
            "text/html");
    }
    else
    {
        send_response(
            client_fd,
            200,
            content,
            get_content_type(file_path.string()));
    }

    close(client_fd);
}

// Worker threads retrieve connections from the shared queue.
void worker_thread(int worker_id)
{
    while (true)
    {
        int client_fd;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            queue_cv.wait(lock, []
                          { return stop_pool || !task_queue.empty(); });

            if (stop_pool && task_queue.empty())
            {
                return;
            }

            client_fd = task_queue.front();
            task_queue.pop();
        }

        {
            std::lock_guard<std::mutex> lock(output_mutex);

            std::cout << "Worker " << worker_id
                      << " processing client "
                      << client_fd << std::endl;
        }

        handle_http_client(client_fd);
    }
}

int main(int argc, char *argv[])
{
    int port = 8080;

    if (argc > 1)
    {
        port = std::atoi(argv[1]);
    }

    if (port < 1 || port > 65535)
    {
        std::cerr << "Invalid port" << std::endl;
        return 1;
    }

    // Ignore broken-pipe signals.
    std::signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            server_fd,
            reinterpret_cast<sockaddr *>(&addr),
            sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 20) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    // Create a fixed pool of worker threads.
    std::vector<std::thread> workers;

    for (int i = 0; i < NUM_THREADS; i++)
    {
        workers.emplace_back(worker_thread, i + 1);
    }

    std::cout << "HTTP server listening on port "
              << port << std::endl;

    std::cout << "Worker threads: "
              << NUM_THREADS << std::endl;

    // Accept incoming client connections.
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            reinterpret_cast<sockaddr *>(&client_addr),
            &client_len);

        if (client_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("accept");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(client_fd);
        }

        queue_cv.notify_one();
    }

    // Reserved for a future graceful shutdown.
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_pool = true;
    }

    queue_cv.notify_all();

    for (auto &worker : workers)
    {
        worker.join();
    }

    close(server_fd);

    return 0;
}
