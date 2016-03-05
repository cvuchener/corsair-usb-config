/*
 * Copyright 2016 Clément Vuchener
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CORSAIR_DEVICE_H
#define CORSAIR_DEVICE_H

#include <cstdint>
#include <stdexcept>
#include <vector>

extern "C" {
#include <libusb.h>
}

struct Color {
	uint8_t r, g, b;
};

class CorsairDevice
{
public:
	class FeatureNotSupported: public std::exception
	{
	public:
		FeatureNotSupported ();
		virtual const char *what () noexcept;
	};

	CorsairDevice (libusb_device *dev, std::size_t status_size);
	virtual ~CorsairDevice ();

	enum Mode: uint8_t {
		HardwareMode = 0x01,
		FirmwareUpdateMode = 0x10,
		SoftwareMode = 0x30,
	};

	Mode getMode ();
	void setMode (Mode mode);

	virtual unsigned int getBacklightBrightness () = 0;
	virtual void setBacklightBrightness (unsigned int brightness) = 0;

	virtual unsigned int getCurrentProfile () = 0;
	virtual void setCurrentProfile (unsigned int index);

	virtual Color getProfileColor (unsigned int profile_index) = 0;
	virtual void setProfileColor (unsigned int profile_index, Color color) = 0;

	struct MacroItem {
		enum Type: uint8_t {
			Key = 0x84,
			End = 0x86,
			Delay = 0x87,
		} type;
		union {
			struct {
				uint8_t usage;
				bool pressed;
			} key_event;
			uint16_t delay;
		};
	};

	struct KeySettings {
		uint8_t key_usage;
		enum RepeatMode: uint8_t {
			RepeatFixed = 1,
			RepeatHold = 2,
			RepeatToggle = 3,
		} repeat_mode;
		enum BindType: uint8_t {
			BindNone = 0x00,
			BindUsage = 0x10,
			BindMacro = 0x20,
		} bind_type;
		uint8_t target_usage;
		uint16_t repeat_count;
		std::vector<MacroItem> macro;
	};

	void setKeys (unsigned int profile_index, const std::vector<KeySettings> &keys);

	std::vector<uint8_t> getRawStatus ();
	bool checkErrorState ();

protected:
	enum CorsairRequest: uint8_t {
		SetMode = 2,
		GetMode = 5,
		Status = 4,
		SetCurrentProfile = 20,
		MacroBindings = 16,
		MacroData = 18,
		MacroKeys = 22,
	};

	static constexpr uint8_t RequestInType =
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
	static constexpr uint8_t RequestOutType =
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;

	libusb_device_handle *_dev;
	std::size_t _status_size;
};

#endif

