/* -*- mode: c++ -*-
 * Kaleidoscope-LED-Palette-Theme -- Palette-based LED theme foundation
 * Copyright (C) 2017, 2018, 2019  Keyboard.io, Inc
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <LED-Palette-Theme-Defy.h>
#include <Kaleidoscope-EEPROM-Settings.h>
#include <Kaleidoscope-FocusSerial.h>

namespace kaleidoscope {
namespace plugin {

uint16_t LEDPaletteThemeDefy::palette_base_;
uint16_t LEDPaletteThemeDefy::leds_per_layer_in_memory_;

uint16_t LEDPaletteThemeDefy::reserveThemes(uint8_t max_themes) {
  if (!palette_base_)
	palette_base_ = ::EEPROMSettings.requestSlice(16*sizeof(cRGB));
  //plus 2 so that we can have the neuron in a full position
  leds_per_layer_in_memory_ = (Runtime.device().led_count)/2;
  return ::EEPROMSettings.requestSlice(max_themes*leds_per_layer_in_memory_);
}

void LEDPaletteThemeDefy::updateHandler(uint16_t theme_base, uint8_t theme) {
  if (!Runtime.has_leds)
	return;

  uint16_t map_base = theme_base + (theme*leds_per_layer_in_memory_);

  for (uint8_t pos = Runtime.device().led_count - 2; pos < Runtime.device().led_count; pos++) {
	cRGB color = lookupColorAtPosition(map_base, pos);
	::LEDControl.setCrgbAt(pos, color);
  }

}

void LEDPaletteThemeDefy::refreshAt(uint16_t theme_base, uint8_t theme, KeyAddr key_addr) {
  if (!Runtime.has_leds)
	return;

  uint16_t map_base = theme_base + (theme*leds_per_layer_in_memory_);
  uint8_t pos = Runtime.device().getLedIndex(key_addr);

  cRGB color = lookupColorAtPosition(map_base, pos);
  ::LEDControl.setCrgbAt(key_addr, color);
}

const uint8_t LEDPaletteThemeDefy::lookupColorIndexAtPosition(uint16_t map_base, uint16_t position) {
  uint8_t color_index;

  color_index = Runtime.storage().read(map_base + position/2);
  if (position%2)
	color_index &= ~0xf0;
  else
	color_index >>= 4;

  return color_index;
}

const cRGB LEDPaletteThemeDefy::lookupColorAtPosition(uint16_t map_base, uint16_t position) {
  uint8_t color_index = lookupColorIndexAtPosition(map_base, position);
  return lookupPaletteColor(color_index);
}

const cRGB LEDPaletteThemeDefy::lookupPaletteColor(uint8_t color_index) {
  cRGB color;

  Runtime.storage().get(palette_base_ + color_index*sizeof(cRGB), color);
  color.r ^= 0xff;
  color.g ^= 0xff;
  color.b ^= 0xff;
  color.w ^= 0xff;
  return color;
}

void LEDPaletteThemeDefy::updateColorIndexAtPosition(uint16_t map_base, uint16_t position, uint8_t color_index) {
  uint8_t indexes;

  indexes = Runtime.storage().read(map_base + position/2);
  if (position%2) {
	uint8_t other = indexes >> 4;
	indexes = (other << 4) + color_index;
  } else {
	uint8_t other = indexes & ~0xf0;
	indexes = (color_index << 4) + other;
  }
  Runtime.storage().update(map_base + position/2, indexes);
  Runtime.storage().commit();
}

EventHandlerResult LEDPaletteThemeDefy::onFocusEvent(const char *command) {
  if (!Runtime.has_leds)
	return EventHandlerResult::OK;

  const char *cmd = "palette";

  if (::Focus.handleHelp(command, cmd))
	return EventHandlerResult::OK;

  if (strcmp(command, cmd)!=0)
	return EventHandlerResult::OK;

  if (::Focus.isEOL()) {
	for (uint8_t i = 0; i < 16; i++) {
	  cRGB color;

	  color = lookupPaletteColor(i);
	  ::Focus.send(color.r, color.g, color.b, color.w);
	}
	return EventHandlerResult::EVENT_CONSUMED;
  }

  uint8_t i = 0;
  while (i < 16 && !::Focus.isEOL()) {
	cRGB color;

	::Focus.read(color.r);
	::Focus.read(color.g);
	::Focus.read(color.b);
	::Focus.read(color.w);
	::LEDPaletteThemeDefy.updatePaletteColor(i, color);
	i++;
  }
  Runtime.storage().commit();

  ::LEDControl.refreshAll();

  return EventHandlerResult::EVENT_CONSUMED;
}

EventHandlerResult LEDPaletteThemeDefy::themeFocusEvent(const char *command,
														const char *expected_command,
														uint16_t theme_base,
														uint8_t max_themes) {
  if (!Runtime.has_leds)
	return EventHandlerResult::OK;

  if (::Focus.handleHelp(command, expected_command))
	return EventHandlerResult::OK;

  if (strcmp_P(command, expected_command)!=0)
	return EventHandlerResult::OK;

  uint16_t max_index = (max_themes*leds_per_layer_in_memory_);

  if (::Focus.isEOL()) {
	for (uint16_t pos = 0; pos < max_index; pos++) {
	  uint8_t indexes = Runtime.storage().read(theme_base + pos);

	  ::Focus.send((uint8_t)(indexes >> 4), indexes & ~0xf0);
	}
	return EventHandlerResult::EVENT_CONSUMED;
  }

  uint16_t pos = 0;

  while (!::Focus.isEOL() && (pos < max_index)) {
	uint8_t idx1, idx2;
	::Focus.read(idx1);
	::Focus.read(idx2);

	uint8_t indexes = (idx1 << 4) + idx2;

	Runtime.storage().update(theme_base + pos, indexes);
	pos++;
  }
  Runtime.device().syncLayers();
  Runtime.storage().commit();

  ::LEDControl.refreshAll();

  return EventHandlerResult::EVENT_CONSUMED;
}

void LEDPaletteThemeDefy::updatePaletteColor(uint8_t palette_index, cRGB color) {
  color.r ^= 0xff;
  color.g ^= 0xff;
  color.b ^= 0xff;
  color.w ^= 0xff;

  Runtime.storage().put(palette_base_ + palette_index*sizeof(color), color);
}
}
}

kaleidoscope::plugin::LEDPaletteThemeDefy LEDPaletteThemeDefy;