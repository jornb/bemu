#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/emulator.hpp>
#include <filesystem>
#include <iostream>

using namespace bemu;
using namespace bemu::gb;

namespace {

bool test_is_done(const Emulator &emulator) { return emulator.m_external->m_serial_data_received.size() == 6; }

bool check_test_success(const Emulator &emulator) {
    return emulator.m_external->m_serial_data_received == std::vector<u8>{3, 5, 8, 13, 21, 34};
}

bool run_test(const std::string &rom_path) {
    auto logger = spdlog::get("test");
    logger->debug("[{:<75}] Running", rom_path);

    try {
        auto cartridge = Cartridge::from_file(rom_path);
        Emulator emulator{std::move(cartridge)};

        auto result = emulator.run_until(test_is_done);
        result &= check_test_success(emulator);
        if (result) {
            logger->info("[{:<75}] Pass", rom_path);
        } else {
            auto serial_out = emulator.m_external->m_serial_data_received;
            logger->warn("[{:<75}] Fail");
        }
        return result;
    } catch (const std::exception &ex) {
        logger->error("[{:<75}] Exception: ", rom_path, ex.what());
        return false;
    } catch (...) {
        logger->error("[{:<75}] Unknown exception", rom_path);
        return false;
    }
}

bool run_tests(const std::filesystem::path &dir) {
    bool result = true;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gb") {
            result &= run_test(entry.path().string());
        } else if (entry.is_directory()) {
            result &= run_tests(entry.path());
        }
    }
    return result;
}
}  // namespace

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./emulator <rom|dir>" << std::endl;
        return -1;
    }

    auto log = spdlog::stdout_color_mt("test");

    bool result = true;

    auto arg = std::filesystem::path(argv[1]);
    if (std::filesystem::is_directory(arg)) {
        result = run_tests(arg);
    } else {
        result = run_test(arg.string());
    }

    return result ? 0 : 1;
}
