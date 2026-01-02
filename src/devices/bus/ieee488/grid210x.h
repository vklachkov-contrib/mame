// license:BSD-3-Clause
// copyright-holders:usernameak,vklachkov

/**********************************************************************

    GRiD 210X GPIB Disk Emulation

**********************************************************************/

#ifndef MAME_BUS_IEEE488_GRID210X_H
#define MAME_BUS_IEEE488_GRID210X_H

#pragma once

#include "ieee488.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>


class grid210x_device;


//**************************************************************************
//  GPIB DEFINITIONS
//**************************************************************************

struct grid210x_gpib_cmd
{
	enum class cmd_type : uint8_t
	{
		DCL,
		SPE,
		SPD,
		MLA,
		UNL,
		MTA,
		UNT,
		UNKNOWN = 0xFF,
	};

	uint8_t raw;
	cmd_type type;
	uint8_t addr;

	static grid210x_gpib_cmd from_u8(uint8_t value);
	void debug_log() const;
};

class grid210x_gpib_sync
{
public:
	void notify_and_wait();
	void wait_changes();

private:
	enum class state
	{
		init,
		waiting,
		changed,
		ready,
		process,
	};

	std::mutex m_mutex;
	std::condition_variable m_cv;
	state m_state = state::init;
};

class grid210x_gpib_listener
{
public:
	grid210x_gpib_listener(grid210x_device *dev, ieee488_device *bus, grid210x_gpib_sync *sync);

	grid210x_gpib_cmd start_command_handshake();
	void end_handshake();
	void unexpected_command();
	uint8_t handshake_byte();

private:
	grid210x_device *m_dev;
	ieee488_device *m_bus;
	grid210x_gpib_sync *m_sync;
};

class grid210x_gpib_talker
{
public:
	grid210x_gpib_talker(grid210x_device *dev, ieee488_device *bus, grid210x_gpib_sync *sync);

	void send_bytes(std::vector<uint8_t> &bytes);
	void send_serial_poll_response(uint8_t byte);

private:
	void setup_bus();
	void send_byte(uint8_t byte, bool eoi);

	grid210x_device *m_dev;
	ieee488_device *m_bus;
	grid210x_gpib_sync *m_sync;
};


//**************************************************************************
//  DISK EMULATOR DEFINITIONS
//**************************************************************************

const size_t REQUEST_LEN = 10;

enum grid210x_disk_req_code : uint8_t
{
	REQ_INITIALIZE   = 0,
	REQ_GET_STATUS   = 1,
	REQ_READ         = 4,
	REQ_WRITE        = 5,
	REQ_FORMAT       = 17
};

struct grid210x_disk_req
{
	// Operation code. Determines what magic the emulator will do next.
	uint8_t code;

	// The exact purpose is unknown.
	uint8_t unused;

	// The exact purpose is unknown.
	uint8_t connection;

	// Sector number. Only used for Read, Write, and TrackFormat operations.
	uint32_t sector;

	// Request data size.
	// For Format, it must be 1.
	// For GetStatus, it can be 52, 54, or 56.
	// For Read and Write, it should always be 512.
	uint16_t data_size;

	// Determines what action the command will do.
	// For example, Write with mode=1 is a verification of the received data.
	// Or, for SelfTest, mode=7 turns the drive on, mode=8 turns the drive power off.
	uint8_t mode;

	static grid210x_disk_req deserialize(const std::vector<uint8_t> &input);
};

const size_t RESPONSE_LEN = 7;

enum grid210x_disk_resp_status : uint16_t
{
	RESP_OK            = 0x00,
	RESP_UNSUPPORTED   = 0x23,
	RESP_NOT_READY     = 0x6b,
	RESP_OUT_OF_BOUNDS = 0x66,
	RESP_BAD_SECTOR    = 0x67,
	RESP_NOT_FORMATTED = 0x68
};

struct grid210x_disk_resp
{
	// Status code of the request.
	uint16_t status;

	// Exact purpose is unknown, maybe connection or drive init flag, on real drive always 0.
	uint8_t unknown;

	// Sector number from the request, if needed.
	uint16_t sector;

	// Unused, always 0.
	uint16_t unused;

	void serialize(std::vector<uint8_t> &output) const;
};

const size_t DISK_STATUS_MAX_LEN = 56;

struct grid210x_disk_status
{
	// Actual sector size. Always 512 bytes.
	uint16_t sector_size;

	// Number of bytes in a sector that can be used for data. Always 504 bytes.
	uint16_t logical_sector_size;

	// Number of sectors.
	// Must match real value, because CCOS checks
	// disk boundaries when working with it.
	uint16_t sector_count;

	// Status of the drive. 0 is not ready, 1 is ready, 3 is error.
	uint8_t drive_status;

