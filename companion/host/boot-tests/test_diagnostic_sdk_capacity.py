#!/usr/bin/env python3
"""Exercise the actual boot USB adapter and export against IDF byte-buffer limits.

SDK doubles model pinned usb_serial_jtag.c xRingbufferSend of one whole item;
esp_ringbuf/ringbuf.c rejects items larger than its byte-buffer capacity.
No hardware, network or vendor code is executed.
"""
import pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
def main():
 source=(ROOT/'firmware/main/companion/boot.cc').read_text()
 driver=source[source.index('struct UsbSdkDriver {'):source.index('class UsbProvisioner {')]
 with tempfile.TemporaryDirectory(prefix='stackchan-export-sdk-capacity-') as directory:
  tmp=pathlib.Path(directory)
  code=r'''#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include "companion/diagnostic_usb_export.hpp"
constexpr int ESP_OK=0;
#define pdMS_TO_TICKS(n) (n)
struct usb_serial_jtag_driver_config_t {size_t rx_buffer_size,tx_buffer_size;};
static size_t tx_capacity=0;static bool installed=false;static unsigned writes=0,flushes=0,closes=0;static int core=0;
int xPortGetCoreID(){return core;}
bool usb_serial_jtag_is_driver_installed(){return installed;}
int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t* config){assert(!installed);assert(config->rx_buffer_size==1024);tx_capacity=config->tx_buffer_size;installed=true;return ESP_OK;}
int usb_serial_jtag_driver_uninstall(){++closes;installed=false;return ESP_OK;}
int usb_serial_jtag_read_bytes(uint8_t*,size_t,int){assert(false);return 0;}
int usb_serial_jtag_write_bytes(const uint8_t*,size_t n,int timeout){assert(installed&&timeout==100);++writes;return n<=tx_capacity?int(n):0;}
int usb_serial_jtag_wait_tx_done(int timeout){assert(installed&&timeout==100);++flushes;return ESP_OK;}
namespace companion {
''' + driver + r'''}
int main(){
 companion::UsbSdkDriver driver;
 assert(driver.Install());assert(tx_capacity==64);assert(driver.Uninstall());
 std::array<uint8_t,176> frame{};
 companion::DiagnosticUsbExport<companion::UsbSdkDriver> export_(driver);
 assert(export_.Export(true,frame.data(),frame.size()));
 assert(writes==1&&flushes==1&&closes==2&&!installed&&!export_.OwnsDriver());
 assert(tx_capacity>=frame.size()&&tx_capacity<=256);
 assert(driver.Install());assert(tx_capacity==64);assert(driver.Uninstall());
 assert(!export_.Export(false,frame.data(),frame.size()));assert(!export_.Export(true,frame.data(),257));
 core=1;assert(!export_.Export(true,frame.data(),frame.size()));assert(writes==1);
}
'''
  (tmp/'fixture.cpp').write_text(code)
  exe=tmp/'fixture'
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'firmware/main'),str(tmp/'fixture.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,timeout=10)
 print('PASS actual USB SDK adapter/export: complete176-byte frame fits bounded TX capacity; numeric owner remains64; no hardware/network')
if __name__=='__main__':main()
