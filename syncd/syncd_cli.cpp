#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <thread>
#include <algorithm>
#include <string>

#include "syncd.h"
#include "swss/tokenize.h"

#define CLI_PORT 4000
#define CLI_PROMPT "syncd> "
#define BUFFER_SIZE 256

std::shared_ptr<std::thread> cli_thread;

volatile bool run_cli = false;
volatile bool run_user = false;

volatile int cli_server_socket = 0;
volatile int cli_client_socket = 0;

bool sendtoclient(const std::string& s)
{
    SWSS_LOG_ENTER();

    if (write(cli_client_socket, s.c_str(), s.length()) < 0)
    {
        SWSS_LOG_ERROR("write failed, errno: %s", strerror(errno));
        return false;
    }

    return true;
}

void cmd_exit()
{
    SWSS_LOG_ENTER();

    run_user = false;

    sendtoclient("bye\n");
}

void cmd_help(const std::vector<std::string>& args)
{
    SWSS_LOG_ENTER();

    std::string help = "\n\
    help            - this command\n\
    exit            - close cli connection\n\
    loglevel LEVEL  - sets debug logger level\n\n";

    sendtoclient(help);
}

void cmd_loglevel(const std::vector<std::string>& args)
{
    SWSS_LOG_ENTER();

    if (args.size() < 2)
    {
        sendtoclient("missing level parameter\n");
        return;
    }

    std::string slevel = args[1];

    std::transform(slevel.begin(), slevel.end(), slevel.begin(), ::toupper);

    swss::Logger::Priority level = swss::Logger::stringToPriority(slevel);

    swss::Logger::getInstance().setMinPrio(level);

    sendtoclient("log level changed to " + swss::Logger::priorityToString(level) + "\n");
}

std::string trim(const std::string &in)
{
    std::string s = in;

    s.erase(std::find_if(s.rbegin(), s.rend(),
                std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                std::not1(std::ptr_fun<int, int>(std::isspace))));

    return s;
}

void handle_cli_commands(const std::string& line)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("cli: %s", line.c_str());

    // TODO this must be more smart, to split multiple spaces
    auto tokens = swss::tokenize(trim(line), ' ');

    if (tokens.size() < 1)
    {
        return;
    }

    const std::string& cmd = tokens[0];

    if (cmd == "exit" || cmd == "quit")
    {
        cmd_exit();
    }
    else if (cmd == "loglevel")
    {
        cmd_loglevel(tokens);
    }
    else if (cmd == "help")
    {
        cmd_help(tokens);
    }
    else
    {
        sendtoclient("unknown command: " + line + "\n");
    }
}

void handle_cli()
{
    SWSS_LOG_ENTER();

    char buffer[BUFFER_SIZE];

    run_user = true;

    while (run_user)
    {
        if (!sendtoclient(CLI_PROMPT))
        {
            return;
        }

        // blocking
        ssize_t n = read(cli_client_socket, buffer, BUFFER_SIZE);

        if (n == 0)
        {
            SWSS_LOG_NOTICE("stream closed");
            return;
        }

        if (n < 0)
        {
            if (run_cli || run_user)
            {
                SWSS_LOG_ERROR("read failed, errno: %s", strerror(errno));
            }

            return;
        }

        const std::string& s = std::string(buffer, n);

        handle_cli_commands(s);
    }
}

void cli_thread_function()
{
    SWSS_LOG_ENTER();

    cli_server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (cli_server_socket < 0)
    {
        SWSS_LOG_ERROR("open socket failed, errno: %s", strerror(errno));
        return;
    }

    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(CLI_PORT);

    if (bind(cli_server_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0)
    {
        SWSS_LOG_ERROR("bind socket failed, errno: %s", strerror(errno));
        close(cli_server_socket);
        return;
    }

    const int backlog = 5;

    if (listen(cli_server_socket, backlog) != 0)
    {
        SWSS_LOG_ERROR("listen on socket failed, errno: %s", strerror(errno));
        close(cli_server_socket);
        return;
    }

    while (run_cli)
    {
        struct sockaddr_in cli_addr;

        memset(&cli_addr, 0, sizeof(cli_addr));

        socklen_t clilen = sizeof(cli_addr);

        SWSS_LOG_NOTICE("accepting connections for cli");

        cli_client_socket = accept(cli_server_socket, (struct sockaddr *) &cli_addr, &clilen);

        if (cli_client_socket < 0)
        {
            // don't log when run is false, that's mean we closed socket
            if (run_cli)
            {
                SWSS_LOG_ERROR("accept failed, errno: %s", strerror(errno));
            }

            break;
        }

        SWSS_LOG_NOTICE("client connected to cli");

        // we allow only 1 connection at given time
        handle_cli();

        close(cli_client_socket);
    }

    close(cli_server_socket);
}

void start_cli()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("starting cli");

    if (run_cli)
    {
        stop_cli();
    }

    run_cli = true;

    cli_thread = std::make_shared<std::thread>(std::thread(cli_thread_function));
}

void stop_cli()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("stopping cli");

    run_cli = false;

    // close actually don't break accept :/
    close(cli_client_socket);
    close(cli_server_socket);

    // cli_thread->join(); // TODO fix
}
