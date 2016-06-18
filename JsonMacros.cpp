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

#include "JsonMacros.h"

#include "KeyUsage.h"
#include <list>
#include <iostream>

static uint8_t findKeyUsage (const std::list<const std::map<std::string, uint8_t> *> &maps,
			     std::string key_name)
{
	for (const auto &map: maps) {
		auto it = map->find (key_name);
		if (it != map->end ())
			return it->second;
	}
	return 0;
}

bool JsonToMacros (const Json::Value &profile,
		   std::vector<CorsairDevice::KeySettings> &keys,
		   const std::string &layout)
{
	std::list<const std::map<std::string, uint8_t> *> keymaps = { &KeyUsage::keymap };
	std::string key_str;

	if (!layout.empty ()) {
		auto it = KeyUsage::layouts.find (layout);
		if (it != KeyUsage::layouts.end ()) {
			keymaps.push_front (&it->second);
		}
		else
			std::cerr << "warning: layout " << layout << "not found" << std::endl;
	}

	if (!profile.isArray ()) {
		std::cerr << "profile is not an array" << std::endl;
		return false;
	}

	keys.resize (profile.size ());
	for (unsigned int i = 0; i < profile.size (); ++i) {
		if (!profile[i].isMember ("key")) {
			std::cerr << "Missing \"key\" member in key " << i << std::endl;
			return false;
		}
		key_str = profile[i]["key"].asString ();
		keys[i].key_usage = findKeyUsage (keymaps, key_str);
		if (keys[i].key_usage == 0) {
			std::cerr << "Unknown key: " << key_str << std::endl;
			return false;
		}

		if (profile[i].isMember ("repeat_mode")) {
			std::string repeat_mode = profile[i]["repeat_mode"].asString ();
			if (repeat_mode == "fixed")
				keys[i].repeat_mode = CorsairDevice::KeySettings::RepeatFixed;
			else if (repeat_mode == "hold")
				keys[i].repeat_mode = CorsairDevice::KeySettings::RepeatHold;
			else if (repeat_mode == "toggle")
				keys[i].repeat_mode = CorsairDevice::KeySettings::RepeatToggle;
			else {
				std::cerr << "Unknown repeat mode: " << repeat_mode << std::endl;
				return false;
			}
		}
		else
			keys[i].repeat_mode = CorsairDevice::KeySettings::RepeatFixed;

		if (profile[i].isMember ("type")) {
			std::string type = profile[i]["type"].asString ();
			if (type == "none")
				keys[i].bind_type = CorsairDevice::KeySettings::BindNone;
			else if (type == "key")
				keys[i].bind_type = CorsairDevice::KeySettings::BindUsage;
			else if (type == "macro")
				keys[i].bind_type = CorsairDevice::KeySettings::BindMacro;
			else {
				std::cerr << "Unknown type: " << type << std::endl;
				return false;
			}
		}
		else
			keys[i].bind_type = CorsairDevice::KeySettings::BindMacro;

		switch (keys[i].bind_type) {
		case CorsairDevice::KeySettings::BindNone:
			break;

		case CorsairDevice::KeySettings::BindUsage: {
			if (!profile[i].isMember ("new_key")) {
				std::cerr << "Missing \"new_key\" member for type \"key\"" << std::endl;
				return false;
			}
			key_str = profile[i]["new_key"].asString ();
			keys[i].target_usage = findKeyUsage (keymaps, key_str);
			if (keys[i].target_usage == 0) {
				std::cerr << "Unknown key: " << key_str << std::endl;
				return false;
			}
			break;
		}

		case CorsairDevice::KeySettings::BindMacro: {
			if (profile[i].isMember ("repeat_count"))
				keys[i].repeat_count = profile[i]["repeat_count"].asUInt ();
			else
				keys[i].repeat_count = 1;

			if (!profile[i].isMember ("macro")) {
				std::cerr << "Missing \"macro\" member" << std::endl;
				return false;
			}
			Json::Value macro = profile[i]["macro"];
			if (!macro.isArray ()) {
				std::cerr << "\"macro\" must be an array" << std::endl;
				return false;
			}
			keys[i].macro.resize (macro.size ());
			for (unsigned int j = 0; j < macro.size (); ++j) {
				if (macro[j].isMember ("key")) {
					keys[i].macro[j].type = CorsairDevice::MacroItem::Key;
					key_str = macro[j]["key"].asString ();
					keys[i].macro[j].key_event.usage = findKeyUsage (keymaps, key_str);
					if (keys[i].macro[j].key_event.usage == 0) {
						std::cerr << "Unknown key: " << key_str << std::endl;
						return false;
					}
					if (!macro[j].isMember ("pressed")) {
						std::cerr << "Missing \"pressed\" member in macro item" << std::endl;
						return false;
					}
					keys[i].macro[j].key_event.pressed = macro[j]["pressed"].asBool ();
				}
				else if (macro[j].isMember ("delay")) {
					keys[i].macro[j].type = CorsairDevice::MacroItem::Delay;
					keys[i].macro[j].delay = macro[j]["delay"].asUInt ();
				}
				else {
					std::cerr << "Invalid macro item" << std::endl;
				}
			}
			break;
		}
		}
	}

	return true;
}
