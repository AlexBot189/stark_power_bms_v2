#include "GPIOMonitor.hpp"
#include <gpiod.h>
#include <iostream>
#include <unistd.h>
#include <ctime>

GPIOMonitor::GPIOMonitor(const std::string& chip_path)
    : chip_(nullptr), running_(false) {
    chip_ = gpiod_chip_open(chip_path.c_str());
    if (!chip_) {
        std::cerr << "[ERROR] Failed to open GPIO chip: " << chip_path << std::endl;
    } else {
        std::cout << "[INFO] GPIO chip opened: " << chip_path << std::endl;
    }
}

GPIOMonitor::~GPIOMonitor() {
    stop();
    if (chip_) {
        gpiod_chip_close(chip_);
        std::cout << "[INFO] GPIO chip closed." << std::endl;
    }
}

bool GPIOMonitor::watch_line(int line_num, EventCallback callback) {
    if (!chip_) {
        std::cerr << "[ERROR] GPIO chip not available." << std::endl;
        return false;
    }

    struct gpiod_line* line = gpiod_chip_get_line(chip_, line_num);
    if (!line) {
        std::cerr << "[ERROR] Failed to get GPIO line: " << line_num << std::endl;
        return false;
    }

    if (gpiod_line_request_both_edges_events(line, "touch_monitor") < 0) {
        std::cerr << "[ERROR] Failed to request events for line: " << line_num << std::endl;
        return false;
    }

    if (!running_) running_ = true;

    monitor_threads_[line_num] = std::thread([this, line, line_num, callback]() {
        struct gpiod_line_event event;
        while (running_) {
            int ret = gpiod_line_event_wait(line, nullptr);
            if (ret < 0) {
                std::cerr << "[ERROR] Waiting for event on line " << line_num << std::endl;
                break;
            }
            if (ret == 0) continue;

            if (gpiod_line_event_read(line, &event) < 0) {
                std::cerr << "[ERROR] Reading event on line " << line_num << std::endl;
                continue;
            }

            if (callback) {
                callback(line_num, event.event_type);
            }
        }

        gpiod_line_release(line);
        std::cout << "[INFO] Monitoring stopped for GPIO line " << line_num << std::endl;
    });

    return true;
}

void GPIOMonitor::stop() {
    running_ = false;
    for (auto& pair : monitor_threads_) {
        if (pair.second.joinable()) {
            pair.second.join();
        }
    }
    monitor_threads_.clear();
}

