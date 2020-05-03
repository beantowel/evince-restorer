#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

using Clock = std::chrono::steady_clock;

const char logData[]{"./logData.txt"};
const auto timeOut = 15s;

static std::array<std::string, 2> logs;
static unsigned int idx = 0;

std::string exec(const char *cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::vector<int> readPids(const std::string &s) {
    std::vector<int> pids;
    int x;
    std::stringstream ss{s};
    while ((ss >> x)) {
        pids.push_back(x);
    }
    return pids;
}

void replaceAll(std::string &str, const std::string &from,
                const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos +=
            to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](int ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

std::string getOpenedPdfs() {
    std::string cmd{
        "lsof -p 0,$(pidof evince | sed \"s| |,|g\") | grep -o \"/.*.pdf$\""};
    auto s = exec(cmd.c_str());
    rtrim(s);
    std::cout << "getOpened:" << s << std::endl;
    return s;
}

std::string getOpenedPdfs(const char *path) {
    std::string result, buffer;
    std::ifstream logFile(path);
    if (logFile.is_open()) {
        while (std::getline(logFile, buffer)) {
            if (result.size() > 0) {
                result += '\n';
            }
            result += buffer;
        }
    } else {
        std::cout << "can't open file " << path << std::endl;
        result = "";
    }
    return result;
}

void writeOpenedPdfs(const char *path, const std::string &pdfs) {
    std::ofstream logFile(path);
    if (logFile.is_open()) {
        logFile << pdfs;
        std::cout << "written:\n[[[" << pdfs << "]]]\n";
    } else {
        std::cout << "can't open file " << path << std::endl;
    }
}

void launchEvince(std::string pdfs) {
    std::cout << "restoring last evince session:" << std::endl;
    std::cout << "pdfs:\n[[[" << pdfs << "]]]\n";
    if (pdfs.size() == 0) {
        system("evince &");
        std::cout << "empty session" << std::endl;
    } else {
        replaceAll(pdfs, "\n", "\' & evince \'");
        pdfs = "evince \'" + pdfs + "\' &";
        std::cout << "calling:\n" << pdfs << std::endl;
        system(pdfs.c_str());
    }
    auto openedPdfs = getOpenedPdfs();
    while (openedPdfs.size() <= 0) {
        openedPdfs = getOpenedPdfs();
        std::this_thread::sleep_for(0.5s);
    }
    std::cout << "at least one evince windows launched, openedPdfs:\n"
              << openedPdfs << std::endl;
}

decltype(auto) restore(const char *path) {
    auto pdfs = getOpenedPdfs();
    auto savedPdfs = getOpenedPdfs(path);
    if (pdfs.size() == 0 || savedPdfs.size() > 0) {
        return savedPdfs;
    } else {
        return pdfs;
    }
}

void log(std::chrono::seconds timeOut, const char *path, std::string &&pdfs) {
    logs[0] = std::move(pdfs);
    logs[1] = logs[0];
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    for (pdfs = getOpenedPdfs(); pdfs.size() > 0; pdfs = getOpenedPdfs()) {
        logs[idx] = std::move(pdfs);
        idx ^= 1;
        std::this_thread::sleep_for(timeOut);
    }
    writeOpenedPdfs(path, logs[idx]);
}

void intHandler(int signum) {
    std::cout << "signal: " << signum << std::endl;
    auto pdfs = getOpenedPdfs();
    writeOpenedPdfs(logData, pdfs);
    auto eviPids = readPids(exec("pidof evince"));
    for (auto &v : eviPids) {
        std::cout << "detected evince: " << v << std::endl;
        std::cout << "killing: " << v << std::endl;
        kill(v, SIGTERM);
    }
    eviPids = readPids(exec("pidof evince"));
    for (auto &v : eviPids) {
        std::cout << "detected evince: " << v << std::endl;
        std::cout << "killing: " << v << std::endl;
        kill(v, SIGKILL);
    }
    exit(0);
}

void termHandler(int signum) {
    std::cout << "signal: " << signum << std::endl;
    auto pdfs = getOpenedPdfs();
    if (pdfs.size() > 0) {
        // store and close session
        intHandler(SIGINT);
    } else {
        // fast reopen
        auto pdfs = logs[idx];
        std::cout << "fast reopen:\n[[[" << pdfs << "]]]" << std::endl;
        launchEvince(pdfs);
    }
}

void detectAndKill() {
    auto resPids = readPids(exec("pidof evince-restorer"));
    if (resPids.size() > 1) {
        auto thisPid = getpid();
        for (auto &v : resPids) {
            std::cout << "detected restorer: " << v << std::endl;
            if (v != thisPid) {
                std::cout << "send kill: " << v << std::endl;
                kill(v, SIGTERM);
            }
        }
        std::cout << "exit: " << thisPid << std::endl;
        exit(0);
    }
}

int main() {
    signal(SIGINT, intHandler);
    signal(SIGTERM, termHandler);
    detectAndKill();
    auto pdfs = restore(logData);
    launchEvince(pdfs);
    log(timeOut, logData, std::move(pdfs));
    return 0;
}