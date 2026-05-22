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

#include "barrier/key_types.h"

#include <map>
#include <string>
#include <vector>

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
			KeyButton button, SInt32 count = 0);

	public:
		Type            m_type;
		KeyID            m_id;
		KeyModifierMask  m_mask;
		KeyButton        m_button;
		SInt32           m_count;
	};

	typedef std::vector<KeyEvent> KeyEventList;

	KeyEventList remapKeyDown(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button);
	KeyEventList remapKeyUp(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button);
	KeyEventList remapKeyRepeat(const std::string& screen, KeyID id,
		KeyModifierMask mask, SInt32 count, KeyButton button);

	void reset();
	void resetScreen(const std::string& screen);
	void resetPending();
	void resetPendingScreen(const std::string& screen);

private:
	class Rule {
	public:
		Rule(KeyID fromID, KeyID toID);

	public:
		KeyID            m_fromID;
		KeyID            m_toID;
		KeyModifierMask  m_fromModifier;
		KeyModifierMask  m_toModifier;
	};

	class TapRule {
	public:
		TapRule(KeyID fromID, KeyID aloneID, KeyID holdID);

	public:
		KeyID m_fromID;
		KeyID m_aloneID;
		KeyID m_holdID;
	};

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
			KeyModifierMask mask, KeyButton button);

	public:
		KeyID            m_sourceID;
		KeyID            m_aloneID;
		KeyID            m_holdID;
		KeyModifierMask  m_mask;
		KeyButton        m_button;
	};

	typedef std::map<KeyButton, PressedKey> PressedKeyMap;
	typedef std::map<KeyButton, PendingTap> PendingTapMap;
	typedef std::map<std::string, PressedKeyMap> ScreenPressedKeyMap;
	typedef std::map<std::string, PendingTapMap> ScreenPendingTapMap;

	KeyEvent remapKey(const std::string& screen, KeyID id,
		KeyModifierMask mask, SInt32 count, KeyButton button,
		KeyEvent::Type type) const;
	void flushPendingTaps(const std::string& screen, KeyButton exceptButton,
		KeyEventList& events);
	KeyModifierMask translateMask(const std::string& screen,
		KeyModifierMask mask) const;
	const Rule* findRule(const std::string& screen, KeyID id) const;
	const TapRule* findTapRule(const std::string& screen, KeyID id) const;
	static KeyModifierMask modifierForKey(KeyID id);
	static std::string normalizeScreen(const std::string& screen);
	static void logRemap(const char* eventName, const std::string& screen,
		const KeyEvent& before, const KeyEvent& after);

private:
	ScreenPressedKeyMap m_pressedKeys;
	ScreenPendingTapMap m_pendingTaps;
};
