// license:BSD-3-Clause
// copyright-holders:vklachkov
/*********************************************************************

    grid2102.cpp

    GRiD 2102/2107 Portable Diskette Drive

*********************************************************************/

#include "emu.h"
#include "grid2102.h"

DEFINE_DEVICE_TYPE(GRID2102, grid2102_device, "grid2102", "GRiD 2102 Portable Diskette Drive")

grid2102_device::grid2102_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, GRID2102, tag, owner, clock),
	  device_ieee488_interface(mconfig, *this),
	  m_cpu(*this, "cpu"),
	  m_fdc(*this, "fdc"),
	  m_floppy(*this, "floppy"),
	  m_gpib_addr(*this, "ADDRESS"),
	  m_send_gpib(false)
{
}

static INPUT_PORTS_START(grid2102_port)
	PORT_START("ADDRESS")
	PORT_DIPNAME(0x1f, 6, "GPIB address")
	PORT_DIPSETTING(0,  "0");
	PORT_DIPSETTING(1,  "1");
	PORT_DIPSETTING(2,  "2");
	PORT_DIPSETTING(3,  "3");
	PORT_DIPSETTING(4,  "4");
	PORT_DIPSETTING(5,  "5");
	PORT_DIPSETTING(6,  "6");
	PORT_DIPSETTING(7,  "7");
	PORT_DIPSETTING(8,  "8");
	PORT_DIPSETTING(9,  "9");
	PORT_DIPSETTING(10, "10");
	PORT_DIPSETTING(11, "11");
	PORT_DIPSETTING(12, "12");
	PORT_DIPSETTING(13, "13");
	PORT_DIPSETTING(14, "14");
	PORT_DIPSETTING(15, "15");
	PORT_DIPSETTING(16, "16");
	PORT_DIPSETTING(17, "17");
	PORT_DIPSETTING(18, "18");
	PORT_DIPSETTING(19, "19");
	PORT_DIPSETTING(20, "20");
	PORT_DIPSETTING(21, "21");
	PORT_DIPSETTING(22, "22");
	PORT_DIPSETTING(23, "23");
	PORT_DIPSETTING(24, "24");
	PORT_DIPSETTING(25, "25");
	PORT_DIPSETTING(26, "26");
	PORT_DIPSETTING(27, "27");
	PORT_DIPSETTING(28, "28");
	PORT_DIPSETTING(29, "29");
	PORT_DIPSETTING(30, "30");
	PORT_DIPSETTING(31, "31");
INPUT_PORTS_END

ioport_constructor grid2102_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(grid2102_port);
}

static void grid2102_floppies(device_slot_interface &device)
{
	device.option_add("525dd", FLOPPY_525_DD);
}

void grid2102_device::device_add_mconfig(machine_config &config)
{
	I8051(config, m_cpu, 12_MHz_XTAL);
	m_cpu->set_addrmap(AS_PROGRAM, &grid2102_device::prg_map);
	m_cpu->set_addrmap(AS_DATA, &grid2102_device::data_map);
	m_cpu->port_in_cb<1>().set(FUNC(grid2102_device::cpu_p1_read));
	m_cpu->port_out_cb<1>().set(FUNC(grid2102_device::cpu_p1_write));
	m_cpu->port_in_cb<3>().set(FUNC(grid2102_device::cpu_p3_read));
	m_cpu->port_out_cb<3>().set(FUNC(grid2102_device::cpu_p3_write));

	FLOPPY_CONNECTOR(config, m_floppy, grid2102_floppies, "525dd", floppy_image_device::default_pc_floppy_formats, true).enable_sound(true);

	WD2797(config, m_fdc, 1_MHz_XTAL);
	m_fdc->intrq_wr_callback().set(FUNC(grid2102_device::cpu_irq));
	m_fdc->set_floppy(m_floppy->get_device());
}

void grid2102_device::device_start()
{
	save_item(NAME(m_send_gpib));
}

void grid2102_device::device_reset()
{
	m_send_gpib = false;
}

