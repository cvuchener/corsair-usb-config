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
#include "K90Device.h"
#include "K40Device.h"

#include "KeyUsage.h"
#include "JsonMacros.h"

#include <set>
#include <functional>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include <json/json.h>
#include <json/reader.h>

extern "C" {
#include <unistd.h>
}

constexpr uint16_t CORSAIR_VENDOR_ID = 0x1b1c;

enum ProductID: uint16_t {
	CORSAIR_K90_ID = 0x1b02,
	CORSAIR_K40_ID = 0x1b0e,
};

struct DeviceInfo {
	std::set<uint16_t> products;
	std::function<CorsairDevice *(libusb_device *)> factory;
} device_table[] = {
	{
		{ CORSAIR_K90_ID },
		[] (libusb_device *dev) { return new K90Device (dev); }
	},
	{
		{ CORSAIR_K40_ID },
		[] (libusb_device *dev) { return new K40Device (dev); }
	},
};

static const char *usage = R"(Usage: %s [options] command

Options are:
	-d address	Use this device instead of first found.
	-l layout	Use layout for converting string to key codes (in send-macros command).
	-h		Print this help.

Commands are:
list
	List supported devices.
mode get
	Get the current macro mode.
mode set HW|SW
	Set the current macro mode to hardware or software.
backlight get
	Get the backlight brightness.
backlight set value
	Set the backlight brightness to value (from 0 to 3).
current-profile get
	Get the current profile.
current-profile set index
	Set the current profile to index (from 1 to 3).
profile-color get index
	Get the color for profile index.
profile-color set index color
	Set the color for profile index to color (24 bits hexadecimal code).
send-macros profile_index [file]
	Send macros read from file or stdin.
)";

libusb_device *findDevice (libusb_context *context, const char *address = nullptr);
CorsairDevice *initDevice (libusb_device *dev);
bool commandMode (CorsairDevice *cdev, const char * const *args);
bool commandBacklight (CorsairDevice *cdev, const char * const *args);
bool commandCurrentProfile (CorsairDevice *cdev, const char * const *args);
bool commandProfileColor (CorsairDevice *cdev, const char * const *args);
bool commandSendMacros (CorsairDevice *cdev, const char * const *args);

std::string layout;

