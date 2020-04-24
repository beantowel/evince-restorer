#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
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

std::string getOpenedPdfs() {
    std::string cmd{
        "lsof -p 0,$(pidof evince | sed \"s/ /,/g\") | grep -o \"/.*.pdf$\""};
    auto s = exec(cmd.c_str());
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
        std::cout << "written:" << pdfs << std::endl;
    } else {
        std::cout << "can't open file " << path << std::endl;
    }
    return;
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

void launchEvince(std::string pdfs) {
    std::cout << "restoring last evince session:" << std::endl;
    std::cout << "pdfs:" << pdfs << std::endl;
    if (pdfs.size() == 0) {
        system("evince");
    } else {
        replaceAll(pdfs, "\n", "\' \'");
        pdfs = "evince \'" + pdfs + "\'";
        std::cout << "calling:" << pdfs << std::endl;
        system(pdfs.c_str());
    }
    std::cout << "restored evince windows closed" << std::endl;
    return;
}

void launchAndLog(std::chrono::seconds timeOut, const char *path) {
    auto pdfs = getOpenedPdfs(path);
    auto fut = std::async(std::launch::async, launchEvince, pdfs);
    std::array<std::string, 2> logs;
    unsigned int idx = 0;
    logs[0] = std::move(pdfs);
    logs[1] = logs[0];
    while (fut.wait_for(timeOut) != std::future_status::ready) {
        logs[idx] = std::move(getOpenedPdfs());
        idx ^= 1;
    }
    for (pdfs = getOpenedPdfs(); pdfs.size() > 0; pdfs = getOpenedPdfs()) {
        logs[idx] = std::move(pdfs);
        idx ^= 1;
        std::this_thread::sleep_for(timeOut);
    }
    writeOpenedPdfs(path, logs[idx]);
    return;
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

void sighandler(int signum) {
    std::cout << "Signal: " << signum << std::endl;
    writeOpenedPdfs(logData, getOpenedPdfs());
    auto eviPids = readPids(exec("pidof evince"));
    for (auto &v : eviPids) {
        std::cout << "Killing: " << v << std::endl;
        kill(v, SIGINT);
    }
    exit(signum);
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
        exit(0);
    }
    return;
}

int main() {
    detectAndKill();
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    launchAndLog(timeOut, logData);
    return 0;
}