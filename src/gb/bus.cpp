#include <bemu/gb/bus.hpp>
#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/cpu.hpp>

using namespace bemu;
using namespace bemu::gb;

Bus::Bus(ICycler &cycler, Cpu &cpu, Cartridge &cartridge, External &external)
    : MemoryBus(&cycler),
      m_joypad(external, cpu),
      m_ppu(external, *this, m_lcd, cpu),
      m_timer(cpu),
      m_serial(external) {
    add_region(cpu);
    add_region(cartridge);
    add_region(m_wram_fixed);
    add_region(m_wram);
    add_region(m_hram);
    add_region(m_ppu);
    add_region(m_audio);
    add_region(m_wave_pattern);
    add_region(m_serial);
    add_region(m_timer);
    add_region(m_joypad);
    add_region(m_lcd);
    add_region(m_reserved_echo);
    add_region(m_reserved_unused);
}
