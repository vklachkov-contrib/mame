// license:BSD-3-Clause
// copyright-holders:usernameak,vklachkov

#include "emu.h"
#include "grid210x.h"

#include "formats/flopimg.h"
#include "formats/pc_dsk.h"
#include "formats/imd_dsk.h"

#include <algorithm> // for std::min
#include <cinttypes> // for PRIu32


//**************************************************************************
//  GRID 2102 FLOPPY DEVICE IMPLEMENTATION
//**************************************************************************

DEFINE_DEVICE_TYPE(GRID2102, grid2102_device, "grid2102_floppy", "GRiD 2102 Floppy")

grid2102_device::grid2102_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: grid210x_device<floppy_image_device>(mconfig, GRID2102, tag, owner, clock, 5)
{
}

void grid2102_device::device_start()
{
	floppy_image_device::device_start();
	grid210x_device<floppy_image_device>::start();
}

void grid2102_device::device_stop()
{
	grid210x_device<floppy_image_device>::stop();
	floppy_image_device::device_stop();
}

void grid2102_device::setup_characteristics()
{
	m_form_factor = floppy_image::FF_525;
	m_tracks = 40;
	m_sides = 2;
	set_rpm(300);
	add_variant(floppy_image::DSDD);

	set_formats(grid2102_device::setup_formats);
}

void grid2102_device::setup_formats(format_registration &fr)
{
	fr.add(FLOPPY_PC_FORMAT);
	fr.add(FLOPPY_IMD_FORMAT);
}

bool grid2102_device::has_disk()
{
	return is_loaded();
}

grid210x_disk_geometry grid2102_device::get_geometry() const
{
	// For the GRiD Compass all diskettes were 360KB.
	// The geometry values were taken from the firmware:
	// https://github.com/vklachkov/grid-compass-fdd-2102/tree/main/firmware%20(300237-02).
	//
	// The values here correspond to the parameters in setup_characteristics().
	return grid210x_disk_geometry
	{
		/* sector_count        */ 720,
		/* sectors_per_track   */ 9,
		/* tracks_per_cylinder */ 2,
		/* interleave_factor   */ 5,
		/* second_side_count   */ 1,
		/* unused              */ 0,
	};
}

void grid2102_device::read_sector(uint32_t sector, uint8_t *buffer)
{
	osd_printf_verbose("%s io: read sector %d\n", tag(), sector);

	// todo
}

void grid2102_device::write_sector(uint32_t sector, const uint8_t *buffer)
{
	osd_printf_verbose("%s io: write sector %d\n", tag(), sector);

	// todo
}

void grid2102_device::format_disk()
{
	osd_printf_verbose("%s io: low level disk format\n", tag());

	std::vector<uint8_t> uninit_sector(512, 0xe5);
	std::fill_n(uninit_sector.begin(), 8, 0xff);

	const grid210x_disk_geometry geom = get_geometry();
	for (size_t i = 0; i < geom.sector_count; i++)
	{
		write_sector(i, uninit_sector.data());
	}
}

//**************************************************************************
//  GRID 2101 HDD DEVICE IMPLEMENTATION
//**************************************************************************

DEFINE_DEVICE_TYPE(GRID2101, grid2101_device, "grid2101_hdd", "GRiD 2101 HDD")

grid2101_device::grid2101_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: grid210x_device<harddisk_image_device>(mconfig, GRID2101, tag, owner, clock, 4)
{
}

void grid2101_device::device_start()
{
	harddisk_image_device::device_start();
	grid210x_device<harddisk_image_device>::start();
}

void grid2101_device::device_stop()
{
	grid210x_device<harddisk_image_device>::stop();
	harddisk_image_device::device_stop();
}

bool grid2101_device::has_disk()
{
	return is_loaded();
}

