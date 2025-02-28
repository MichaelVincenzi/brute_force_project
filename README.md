# README.md

# Brute Force Project

This project implements a brute-force login script using cURL and a thread pool. It is designed to read usernames and passwords from files and attempt to log in with each combination. The script can be configured through a JSON file, allowing for customization of various parameters such as the login URL, number of threads, and request delay.

## Features

- Concurrent login attempts using a thread pool
- Configurable through a JSON file
- Option to stop on successful login
- Random user agent selection for each request
- Logging of attempts and responses

## Requirements

- C++11 or later
- cURL library
- nlohmann/json library

## Installation

1. Clone the repository:
   ```
   git clone <repository-url>
   cd brute_force_project
   ```

2. Install the required libraries (if not already installed).

3. Compile the project:
   ```
   g++ -o bruteforce src/bruteforce.cpp -lcurl -lpthread
   ```

## Usage

To run the script, provide a configuration JSON file as an argument:

```
./bruteforce <config_file.json>
```

### Configuration File

The configuration file should be in JSON format and include the following fields:

- `login_url`: The URL of the login endpoint.
- `stop_on_success`: A boolean flag to stop all threads once valid credentials are found.
- `threads`: The number of threads to use for concurrent login attempts.
- `delay`: The delay between requests in milliseconds.
- `proxy`: (Optional) The proxy URL to use for requests.
- `user_agents`: The path to a file containing user agents.
- `log_file`: The path to the log file where attempts will be recorded.
- `usernames`: The path to the file containing usernames.
- `passwords`: The path to the file containing passwords.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request for any improvements or bug fixes.

## License

This project is licensed under the MIT License. See the LICENSE file for details.