// license:BSD-3-Clause
// copyright-holders:vklachkov
/*********************************************************************

    grid2102.h

    GRiD 2102/2107 Portable Diskette Drive

*********************************************************************/

#ifndef MAME_BUS_IEEE488_GRID2102_H
#define MAME_BUS_IEEE488_GRID2102_H

#pragma once

#include "ieee488.h"
#include "cpu/mcs51/i8051.h"
#include "machine/wd_fdc.h"
#include "imagedev/floppy.h"

class grid2102_device : public device_t,
						public device_ieee488_interface
{
public:
	grid2102_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// device-level overrides
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	// device_ieee488_interface overrides
	virtual void ieee488_eoi(int state) override;
	virtual void ieee488_dav(int state) override;
	virtual void ieee488_nrfd(int state) override;
	virtual void ieee488_ndac(int state) override;
	virtual void ieee488_ifc(int state) override;
	virtual void ieee488_srq(int state) override;
	virtual void ieee488_atn(int state) override;
	virtual void ieee488_ren(int state) override;

private:
	void prg_map(address_map &map) ATTR_COLD;
	void data_map(address_map &map) ATTR_COLD;

	uint8_t gpib_addr_r(offs_t offset);

	uint8_t cpu_p1_read(offs_t offset);
	void cpu_p1_write(uint8_t data);

	uint8_t cpu_p3_read(offs_t offset);
	void cpu_p3_write(uint8_t data);

	required_device<i8051_device> m_cpu;
	required_device<wd2797_device> m_fdc;
	required_device<floppy_connector> m_floppy;

	required_ioport m_gpib_addr;
};

// device type definition
DECLARE_DEVICE_TYPE(GRID2102, grid2102_device)

#endif // MAME_BUS_IEEE488_GRID2102_H
