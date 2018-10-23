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

#include "K30Device.h"

K30Device::K30Device (libusb_device *dev):
	CorsairDevice (dev, sizeof (K30Status))
{
}

unsigned int K30Device::getBacklightBrightness ()
{
	std::vector<uint8_t> raw_status = getRawStatus ();
	K30Status *status = reinterpret_cast<K30Status *> (raw_status.data ());
	return status->backlight_brightness;
}

void K30Device::setBacklightBrightness (unsigned int brightness)
{
	int ret;
	if (brightness > 3)
		brightness = 3;
	ret = libusb_control_transfer (_dev, RequestOutType, SetBacklightBrightness,
				       brightness << 8, 0, nullptr, 0, 0);
	if (ret < 0) {
		throw std::runtime_error (libusb_error_name (ret));
	}
}

unsigned int K30Device::getCurrentProfile ()
{
	std::vector<uint8_t> raw_status = getRawStatus ();
	K30Status *status = reinterpret_cast<K30Status *> (raw_status.data ());
	return status->current_profile;
}

Color K30Device::getProfileColor (unsigned int profile_index)
{
	throw FeatureNotSupported ();
}

void K30Device::setProfileColor (unsigned int profile_index, Color color)
{
	throw FeatureNotSupported ();
}

