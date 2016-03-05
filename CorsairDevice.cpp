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

#include "CorsairDevice.h"

#include <string>
#include <tuple>

extern "C" {
#include <unistd.h>
}

CorsairDevice::FeatureNotSupported::FeatureNotSupported ()
{
}

const char *CorsairDevice::FeatureNotSupported::what () noexcept
{
	return "Feature not supported.";
}

constexpr unsigned int Delay = 200000;

CorsairDevice::CorsairDevice (libusb_device *dev, std::size_t status_size):
	_status_size (status_size)
{
	int err;
	if (0 != (err = libusb_open (dev, &_dev))) {
		throw std::runtime_error (libusb_error_name (err));
	}
}

CorsairDevice::~CorsairDevice ()
{
	libusb_close (_dev);
}

CorsairDevice::Mode CorsairDevice::getMode ()
{
	int ret;
	uint8_t data[2];
	ret = libusb_control_transfer (_dev, RequestInType, GetMode,
				       0, 0, data, sizeof (data), 0);
	if (ret < 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
	return static_cast<Mode> (data[0]);
}

void CorsairDevice::setMode (Mode mode)
{
	int ret;
	ret = libusb_control_transfer (_dev, RequestOutType, SetMode,
				       mode, 0, nullptr, 0, 0);
	if (ret < 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
}

void CorsairDevice::setCurrentProfile (unsigned int index)
{
	int ret;
	if (index < 1 || index > 3) {
		throw std::invalid_argument ("Index must be between 1 and 3.");
	}
	ret = libusb_control_transfer (_dev, RequestOutType, SetCurrentProfile,
				       index, 0, nullptr, 0, 0);
	if (ret != 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
}

template <typename T>
static void append (std::vector<uint8_t> &vec, T value) {
	// Push bytes in big endian order
	for (unsigned int i = 0; i < sizeof (T); ++i)
		vec.push_back ((value >> 8*(sizeof (T)-1 - i)) & 0xFF);
}

void CorsairDevice::setKeys (unsigned int profile_index, const std::vector<KeySettings> &keys)
{
	int ret;
	if (profile_index < 1 || profile_index > 3) {
		throw std::invalid_argument ("Profile index must be between 1 and 3.");
	}

	std::vector<uint8_t> raw_keys, raw_bindings, raw_data;

	// Build raw data from key usages and macro items
	std::vector<unsigned int> addresses;
	for (const auto &key: keys) {
		addresses.push_back (raw_data.size ());
		switch (key.bind_type) {
		case KeySettings::BindNone:
			break;

		case KeySettings::BindUsage:
			append (raw_data, key.target_usage);
			break;

		case KeySettings::BindMacro:
			for (const auto &item: key.macro) {
				append (raw_data, item.type);
				switch (item.type) {
				case MacroItem::Key:
					append (raw_data, item.key_event.usage);
					append (raw_data, static_cast<uint8_t> (item.key_event.pressed ? 0x01 : 0x00));
					break;

				case MacroItem::Delay:
					append (raw_data, item.delay);
					break;

				case MacroItem::End:
					// written after this loop, it should not be in this vector
					break;
				}
			}
			append (raw_data, MacroItem::End);
			append (raw_data, key.repeat_count);
			break;
		}
	}
	addresses.push_back (raw_data.size ());

	// Build key and binding data
	append (raw_keys, static_cast<uint8_t> (keys.size ()));
	append (raw_bindings, static_cast<uint8_t> (keys.size ()));
	append (raw_bindings, static_cast<uint16_t> (5 + 5*keys.size ())); // size of raw_bindings
	append (raw_bindings, static_cast<uint16_t> (raw_data.size ()));
	for (unsigned int i = 0; i < keys.size (); ++i) {
		append (raw_keys, keys[i].key_usage);
		append (raw_keys, keys[i].repeat_mode);

		append (raw_bindings, keys[i].bind_type);
		if (keys[i].bind_type == KeySettings::BindNone)
			append (raw_bindings, static_cast<uint16_t> (0));
		else
			append (raw_bindings, static_cast<uint16_t> (addresses[i]));
		append (raw_bindings, static_cast<uint16_t> (addresses[i+1] - addresses[i]));
	}

	// Send data to device
	for (auto tuple: { std::make_tuple (&raw_bindings, MacroBindings),
			   std::make_tuple (&raw_data, MacroData),
			   std::make_tuple (&raw_keys, MacroKeys) }) {
                std::vector<uint8_t> *packet;
                uint8_t request;
                std::tie (packet, request) = tuple;

                if (request == MacroData && packet->size () == 0)
			continue;

                ret = libusb_control_transfer (_dev, RequestOutType, request,
                                               0, profile_index,
					       packet->data (), packet->size (), 0);

                if (ret < 0) {
			throw std::runtime_error (libusb_error_name (ret));
                }
                else if ((unsigned int) ret != packet->size ()) {
                        throw std::runtime_error ("Incomplete transfer");
                }

                usleep (Delay);

		if (!checkErrorState ()) {
			throw std::runtime_error ("Transfer error (going too fast?).");
		}

                usleep (Delay);
        }
}

std::vector<uint8_t> CorsairDevice::getRawStatus ()
{
	int ret;
	std::vector<uint8_t> status (_status_size);
	ret = libusb_control_transfer (_dev, RequestInType, Status,
				       0, 0,
				       status.data (), _status_size, 0);
	if (ret < 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
	return status;
}

bool CorsairDevice::checkErrorState ()
{
	int ret;
	uint8_t data[2];
	ret = libusb_control_transfer (_dev, RequestInType, GetMode,
				       0, 0, data, sizeof (data), 0);
	if (ret < 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
	return data[1] == 0x01;
}