	// Bitmap block number. Always 0x120 (one less than the superblock).
	// Used only in CCOS.
	uint16_t bitmap_block_id;

	// Superblock number. Always 0x121.
	// Used only in CCOS.
	uint16_t superblock_id;

	// Unknown purpose. On 2101 always 1.
	uint16_t min_dir_pages;

	// Unknown purpose. On 2101 always 0.
	uint8_t flush;

	// Device name. Not shown in the CCOS interface, can be anything.
	uint8_t device_name[32];

	// Same as sector_size.
	uint16_t bytes_per_sector;

	// Unknown purpose. Can be 0.
	uint16_t sectors_per_track;

	// Unknown purpose. Can be 0.
	uint16_t tracks_per_cylinder;

	// Unknown purpose. Can be 0.
	uint8_t interleave_factor;

	// Unknown purpose. Can be 0.
	uint8_t second_side_count;

	// Unknown purpose. Can be 0.
	uint16_t num_cylinders;

	void serialize(std::vector<uint8_t> &output) const;
};

struct grid210x_disk_geometry
{
	uint16_t sector_count;
	uint16_t sectors_per_track;
	uint16_t tracks_per_cylinder;
	uint8_t  interleave_factor;
	uint8_t  second_side_count;
	uint16_t num_cylinders;
};

struct grid210x_disk_io
{
	std::function<bool()> is_floppy;
	std::function<bool()> has_disk;
	std::function<grid210x_disk_geometry()> get_geometry;
	std::function<void(uint32_t sector, uint8_t *buffer)> read_sector;
	std::function<void(uint32_t sector, const uint8_t *buffer)> write_sector;
	std::function<void()> format_disk;
	std::function<void()> raise_srq;
};

class grid210x_disk_emu
{
public:
	grid210x_disk_emu(grid210x_device *dev, grid210x_disk_io disk_io, attotime io_delay = attotime::from_msec(5));

	void reset();
	void process_buffer(const std::vector<uint8_t> &buffer);
	void talk(std::unique_ptr<grid210x_gpib_talker> &talker);

private:
	const char* tag;

	grid210x_disk_io m_disk_io;

	std::optional<grid210x_disk_req> m_current_req;
	std::vector<uint8_t> m_buffer;

	void process_new_request(const std::vector<uint8_t> &buffer);
	void process_disk_request();

	attotime m_io_delay;
	emu_timer *m_io_delay_timer;
	TIMER_CALLBACK_MEMBER(process_io_request);

	void get_status(uint16_t data_size);
};


//**************************************************************************
//  DEVICE DEFINITION
//**************************************************************************

class grid210x_device : public device_t,
						public device_ieee488_interface,
						public device_image_interface
{
public:
	// construction/destruction
	grid210x_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

	// device_ieee488_interface overrides
	virtual bool ieee488_allow_recursion() const override { return false; };
	virtual void ieee488_atn(int state)  override { /*printf("ATN changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_eoi(int state)  override { /*printf("EOI changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_dav(int state)  override { /*printf("DAV changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_nrfd(int state) override { /*printf("NRFD changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_ndac(int state) override { /*printf("NDAC changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_ifc(int state)  override { /*printf("IFC changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_srq(int state)  override { /*printf("SRQ changed\n");*/ m_gpib_sync.notify_and_wait(); };
	virtual void ieee488_ren(int state)  override { /*printf("REN changed\n");*/ m_gpib_sync.notify_and_wait(); };

	// image-level overrides
	virtual bool is_readable()  const noexcept override { return true; }
	virtual bool is_writeable() const noexcept override { return true; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual const char *file_extensions() const noexcept override { return "img"; }
	virtual const char *image_type_name() const noexcept override { return "floppydisk"; }
	virtual const char *image_brief_type_name() const noexcept override { return "flop"; }

protected:
	bool is_floppy();
	bool has_disk();
	grid210x_disk_geometry get_geometry();
	void read_sector(uint32_t sector, uint8_t *buffer);
	void write_sector(uint32_t sector, const uint8_t *buffer);
	void format_disk();
	void raise_srq();

private:
	void process_command();
	void thread_entry();
	void listen_to_buffer();

	std::thread m_thread;

	std::vector<uint8_t> m_buffer;

	grid210x_gpib_sync m_gpib_sync;

	std::unique_ptr<grid210x_gpib_listener> m_listener;
	std::unique_ptr<grid210x_gpib_talker> m_talker;

	bool m_talking;
	bool m_listening;
	bool m_serial_poll;
	bool m_srq_raised;

	std::unique_ptr<grid210x_disk_emu> m_emu;
};

// device type definition
DECLARE_DEVICE_TYPE(GRID210X, grid210x_device)

#endif // MAME_BUS_IEEE488_GRID210X_H
