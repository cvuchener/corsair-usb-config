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

#ifndef K90_DEVICE_H
#define K90_DEVICE_H

#include "CorsairDevice.h"

class K90Device: public CorsairDevice
{
public:
	K90Device (libusb_device *dev);

	virtual unsigned int getBacklightBrightness ();
	virtual void setBacklightBrightness (unsigned int brightness);

	virtual unsigned int getCurrentProfile ();

	virtual Color getProfileColor (unsigned int profile_index);
	virtual void setProfileColor (unsigned int profile_index, Color color);

private:
	enum K90Request: uint8_t {
		SetBacklightBrightness = 49,
	};

	struct K90Status {
		uint8_t unk1[4];
		uint8_t backlight_brightness;
		uint8_t unk2[2];
		uint8_t current_profile;
	} __attribute__ ((packed));
};

#endif