grid210x_disk_geometry grid2101_device::get_geometry() const
{
	const auto info = get_info();

	const auto clamp_uint16t = [](uint32_t value) {
		return static_cast<uint16_t>(std::min<uint32_t>(value, uint16_t(0xFFFF)));
	};

	return grid210x_disk_geometry
	{
		/* sector_count        */ clamp_uint16t(info.cylinders * info.heads * info.sectors),
		/* sectors_per_track   */ clamp_uint16t(info.sectors),
		/* tracks_per_cylinder */ clamp_uint16t(info.heads),
		/* interleave_factor   */ 0,
		/* second_side_count   */ 0,
		/* num_cylinders       */ clamp_uint16t(info.cylinders),
	};
}

void grid2101_device::read_sector(uint32_t sector, uint8_t *buffer)
{
	osd_printf_verbose("%s io: read sector %d\n", tag(), sector);
	read(sector, buffer);
}

void grid2101_device::write_sector(uint32_t sector, const uint8_t *buffer)
{
	osd_printf_verbose("%s io: write sector %d\n", tag(), sector);
	write(sector, buffer);
}

void grid2101_device::format_disk()
{
	osd_printf_verbose("%s io: low level disk format\n", tag());

	std::vector<uint8_t> uninit_sector(512, 0xe5);
	std::fill_n(uninit_sector.begin(), 8, 0xff);

	const grid210x_disk_geometry geom = get_geometry();
	for (size_t i = 0; i < geom.sector_count; i++)
	{
		write_sector(i, uninit_sector.data());
	}
}


//**************************************************************************
//  BASE DEVICE IMPLEMENTATION
//**************************************************************************

