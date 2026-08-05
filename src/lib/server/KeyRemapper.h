/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "server/KeyRemapConfig.h"
#include "inputleap/key_types.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace inputleap {

class KeyRemapper {
public:
	class KeyEvent {
	public:
		enum Type {
			kDown,
			kUp,
			kRepeat
		};

		KeyEvent(Type type, KeyID id, KeyModifierMask mask,
			KeyButton button, std::int32_t count = 0);

	public:
		Type            m_type;
		KeyID            m_id;
		KeyModifierMask  m_mask;
		KeyButton        m_button;
		std::int32_t     m_count;
	};

	typedef std::vector<KeyEvent> KeyEventList;
	typedef std::map<std::string, KeyEventList> ScreenKeyEventMap;

	KeyRemapper();
	explicit KeyRemapper(const KeyRemapConfig& config);

	KeyEventList remapKeyDown(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button);
	KeyEventList remapKeyUp(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button);
	KeyEventList remapKeyRepeat(const std::string& screen, KeyID id,
		KeyModifierMask mask, std::int32_t count, KeyButton button);

	void setConfig(const KeyRemapConfig& config);
	void reset();
	void resetScreen(const std::string& screen);
	void resetPending();
	void resetPendingScreen(const std::string& screen);
	bool hasPendingTaps() const;
	ScreenKeyEventMap flushPendingTapHolds();

private:
	class PressedKey {
	public:
		PressedKey();
		PressedKey(KeyID sourceID, KeyID remappedID);

	public:
		KeyID            m_sourceID;
		KeyID            m_remappedID;
		KeyModifierMask  m_sourceModifier;
		KeyModifierMask  m_remappedModifier;
		bool             m_remapped;
	};

	class PendingTap {
	public:
		PendingTap();
		PendingTap(KeyID sourceID, KeyID aloneID, KeyID holdID,
			KeyModifierMask aloneMask, KeyModifierMask mask, KeyButton button);

	public:
		KeyID            m_sourceID;
		KeyID            m_aloneID;
		KeyID            m_holdID;
		KeyModifierMask  m_aloneMask;
		KeyModifierMask  m_mask;
		KeyButton        m_button;
	};

	typedef std::map<KeyButton, PressedKey> PressedKeyMap;
	typedef std::map<KeyButton, PendingTap> PendingTapMap;
	typedef std::map<KeyButton, KeyID> SuppressedChordMap;
	typedef std::map<std::string, PressedKeyMap> ScreenPressedKeyMap;
	typedef std::map<std::string, PendingTapMap> ScreenPendingTapMap;
	typedef std::map<std::string, SuppressedChordMap> ScreenSuppressedChordMap;

	KeyEvent remapKey(const std::string& screen, KeyID id,
		KeyModifierMask mask, std::int32_t count, KeyButton button,
		KeyEvent::Type type) const;
	void flushPendingTaps(const std::string& screen, KeyButton exceptButton,
		KeyEventList& events);
	void emitChordTap(const std::string& screen,
		const KeyRemapConfig::ChordRule& rule, KeyModifierMask mask,
		KeyButton button, KeyEventList& events) const;
	KeyModifierMask translateMask(const std::string& screen,
		KeyModifierMask mask) const;
	static KeyModifierMask modifierForKey(KeyID id);
	static void logRemap(const char* eventName, const std::string& screen,
		const KeyEvent& before, const KeyEvent& after);

private:
	KeyRemapConfig m_config;
	ScreenPressedKeyMap m_pressedKeys;
	ScreenPendingTapMap m_pendingTaps;
	ScreenSuppressedChordMap m_suppressedChords;
};

} // namespace inputleap
