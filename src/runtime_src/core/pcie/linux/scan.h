/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Author: Hem Neema, Ryan Radjabi
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#ifndef _XCL_SCAN_H_
#define _XCL_SCAN_H_

#include <string>
#include <vector>
#include <memory>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mutex>

#define INVALID_ID      0xffff
#define MFG_REV_OFFSET  0x131008 // For obtaining Golden image version number

#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9
#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

enum p2p_config {
    P2P_CONFIG_DISABLED,
    P2P_CONFIG_ENABLED,
    P2P_CONFIG_REBOOT,
    P2P_CONFIG_NOT_SUPP,
    P2P_CONFIG_ERROR,
};

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};


namespace pcidev {

// One PCIE function on FPGA board
class pci_device
{
public:
  // Fundamental and static information for this device are defined as class
  // members and initialized during object construction.
  //
  // The rest of information related to the device shall be obtained
  // dynamically via sysfs APIs below.

  uint16_t domain =           INVALID_ID;
  uint16_t bus =              INVALID_ID;
  uint16_t dev =              INVALID_ID;
  uint16_t func =             INVALID_ID;
  uint32_t instance =         INVALID_ID;
  std::string sysfs_name =    ""; // dir name under /sys/bus/pci/devices
  int user_bar =              0;  // BAR mapped in by tools, default is BAR0
  size_t user_bar_size =      0;
  bool is_mgmt =              false;
  bool is_ready =             false;

  pci_device(const std::string& sysfs_name);
  ~pci_device();

  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::vector<std::string>& sv);
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::vector<uint64_t>& iv);
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::string& s);
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::vector<char>& buf);
  template <typename T>
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, T& i, const T& default_val)
  {
    std::vector<uint64_t> iv;
    sysfs_get(subdev, entry, err, iv);
    if (!iv.empty())
      i = static_cast<T>(iv[0]);
    else
      i = static_cast<T>(default_val); // default value
  }

  void
  sysfs_get_sensor(const std::string& subdev, const std::string& entry, uint32_t& i)
  {
    std::string err;
    sysfs_get<uint32_t>(subdev, entry, err, i, 0);
  }

  void
  sysfs_put(const std::string& subdev, const std::string& entry,
            std::string& err, const std::string& input);
  void
  sysfs_put(const std::string& subdev, const std::string& entry,
            std::string& err, const std::vector<char>& buf);

  void
  sysfs_put(const std::string& subdev, const std::string& entry,
            std::string& err, const unsigned int& buf);

  std::string
  get_sysfs_path(const std::string& subdev, const std::string& entry);

  std::string
  get_subdev_path(const std::string& subdev, uint32_t idx);

  int pcieBarRead(uint64_t offset, void *buf, uint64_t len);
  int pcieBarWrite(uint64_t offset, const void *buf, uint64_t len);

  int open(const std::string& subdev, int flag);
  int open(const std::string& subdev, uint32_t idx, int flag);
  void close(int devhdl);
  int ioctl(int devhdl, unsigned long cmd, void *arg = nullptr);
  int poll(int devhdl, short events, int timeoutMilliSec);
  void *mmap(int devhdl, size_t len, int prot, int flags, off_t offset);
  int munmap(int devhdl, void* addr, size_t len);
  int flock(int devhdl, int op);
  int get_partinfo(std::vector<std::string>& info, void *blob = nullptr);
  std::shared_ptr<pcidev::pci_device> lookup_peer_dev();

private:
  int map_usr_bar(void);

  std::mutex lock;
  char *user_bar_map = reinterpret_cast<char *>(MAP_FAILED);
};

void rescan(void);
size_t get_dev_total(bool user = true);
size_t get_dev_ready(bool user = true);
std::shared_ptr<pci_device> get_dev(unsigned index, bool user = true);

int get_axlf_section(const std::string& filename, int kind, std::shared_ptr<char>& buf);
int get_uuids(std::shared_ptr<char>& dtbbuf, std::vector<std::string>& uuids);
std::shared_ptr<pcidev::pci_device> lookup_user_dev(std::shared_ptr<pcidev::pci_device> mgmt_dev);
int shutdown(std::shared_ptr<pcidev::pci_device> mgmt_dev, bool remove_user = false, bool remove_mgmt = false);
int check_p2p_config(const std::shared_ptr<pcidev::pci_device>& dev, std::string &err);

} // namespace pcidev

// For print out per device info
std::ostream&
operator<< (std::ostream& stream, const std::shared_ptr<pcidev::pci_device>& dev);

#endif