template <class BaseDevice>
grid210x_device<BaseDevice>::grid210x_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, uint8_t gpib_address)
	: BaseDevice(mconfig, type, tag, owner, clock)
	, device_ieee488_interface(mconfig, *this)
	, m_gpib_address(gpib_address)
	, m_talking(false)
	, m_listening(false)
	, m_serial_poll(false)
	, m_srq_raised(false)
{
	m_buffer.reserve(512);
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::start()
{
	m_emu = std::make_unique<grid210x_disk_emu>(
		this,
		grid210x_disk_io
		{
			[this]() { return is_floppy(); },
			[this]() { return has_disk(); },
			[this]() { return get_geometry(); },
			[this](uint32_t sector, uint8_t *buffer) { read_sector(sector, buffer); },
			[this](uint32_t sector, const uint8_t *data) { write_sector(sector, data); },
			[this]() { format_disk(); },
			[this]() { raise_srq(); }
		}
	);

	m_listener = std::make_unique<grid210x_gpib_listener>(this, m_bus, &m_thread_sync);
	m_talker = std::make_unique<grid210x_gpib_talker>(this, m_bus, &m_thread_sync);

	m_thread = std::thread([this]() { thread_entry(); });
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::stop()
{
	m_thread_sync.shutdown();
	m_thread.join();
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::thread_entry()
{
	try
	{
		listen_gpib_commands();
	}
	catch (grid210x_device_shutdown e)
	{
		osd_printf_verbose("%s: thread shutdown\n", this->tag());
	}
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::listen_gpib_commands()
{
	while (true)
	{
		grid210x_gpib_cmd cmd = m_listener->start_command_handshake();
		// cmd.debug_log();

		switch (cmd.type)
		{
		case grid210x_gpib_cmd::cmd_type::DCL:
			m_listener->end_handshake();
			m_emu->reset();
			break;

		case grid210x_gpib_cmd::cmd_type::SPE:
			m_serial_poll = true;
			if (m_srq_raised)
			{
				m_listener->end_handshake();
			}
			else
			{
				m_listener->unexpected_command();
			}
			break;

		case grid210x_gpib_cmd::cmd_type::SPD:
			if (m_srq_raised)
			{
				m_listener->end_handshake();
			}
			else
			{
				m_listener->unexpected_command();
			}
			m_serial_poll = false;
			m_srq_raised = false;
			break;

		case grid210x_gpib_cmd::cmd_type::MLA:
			if (cmd.addr == m_gpib_address)
			{
				m_listener->end_handshake();

				m_listening = true;
				listen_to_buffer();
			}
			else
			{
				m_listener->unexpected_command();
			}
			break;

		case grid210x_gpib_cmd::cmd_type::UNL:
			if (m_listening)
			{
				m_listener->end_handshake();

				m_listening = false;

				m_emu->process_buffer(m_buffer);
				m_buffer.clear();
			}
			else
			{
				m_listener->unexpected_command();
			}
			break;

		case grid210x_gpib_cmd::cmd_type::MTA:
			if (cmd.addr == m_gpib_address)
			{
				m_listener->end_handshake();

				m_talking = true;
				if (m_serial_poll)
					m_talker->send_serial_poll_response(m_srq_raised ? 0x4F : 0x0F);
				else
					m_emu->talk(m_talker);
			}
			else
			{
				m_listener->unexpected_command();
			}
			break;

		case grid210x_gpib_cmd::cmd_type::UNT:
			if (m_talking)
			{
				m_listener->end_handshake();
				m_talking = false;
			}
			else
			{
				m_listener->unexpected_command();
			}
			break;

		case grid210x_gpib_cmd::cmd_type::UNKNOWN:
			m_listener->unexpected_command();
			break;

		default:
			throw std::runtime_error("process_command: unhandled gpib command");
		}
	}
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::listen_to_buffer()
{
	while (true) {
		uint8_t byte = m_listener->handshake_byte();

		if (m_bus->atn_r() == 0) {
			m_emu->reset();
			m_buffer.clear();
			break;
		}

		m_buffer.push_back(byte);

		if (m_bus->eoi_r() == 0) {
			break;
		}
	}
}

template <class BaseDevice>
void grid210x_device<BaseDevice>::raise_srq()
{
	m_bus->srq_w(this, 0);
	m_srq_raised = true;
}

template class grid210x_device<harddisk_image_device>;
template class grid210x_device<floppy_image_device>;

//**************************************************************************
//  DISK EMULATOR IMPLEMENTATION
//**************************************************************************

grid210x_disk_req grid210x_disk_req::deserialize(const std::vector<uint8_t> &input)
{
	if (input.size() != REQUEST_LEN) {
		throw std::invalid_argument("Input buffer size is not equal to REQUEST_LEN");
	}

	grid210x_disk_req req;
	req.code = input[0];
	req.unused = input[1];
	req.connection = input[2];
	req.sector = (uint32_t)input[3] |
				((uint32_t)input[4] << 8) |
				((uint32_t)input[5] << 16) |
				((uint32_t)input[6] << 24);
	req.data_size = (uint16_t)input[7] | ((uint16_t)input[8] << 8);
	req.mode = input[9];
	return req;
}

void grid210x_disk_resp::serialize(std::vector<uint8_t> &output) const
{
	output.resize(RESPONSE_LEN);

	output[0] = status & 0xFF;
	output[1] = (status >> 8) & 0xFF;
	output[2] = unknown;
	output[3] = sector & 0xFF;
	output[4] = (sector >> 8) & 0xFF;
	output[5] = unused & 0xFF;
	output[6] = (unused >> 8) & 0xFF;
}

void grid210x_disk_status::serialize(std::vector<uint8_t> &output) const
{
	output.resize(DISK_STATUS_MAX_LEN);

	output[0] = sector_size & 0xFF;
	output[1] = (sector_size >> 8) & 0xFF;
	output[2] = logical_sector_size & 0xFF;
	output[3] = (logical_sector_size >> 8) & 0xFF;
	output[4] = sector_count & 0xFF;
	output[5] = (sector_count >> 8) & 0xFF;
	output[6] = drive_status;
	output[7] = bitmap_block_id & 0xFF;
	output[8] = (bitmap_block_id >> 8) & 0xFF;
	output[9] = superblock_id & 0xFF;
	output[10] = (superblock_id >> 8) & 0xFF;
	output[11] = min_dir_pages & 0xFF;
	output[12] = (min_dir_pages >> 8) & 0xFF;
	output[13] = flush;

	for (size_t i = 0; i < 32; ++i)
		output[14 + i] = device_name[i];

	output[46] = bytes_per_sector & 0xFF;
	output[47] = (bytes_per_sector >> 8) & 0xFF;
	output[48] = sectors_per_track & 0xFF;
	output[49] = (sectors_per_track >> 8) & 0xFF;
	output[50] = tracks_per_cylinder & 0xFF;
	output[51] = (tracks_per_cylinder >> 8) & 0xFF;
	output[52] = interleave_factor;
	output[53] = second_side_count;
	output[54] = num_cylinders & 0xFF;
	output[55] = (num_cylinders >> 8) & 0xFF;
}

grid210x_disk_emu::grid210x_disk_emu(device_t *dev, grid210x_disk_io disk_io, attotime io_delay)
{
	tag = dev->tag();
	m_disk_io = disk_io;
	m_io_delay = io_delay;
	m_io_delay_timer = dev->timer_alloc(FUNC(grid210x_disk_emu::process_io_request), this);
	m_buffer.reserve(512);
}

void grid210x_disk_emu::reset()
{
	m_current_req.reset();
	m_buffer.clear();	
}

void grid210x_disk_emu::process_buffer(const std::vector<uint8_t> &buffer)
{
	if (m_current_req.has_value())
	{
		const grid210x_disk_req req = m_current_req.value();
		if (req.code == REQ_WRITE)
		{
			m_buffer = buffer;
			m_io_delay_timer->adjust(m_io_delay);
			return;
		}
	}

	process_new_request(buffer);
}

void grid210x_disk_emu::process_new_request(const std::vector<uint8_t> &buffer)
{
	m_current_req.reset();

	if (buffer.size() != REQUEST_LEN) {
		// Do nothing, just return an error when we are asked to talk.
		osd_printf_verbose(
			"%s emu: received unusual %zu bytes request, expected %d bytes\n",
			tag, buffer.size(), REQUEST_LEN);
		return;
	}

	const grid210x_disk_req req = grid210x_disk_req::deserialize(buffer);

	switch (req.code)
	{
	case REQ_INITIALIZE:
		// do nothing, everything is already initialized.
		osd_printf_verbose("%s emu: received Initialize request\n", tag);
		break;
	case REQ_GET_STATUS:
		// do nothing, this command requires no action.
		osd_printf_verbose("%s emu: received GetStatus(size=%d) request\n", tag, req.data_size);
		break;
	case REQ_WRITE:
		// after this command, 512 more bytes are expected.
		osd_printf_verbose("%s emu: received Write(sector=%" PRIu32 ", mode=%d) request\n", tag, req.sector, req.mode);
		break;
	case REQ_READ:
		osd_printf_verbose("%s emu: received Read(sector=%" PRIu32 ") request\n", tag, req.sector);
		m_io_delay_timer->adjust(m_io_delay);
		break;
	case REQ_FORMAT:
		osd_printf_verbose("%s emu: received Format request\n", tag);
		m_io_delay_timer->adjust(m_io_delay);
		break;
	default:
		// do nothing, just return an error when we are asked to talk.
		osd_printf_verbose(
			"%s emu: received unsupported request %d with sector=%" PRIu32 ", data_size=%d, mode=%d\n",
			tag, req.code, req.sector, req.data_size, req.mode);
		break;
	}

	m_current_req = req;
}

TIMER_CALLBACK_MEMBER(grid210x_disk_emu::process_io_request)
{
	if (m_disk_io.has_disk())
	{
		process_disk_request();
	}
	else
	{
		osd_printf_verbose("%s emu: disk not inserted\n", tag);
		(grid210x_disk_resp { RESP_NOT_READY, 0, 0, 0 }).serialize(m_buffer);
	}

	m_disk_io.raise_srq();
}

void grid210x_disk_emu::process_disk_request()
{
	const grid210x_disk_req req = m_current_req.value();
	switch (req.code)
	{
	case REQ_READ:
	{
		const grid210x_disk_geometry geom = m_disk_io.get_geometry();
		if (req.sector >= geom.sector_count)
		{
			osd_printf_verbose(
				"%s emu: out of bounds sector %d read, total sectors is %d\n",
				tag, req.sector, geom.sector_count);

			(grid210x_disk_resp { RESP_OUT_OF_BOUNDS, 0, 0, 0 }).serialize(m_buffer);
			break;
		}

		// todo: handle read errors
		m_buffer.resize(512, 0);
		m_disk_io.read_sector(req.sector, m_buffer.data());

		break;
	}

	case REQ_WRITE:
	{
		if (m_buffer.size() != 512)
		{
			reset();

			(grid210x_disk_resp { RESP_UNSUPPORTED, 0, 0, 0 }).serialize(m_buffer);
			break;
		}

		if (req.mode == 1)
		{
			// validate the data that the laptop received from us in the previous request.
			// can be safely ignored.
			(grid210x_disk_resp { RESP_OK, 0, 0xFFFF, 0 }).serialize(m_buffer);
			break;
		}

		if (req.mode != 0)
		{
			(grid210x_disk_resp { RESP_UNSUPPORTED, 0, 0, 0 }).serialize(m_buffer);
			break;
		}

		const grid210x_disk_geometry geom = m_disk_io.get_geometry();
		if (req.sector >= geom.sector_count)
		{
			osd_printf_verbose(
				"%s emu: out of bounds write to sector %d, total sectors is %d\n",
				tag, req.sector, geom.sector_count);
			
			(grid210x_disk_resp { RESP_OUT_OF_BOUNDS, 0, 0, 0 }).serialize(m_buffer);
			break;
		}

		// todo: handle write errors
		m_disk_io.write_sector(req.sector, m_buffer.data());

		(grid210x_disk_resp { RESP_OK, 0, static_cast<uint16_t>(req.sector), 0 }).serialize(m_buffer);
		break;
	}

	case REQ_FORMAT:
	{
		// todo: handle format errors
		m_disk_io.format_disk();

		(grid210x_disk_resp { RESP_OK, 0, 0, 0 }).serialize(m_buffer);
		break;
	}

	default:
		throw std::runtime_error("process_disk_request: unexpected request code");
	}
}

void grid210x_disk_emu::talk(std::unique_ptr<grid210x_gpib_talker> &talker)
{
	if (m_current_req.has_value())
	{
		const grid210x_disk_req req = m_current_req.value();
		switch (req.code)
		{
		case REQ_INITIALIZE:
			(grid210x_disk_resp { RESP_OK, 0, 0, 0 }).serialize(m_buffer);
			break;
		case REQ_GET_STATUS:
			get_status(req.data_size);
			break;
		case REQ_READ:
		case REQ_WRITE:
		case REQ_FORMAT:
			// data already in buffer, prepared by grid210x_disk_emu::process_io_request.
			break;
		default:
			(grid210x_disk_resp { RESP_UNSUPPORTED, 0, 0, 0 }).serialize(m_buffer);
			break;
		}
	}
	else
	{
		(grid210x_disk_resp { RESP_UNSUPPORTED, 0, 0, 0 }).serialize(m_buffer);
	}

	if (m_buffer.size() == 0)
		throw std::runtime_error("validation error: no data to send to laptop");

	talker->send_bytes(m_buffer);

	reset();
}

void grid210x_disk_emu::get_status(uint16_t data_size)
{
	grid210x_disk_geometry geom = m_disk_io.get_geometry();

	grid210x_disk_status status{};
	status.sector_size = 512;
	status.logical_sector_size = 504;
	status.sector_count = geom.sector_count;
	status.drive_status = 1;
	status.bitmap_block_id = 0x120;
	status.superblock_id = 0x121;
	status.min_dir_pages = 1;
	status.flush = 0;
	std::fill(std::begin(status.device_name), std::end(status.device_name), ' ');
	status.bytes_per_sector = 512;
	status.sectors_per_track = geom.sectors_per_track;
	status.tracks_per_cylinder = geom.tracks_per_cylinder;
	status.interleave_factor = geom.interleave_factor;
	status.second_side_count = geom.second_side_count;
	status.num_cylinders = geom.num_cylinders;

	status.serialize(m_buffer);

	if (m_disk_io.is_floppy())
		m_buffer.resize(52);
	else
		m_buffer.resize(data_size == 54 ? 52 : data_size);
}


//**************************************************************************
//  GPIB IMPLEMENTATION
//**************************************************************************

grid210x_gpib_cmd grid210x_gpib_cmd::from_u8(uint8_t value)
{
	if (value == 0b0001'0100)
		return grid210x_gpib_cmd{ value, cmd_type::DCL, 0 };
	else if (value == 0b0001'1000)
		return grid210x_gpib_cmd{ value, cmd_type::SPE, 0 };
	else if (value == 0b0001'1001)
		return grid210x_gpib_cmd{ value, cmd_type::SPD, 0 };
	else if (value == 0b0011'1111)
		return grid210x_gpib_cmd{ value, cmd_type::UNL, 0 };
	else if (value == 0b0101'1111)
		return grid210x_gpib_cmd{ value, cmd_type::UNT, 0 };
	else if ((value & 0b0110'0000) == 0b0010'0000)
		return grid210x_gpib_cmd{ value, cmd_type::MLA, uint8_t(value & 0b0001'1111) };
	else if ((value & 0b0110'0000) == 0b0100'0000)
		return grid210x_gpib_cmd{ value, cmd_type::MTA, uint8_t(value & 0b0001'1111) };
	else
		return grid210x_gpib_cmd{ value, cmd_type::UNKNOWN, 0 };
}

void grid210x_gpib_cmd::debug_log() const
{
	switch (type)
	{
	case cmd_type::DCL:
		osd_printf_verbose("gpib: DCL\n");
		break;
	case cmd_type::SPE:
		osd_printf_verbose("gpib: SPE\n");
		break;
	case cmd_type::SPD:
		osd_printf_verbose("gpib: SPD\n");
		break;
	case cmd_type::UNL:
		osd_printf_verbose("gpib: UNL\n");
		break;
	case cmd_type::UNT:
		osd_printf_verbose("gpib: UNT\n");
		break;
	case cmd_type::MLA:
		osd_printf_verbose("gpib: MLA(%d)\n", addr);
		break;
	case cmd_type::MTA:
		osd_printf_verbose("gpib: MTA(%d)\n", addr);
		break;
	case cmd_type::UNKNOWN:
		osd_printf_verbose("gpib: unknown cmd 0x%x\n", raw);
		break;
	}
}

void grid210x_thread_sync::shutdown()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_state = state::shutdown;
	m_cv.notify_all();
}

void grid210x_thread_sync::sync_with_thread()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	wait_state(lock, state::waiting);

	m_state = state::changed;
	m_cv.notify_all();

	wait_state(lock, state::ready);

	m_state = state::process;
	m_cv.notify_all();

	wait_state(lock, state::waiting);
}

void grid210x_thread_sync::wait_main_thread()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_state = state::waiting;
	m_cv.notify_all();

	wait_state(lock, state::changed);

	m_state = state::ready;
	m_cv.notify_all();

	wait_state(lock, state::process);
}

void grid210x_thread_sync::wait_state(std::unique_lock<std::mutex> &lock, state s)
{
	m_cv.wait(lock, [this, s]() {
		if (m_state == state::shutdown)
		{
			throw grid210x_device_shutdown();
		}
		else
		{
			return m_state == s;
		}
	});
}

grid210x_gpib_listener::grid210x_gpib_listener(device_t* dev, ieee488_device *bus, grid210x_thread_sync *sync)
{
	m_dev = dev;
	m_bus = bus;
	m_thread_sync = sync;
}

grid210x_gpib_cmd grid210x_gpib_listener::start_command_handshake()
{
	while (true)
	{
		if (m_bus->atn_r() == 1)
		{
			m_bus->ndac_w(m_dev, 1);
			m_thread_sync->wait_main_thread();
			continue;
		}

		m_bus->ndac_w(m_dev, 0);

		if (m_bus->dav_r() == 1)
		{
			m_thread_sync->wait_main_thread();
			continue;
		}

		m_bus->nrfd_w(m_dev, 0);

		const uint8_t byte = ~m_bus->dio_r();
		return grid210x_gpib_cmd::from_u8(byte);
	}
}

void grid210x_gpib_listener::end_handshake()
{
	m_bus->nrfd_w(m_dev, 0);
	m_bus->ndac_w(m_dev, 1);

	while (m_bus->dav_r() == 0)
		m_thread_sync->wait_main_thread();

	m_bus->ndac_w(m_dev, 0);
	m_bus->nrfd_w(m_dev, 1);
}

void grid210x_gpib_listener::unexpected_command()
{
	m_bus->ndac_w(m_dev, 1);

	while (m_bus->dav_r() == 0)
		m_thread_sync->wait_main_thread();

	m_bus->nrfd_w(m_dev, 1);
}

uint8_t grid210x_gpib_listener::handshake_byte()
{
	while (m_bus->dav_r() == 1)
		m_thread_sync->wait_main_thread();

	const uint8_t byte = ~m_bus->dio_r();

	end_handshake();

	return byte;
}

grid210x_gpib_talker::grid210x_gpib_talker(device_t* dev, ieee488_device *bus, grid210x_thread_sync *sync)
{
	m_dev = dev;
	m_bus = bus;
	m_thread_sync = sync;
}

void grid210x_gpib_talker::send_bytes(std::vector<uint8_t> &bytes)
{
	setup_bus();
	for (size_t i = 0; i < bytes.size(); i++)
		send_byte(bytes[i], (i == bytes.size() - 1));
}

void grid210x_gpib_talker::send_serial_poll_response(uint8_t byte)
{
	setup_bus();
	send_byte(byte, false);
}

void grid210x_gpib_talker::setup_bus()
{
	m_bus->atn_w(m_dev, 1);
	m_bus->eoi_w(m_dev, 1);
	m_bus->dav_w(m_dev, 1);
	m_bus->nrfd_w(m_dev, 1);
	m_bus->ndac_w(m_dev, 1);
	m_bus->ifc_w(m_dev, 1);
	m_bus->srq_w(m_dev, 1);
	m_bus->ren_w(m_dev, 1);
}

void grid210x_gpib_talker::send_byte(uint8_t byte, bool eoi)
{
	while (!(m_bus->ndac_r() == 0 && m_bus->nrfd_r() == 1))
		m_thread_sync->wait_main_thread();

	if (eoi) m_bus->eoi_w(m_dev, 0);
	m_bus->dio_w(m_dev, ~byte);

	m_bus->dav_w(m_dev, 0);

	while (m_bus->nrfd_r() == 0)
		m_thread_sync->wait_main_thread();

	while (m_bus->ndac_r() == 0)
		m_thread_sync->wait_main_thread();

	m_bus->dav_w(m_dev, 1);

	if (eoi) m_bus->eoi_w(m_dev, 1);
	m_bus->dio_w(m_dev, 0xff);
}