int main (int argc, char *argv[])
{
	const char *address = nullptr;

	int opt;
	while (-1 != (opt = getopt (argc, argv, "d:l:h"))) {
		switch (opt) {
		case 'd':
			address = optarg;
			break;

		case 'l':
			layout.assign (optarg);
			break;

		case 'h':
			fprintf (stderr, usage, argv[0]);
			std::cerr << std::endl;
			std::cerr << "Available layouts are:" << std::endl;
			for (const auto &pair: KeyUsage::layouts)
				std::cerr << pair.first << std::endl;
			return EXIT_SUCCESS;

		default:
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fprintf (stderr, "Missing command.\n");
		fprintf (stderr, usage, argv[0]);
		return EXIT_FAILURE;
	}
	std::string command = argv[optind];

	libusb_context *context;
	bool failed = false;
	int ret;
	if (0 != (ret = libusb_init (&context))) {
		fprintf (stderr, "Failed to initialize libusb: %s\n", libusb_error_name (ret));
		return EXIT_FAILURE;
	}

	if (command == "list") {
		libusb_device **list;
		int count;
		if (0 > (count = libusb_get_device_list (context, &list))) {
			fprintf (stderr, "Failed to get device list: %s\n", libusb_error_name (count));
			failed = true;
			goto cleanup;
		}
		for (int i = 0; i < count; ++i) {
			libusb_device *dev = list[i];
			libusb_device_descriptor desc;
			libusb_get_device_descriptor (dev, &desc);
			if (desc.idVendor != CORSAIR_VENDOR_ID)
				continue;
			for (auto info: device_table) {
				if (info.products.find (desc.idProduct) != info.products.end ()) {
					uint8_t busnum = libusb_get_bus_number (dev);
					uint8_t ports[7];
					int port_count = libusb_get_port_numbers (dev, ports, sizeof (ports));
					if (port_count == 0)
						ports[0] = 0;
					printf ("%d-%d", busnum, ports[0]);
					for (int j = 1; j < port_count; ++j)
						printf (".%d", ports[j]);
					printf (": %04hx:%04hx", desc.idVendor, desc.idProduct);
					libusb_device_handle *handle;
					ret = libusb_open (dev, &handle);
					if (ret == 0 ) {
						unsigned char string[256];
						if (desc.iManufacturer != 0) {
							ret = libusb_get_string_descriptor_ascii (handle, desc.iManufacturer, string, sizeof (string));
							printf (" %*s", ret, string);
						}
						if (desc.iProduct != 0) {
							ret = libusb_get_string_descriptor_ascii (handle, desc.iProduct, string, sizeof (string));
							printf (" %*s", ret, string);
						}
						libusb_close (handle);
					}
					printf ("\n");
					break;
				}
			}
		}
		libusb_free_device_list (list, count);
	}
	else {
		libusb_device *dev = findDevice (context, address);
		if (!dev) {
			fprintf (stderr, "Could not find device.\n");
			failed = true;
			goto cleanup;
		}
		CorsairDevice *cdev = initDevice (dev);
		libusb_unref_device (dev);
		if (!cdev) {
			fprintf (stderr, "Not a valid device.\n");
			failed = true;
			goto cleanup;
		}

		if (command == "mode") {
			if (!commandMode (cdev, &argv[optind+1]))
				failed = true;
		}
		else if (command == "backlight") {
			if (!commandBacklight (cdev, &argv[optind+1]))
				failed = true;
		}
		else if (command == "current-profile") {
			if (!commandCurrentProfile (cdev, &argv[optind+1]))
				failed = true;
		}
		else if (command == "profile-color") {
			if (!commandProfileColor (cdev, &argv[optind+1]))
				failed = true;
		}
		else if (command == "send-macros") {
			if (!commandSendMacros (cdev, &argv[optind+1]))
				failed = true;
		}
		else if (command == "raw-status") {
			std::vector<uint8_t> status = cdev->getRawStatus ();
			printf ("Status:");
			for (uint8_t byte: status)
				printf (" %02hhx", byte);
			printf ("\n");
		}
		else {
			fprintf (stderr, "Unknown command: %s\n", command.c_str ());
			failed = true;
			goto cleanup;
		}
		delete cdev;
	}
cleanup:
	libusb_exit (context);
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

libusb_device *findDevice (libusb_context *context, const char *address)
{
	libusb_device **list;
	int count;
	if (0 > (count = libusb_get_device_list (context, &list))) {
		fprintf (stderr, "Failed to get device list: %s\n", libusb_error_name (count));
		return nullptr;
	}
	for (int i = 0; i < count; ++i) {
		libusb_device *dev = list[i];
		libusb_device_descriptor desc;
		libusb_get_device_descriptor (dev, &desc);
		if (!address) {
			if (desc.idVendor != CORSAIR_VENDOR_ID)
				continue;
			for (auto info: device_table) {
				if (info.products.find (desc.idProduct) != info.products.end ()) {
					dev = libusb_ref_device (dev);
					libusb_free_device_list (list, count);
					return dev;
				}
			}
		}
		else {
			uint8_t dev_busnum = libusb_get_bus_number (dev);
			uint8_t dev_ports[7];
			int dev_port_count = libusb_get_port_numbers (dev, dev_ports, sizeof (dev_ports));

			const char *del = strchr (address, '-');
			if (!del) {
				fprintf (stderr, "Invalid address.");
				goto fail;
			}
			unsigned int addr_busnum = std::stoul (std::string (address, del));
			if (addr_busnum != dev_busnum)
				continue;
			const char *begin = del+1;
			int addr_port_count = 0;
			unsigned int addr_ports[7];
			do {
				del = strchr (begin, '.');
				const char *end;
				if (del)
					end = del;
				else
					end = strchr (begin, '\0');
				addr_ports[addr_port_count] = std::stoul (std::string (begin, end));
				++addr_port_count;
				begin = end+1;
			} while (del);

			bool found = false;
			if (dev_port_count == 0 && addr_port_count == 1 && addr_ports[0] == 0) {
				found = true;
			}
			else if (dev_port_count == addr_port_count) {
				found = true;
				for (int j = 0; j < dev_port_count; ++j) {
					if (dev_ports[j] != addr_ports[j]) {
						found = false;
						break;
					}
				}
			}

			if (found) {
				dev = libusb_ref_device (dev);
				libusb_free_device_list (list, count);
				return dev;
			}
		}
	}
fail:
	libusb_free_device_list (list, count);
	return nullptr;
}

CorsairDevice *initDevice (libusb_device *dev)
{
	libusb_device_descriptor desc;
	libusb_get_device_descriptor (dev, &desc);
	if (desc.idVendor != CORSAIR_VENDOR_ID)
		return nullptr;
	for (auto info: device_table) {
		if (info.products.find (desc.idProduct) != info.products.end ()) {
			return info.factory (dev);
		}
	}
	return nullptr;
}

bool commandMode (CorsairDevice *cdev, const char * const *args)
{
	if (!args[0]) {
		fprintf (stderr, "Missing operation.\n");
		return false;
	}
	std::string op = args[0];
	if (op == "get") {
		CorsairDevice::Mode mode = cdev->getMode ();
		switch (mode) {
		case CorsairDevice::HardwareMode:
			printf ("HW\n");
			break;

		case CorsairDevice::SoftwareMode:
			printf ("SW\n");
			break;

		case CorsairDevice::FirmwareUpdateMode:
			printf ("FW\n");
			break;

		default:
			printf ("Unknown\n");
		}
	}
	else if (op == "set") {
		if (!args[1]) {
			fprintf (stderr, "Missing mode.\n");
			return false;
		}
		std::string mode = args[1];
		if (mode == "HW")
			cdev->setMode (CorsairDevice::HardwareMode);
		else if (mode == "SW")
			cdev->setMode (CorsairDevice::SoftwareMode);
		else {
			fprintf (stderr, "Unknown mode: %s.\n", mode.c_str ());
			return false;
		}
	}
	else {
		fprintf (stderr, "Unknown operation: %s.\n", op.c_str ());
		return false;
	}
	return true;
}

bool commandBacklight (CorsairDevice *cdev, const char * const *args)
{
	if (!args[0]) {
		fprintf (stderr, "Missing operation.\n");
		return false;
	}
	std::string op = args[0];
	if (op == "get") {
		printf ("%d\n", cdev->getBacklightBrightness ());
	}
	else if (op == "set") {
		if (!args[1]) {
			fprintf (stderr, "Missing baclight brightness.\n");
			return false;
		}
		unsigned int brightness = std::stoul (args[1]);
		cdev->setBacklightBrightness (brightness);
	}
	else {
		fprintf (stderr, "Unknown operation: %s.\n", op.c_str ());
		return false;
	}
	return true;
}

bool commandCurrentProfile (CorsairDevice *cdev, const char * const *args)
{
	if (!args[0]) {
		fprintf (stderr, "Missing operation.\n");
		return false;
	}
	std::string op = args[0];
	if (op == "get") {
		printf ("%d\n", cdev->getCurrentProfile ());
	}
	else if (op == "set") {
		if (!args[1]) {
			fprintf (stderr, "Missing profile index.\n");
			return false;
		}
		unsigned int profile_index = std::stoul (args[1]);
		cdev->setCurrentProfile (profile_index);
	}
	else {
		fprintf (stderr, "Unknown operation: %s.\n", op.c_str ());
		return false;
	}
	return true;
}

bool commandProfileColor (CorsairDevice *cdev, const char * const *args)
{
	if (!args[0]) {
		fprintf (stderr, "Missing operation.\n");
		return false;
	}
	std::string op = args[0];
	if (!args[1]) {
		fprintf (stderr, "Missing profile index.\n");
		return false;
	}
	unsigned int profile_index = std::stoul (args[1]);
	if (op == "get") {
		Color color = cdev->getProfileColor (profile_index);
		printf ("%02hhx%02hhx%02hhx\n", color.r, color.g, color.b);
	}
	else if (op == "set") {
		if (!args[2]) {
			fprintf (stderr, "Missing color.\n");
			return false;
		}
		unsigned int c = std::stoul (args[2], nullptr, 16);
		Color color = {
			static_cast<uint8_t> (c >> 16),
			static_cast<uint8_t> (c >> 8),
			static_cast<uint8_t> (c)
		};
		cdev->setProfileColor (profile_index, color);
	}
	else {
		fprintf (stderr, "Unknown operation: %s._n", op.c_str ());
		return false;
	}
	return true;
}

bool commandSendMacros (CorsairDevice *cdev, const char * const *args)
{
	if (!args[0]) {
		fprintf (stderr, "Missing profile index.\n");
		return false;
	}
	unsigned int profile_index = std::stoul (args[0]);

	Json::Value profile_json;
	Json::Reader reader;
	bool ok;
	if (args[1]) {
		std::ifstream file (args[1], std::ifstream::in);
		ok = reader.parse (file, profile_json);
	}
	else {
		ok = reader.parse (std::cin, profile_json);
	}
	if (!ok) {
		fprintf (stderr, "Error while parsing JSON:\n"
		                 "%s",
		         reader.getFormattedErrorMessages ().c_str ());
		return EXIT_FAILURE;
	}

	std::vector<CorsairDevice::KeySettings> keys;
	if (!JsonToMacros (profile_json, keys, layout)) {
		fprintf (stderr, "Invalid profile structure\n");
		return EXIT_FAILURE;
	}

	cdev->setKeys (profile_index, keys);

	return true;
}