void grid2102_device::ieee488_eoi(int state)
{
	logerror("ieee488_eoi called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_dav(int state)
{
	logerror("ieee488_dav called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_nrfd(int state)
{
	logerror("ieee488_nrfd called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_ndac(int state)
{
	logerror("ieee488_ndac called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_ifc(int state)
{
	logerror("ieee488_ifc called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_srq(int state)
{
	logerror("ieee488_srq called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_atn(int state)
{
	logerror("ieee488_atn called with state: %d\n", state);
	// TODO
}

void grid2102_device::ieee488_ren(int state)
{
	logerror("ieee488_ren called with state: %d\n", state);
	// TODO
}

ROM_START(grid2102)
	ROM_REGION(0x1000, "cpu", 0)
	ROM_LOAD("300237-02.bin", 0x0000, 0x1000, CRC(85850e38) SHA1(19493c9bff63cdcc24762c21a876c67eb5d4d825))
ROM_END

const tiny_rom_entry *grid2102_device::device_rom_region() const
{
	return ROM_NAME(grid2102);
}

void grid2102_device::prg_map(address_map &map)
{
	map(0x0000, 0x0fff).rom().region("cpu", 0);
}

void grid2102_device::data_map(address_map &map)
{
	map(0x1000, 0x1fff).ram();
	map(0x2000, 0x2fff).rw(m_fdc, FUNC(wd2797_device::read), FUNC(wd2797_device::write));
	map(0x3000, 0x3fff).r(FUNC(grid2102_device::gpib_addr_r));
	map(0x4000, 0x4fff).rw(FUNC(grid2102_device::gpib_read), FUNC(grid2102_device::gpib_write));
}

void grid2102_device::cpu_irq(int state)
{
	// m_cpu->set_input_line(MCS51_INT1_LINE, state == 0 ? ASSERT_LINE : CLEAR_LINE);
}

uint8_t grid2102_device::gpib_addr_r(offs_t offset)
{
	logerror("read gpib address = %d\n", m_gpib_addr->read());
	return m_gpib_addr->read();
}

uint8_t grid2102_device::gpib_read(offs_t offset)
{
	uint8_t val = m_bus->dio_r();
	logerror("read 0x%02X from gpib bus\n", val ^ 0xff);
	return val;
}

void grid2102_device::gpib_write(uint8_t data)
{
	logerror("gpib_write()!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	m_bus->dio_w(this, data);
}

uint8_t grid2102_device::cpu_p1_read(offs_t offset)
{
	uint8_t val = 0
		| m_bus->ifc_r() << 0
		| m_fdc->drq_r() << 1
		| m_bus->atn_r() << 2
		| m_bus->srq_r() << 3
		| m_bus->dav_r() << 4
		| m_bus->nrfd_r() << 5
		| m_bus->ndac_r() << 6
		| m_bus->eoi_r() << 7;

	// logerror("cpu_p1_read: IFC=%d DRQ=%d ATN=%d SRQ=%d DAV=%d NRFD=%d NDAC=%d EOI=%d\n",
	// 	(val >> 0) & 1,
	// 	(val >> 1) & 1,
	// 	(val >> 2) & 1,
	// 	(val >> 3) & 1,
	// 	(val >> 4) & 1,
	// 	(val >> 5) & 1,
	// 	(val >> 6) & 1,
	// 	(val >> 7) & 1);

	return val;
}

void grid2102_device::cpu_p1_write(uint8_t data)
{
	if (m_send_gpib)
	{
		logerror("cpu_p1_write(send_gpib): ATN=%d DAV=%d EOI=%d\n",
			BIT(data, 2),
			BIT(data, 4),
			BIT(data, 7));

		m_bus->atn_w(this, BIT(data, 2));
		m_bus->dav_w(this, BIT(data, 4));
		m_bus->eoi_w(this, BIT(data, 7));
	}
	else
	{
		logerror("cpu_p1_write(not send_gpib): SRQ=%d NRFD=%d NDAC=%d\n",
			BIT(data, 3),
			BIT(data, 5),
			BIT(data, 6));

		m_bus->srq_w(this, BIT(data, 3));
		m_bus->nrfd_w(this, BIT(data, 5));
		m_bus->ndac_w(this, BIT(data, 6));
	}
}

uint8_t grid2102_device::cpu_p3_read(offs_t offset)
{
	uint8_t val = 0
		| m_fdc->enp_r() << 1
		;

	logerror("cpu_p1_read: ~MOT_B_EN=%d ENP=%d INT_PWR=%d IRQ_MCU=%d EXT_PWR=%d GPIB_SEND=%d WR=%d RD=%d\n",
		(val >> 0) & 1,
		(val >> 1) & 1,
		(val >> 2) & 1,
		(val >> 3) & 1,
		(val >> 4) & 1,
		(val >> 5) & 1,
		(val >> 6) & 1,
		(val >> 7) & 1);

	return val;
}

void grid2102_device::cpu_p3_write(uint8_t data)
{
	logerror("cpu_p3_write: MOT_B_EN=%d PRECOM=%d INT_PWR=%d IRQ_MCU=%d EXT_PWR=%d ~{SendGPiB}=%d ~{WR}=%d ~{RD}=%d (raw=%02x)\n",
		BIT(data,0),  // 0 - MOT_B_EN
		BIT(data,1),  // 1 - PRECOM
		BIT(data,2),  // 2 - INT_PWR
		BIT(data,3),  // 3 - IRQ_MCU
		BIT(data,4),  // 4 - EXT_PWR
		BIT(data,5),  // 5 - ~{SendGPiB}
		BIT(data,6),  // 6 - ~{WR}
		BIT(data,7),  // 7 - ~{RD}
		data);

	m_floppy->get_device()->mon_w(0);
	m_send_gpib = BIT(data, 5) == 0;

	// ~MOT_B_EN
	// wd2797 enp
	// INT_PWR
	// IRQ_MCU
	// TODO: EXT_PWR
	// ignore Buffers direction
	// TODO: WR
	// TODO: RD
}
