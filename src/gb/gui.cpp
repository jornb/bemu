#include <spdlog/spdlog.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <cstdio>
#include <magic_enum.hpp>
#include "gui/screen.hpp"

using namespace bemu;
using namespace bemu::gb;


int main(int argc, char **argv) {
    if (argc != 2) {
        spdlog::critical("Usage: ./main <rom>");
        return -1;
    }
    try {
        spdlog::info("Loading ROM {}", argv[1]);
        auto cartridge = Cartridge::from_file(argv[1]);
        const auto &header = cartridge.header();

        // spdlog::info("\t", magic_enum::enum_name<RegisterType::A>());

        spdlog::info("\tTitle          : {}", header.get_title());
        spdlog::info("\tCartridge type : {}", magic_enum::enum_name(header.cartridge_type));
        spdlog::info("\tRAM size       : {}", magic_enum::enum_name(header.ram_size));
        spdlog::info("\tROM size       : {}", magic_enum::enum_name(header.rom_size));
        spdlog::info("\tEntry          : {:02x} {:02x} {:02x} {:02x}", header.entry[0], header.entry[1],
                     header.entry[2], header.entry[3]);

        Emulator emulator{cartridge};

        QGuiApplication app(argc, argv);
        QQmlApplicationEngine engine;
        qmlRegisterType<::Screen>("bemu.gb", 1, 0, "Screen");
        engine.load(QUrl(QStringLiteral("src/gb/main.qml")));

        qDebug() << "Help!";
        return app.exec();
        // emulator.run();
    } catch (const std::exception &e) {
        spdlog::critical(e.what());
        return -1;
    } catch (...) {
        spdlog::critical("Unknown error");
        return -1;
    }

    return 0;
}
