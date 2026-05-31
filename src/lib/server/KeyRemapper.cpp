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

#include "server/KeyRemapper.h"

#include "barrier/KeyMap.h"
#include "base/Log.h"

namespace {

std::string
keyName(KeyID id)
{
	std::string name = barrier::KeyMap::formatKey(id, 0);
	return name.empty() ? "<none>" : name;
}

std::string
maskName(KeyModifierMask mask)
{
	std::string name = barrier::KeyMap::formatKey(kKeyNone, mask);
	return name.empty() ? "<none>" : name;
}

}

KeyRemapper::KeyRemapper() :
	m_config()
{
}

KeyRemapper::KeyRemapper(const KeyRemapConfig& config) :
	m_config(config)
{
}

KeyRemapper::KeyEvent::KeyEvent(Type type, KeyID id, KeyModifierMask mask,
		KeyButton button, SInt32 count) :
	m_type(type),
	m_id(id),
	m_mask(mask),
	m_button(button),
	m_count(count)
{
}

KeyRemapper::PressedKey::PressedKey() :
	m_sourceID(kKeyNone),
	m_remappedID(kKeyNone),
	m_sourceModifier(0),
	m_remappedModifier(0),
	m_remapped(false)
{
}

KeyRemapper::PressedKey::PressedKey(KeyID sourceID, KeyID remappedID) :
	m_sourceID(sourceID),
	m_remappedID(remappedID),
	m_sourceModifier(modifierForKey(sourceID)),
	m_remappedModifier(modifierForKey(remappedID)),
	m_remapped(sourceID != remappedID)
{
}

KeyRemapper::PendingTap::PendingTap() :
	m_sourceID(kKeyNone),
	m_aloneID(kKeyNone),
	m_holdID(kKeyNone),
	m_mask(0),
	m_button(0)
{
}

KeyRemapper::PendingTap::PendingTap(KeyID sourceID, KeyID aloneID,
		KeyID holdID, KeyModifierMask mask, KeyButton button) :
	m_sourceID(sourceID),
	m_aloneID(aloneID),
	m_holdID(holdID),
	m_mask(mask),
	m_button(button)
{
}

KeyRemapper::KeyEventList
KeyRemapper::remapKeyDown(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button)
{
	KeyEvent before(KeyEvent::kDown, id, mask, button);
	KeyEventList events;
	std::string normalizedScreen = KeyRemapConfig::normalizeScreen(screen);
	flushPendingTaps(normalizedScreen, button, events);

	KeyModifierMask translatedMask = translateMask(normalizedScreen, mask);
	const KeyRemapConfig::ChordRule* chordRule =
		m_config.findChordRule(normalizedScreen, id, translatedMask);
	if (chordRule != NULL) {
		emitChordTap(normalizedScreen, *chordRule, translatedMask, button, events);
		m_suppressedChords[normalizedScreen][button] = id;
		LOG((CLOG_DEBUG1 "key remap chord screen=\"%s\" key=%s->%s mask=%s->%s button=0x%04x",
			screen.c_str(),
			keyName(chordRule->m_fromID).c_str(),
			keyName(chordRule->m_toID).c_str(),
			maskName(chordRule->m_fromMask).c_str(),
			maskName(chordRule->m_toMask).c_str(), button));
		return events;
	}

	const KeyRemapConfig::TapRule* tapRule =
		m_config.findTapRule(normalizedScreen, id);
	if (tapRule != NULL) {
		m_pendingTaps[normalizedScreen][button] =
			PendingTap(tapRule->m_fromID, tapRule->m_aloneID,
				tapRule->m_holdID, mask, button);
		LOG((CLOG_DEBUG1 "key remap pending tap screen=\"%s\" key=%s alone=%s hold=%s button=0x%04x",
			screen.c_str(), keyName(tapRule->m_fromID).c_str(),
			keyName(tapRule->m_aloneID).c_str(),
			keyName(tapRule->m_holdID).c_str(), button));
		return events;
	}

	const KeyRemapConfig::KeyRule* rule =
		m_config.findRule(normalizedScreen, id);
	if (rule != NULL) {
		m_pressedKeys[normalizedScreen][button] =
			PressedKey(rule->m_fromID, rule->m_toID);
	}
	else {
		m_pressedKeys[normalizedScreen][button] = PressedKey(id, id);
	}

	KeyEvent after = remapKey(normalizedScreen, id, mask, 0, button,
		KeyEvent::kDown);
	logRemap("down", screen, before, after);
	events.push_back(after);
	return events;
}

KeyRemapper::KeyEventList
KeyRemapper::remapKeyUp(const std::string& screen, KeyID id,
		KeyModifierMask mask, KeyButton button)
{
	KeyEvent before(KeyEvent::kUp, id, mask, button);
	KeyEventList events;
	std::string normalizedScreen = KeyRemapConfig::normalizeScreen(screen);

	ScreenSuppressedChordMap::iterator suppressedScreen =
		m_suppressedChords.find(normalizedScreen);
	if (suppressedScreen != m_suppressedChords.end()) {
		SuppressedChordMap::iterator suppressed =
			suppressedScreen->second.find(button);
		if (suppressed != suppressedScreen->second.end()) {
			LOG((CLOG_DEBUG1 "key remap suppress chord up screen=\"%s\" key=%s button=0x%04x",
				screen.c_str(), keyName(suppressed->second).c_str(), button));
			suppressedScreen->second.erase(suppressed);
			if (suppressedScreen->second.empty()) {
				m_suppressedChords.erase(suppressedScreen);
			}
			return events;
		}
	}

	ScreenPendingTapMap::iterator pendingScreen =
		m_pendingTaps.find(normalizedScreen);
	if (pendingScreen != m_pendingTaps.end()) {
		PendingTapMap::iterator pending = pendingScreen->second.find(button);
		if (pending != pendingScreen->second.end()) {
			PendingTap tap = pending->second;
			pendingScreen->second.erase(pending);
			if (pendingScreen->second.empty()) {
				m_pendingTaps.erase(pendingScreen);
			}

			KeyEvent down(KeyEvent::kDown, tap.m_aloneID, mask, tap.m_button);
			KeyEvent up(KeyEvent::kUp, tap.m_aloneID, mask, tap.m_button);
			LOG((CLOG_DEBUG1 "key remap tap screen=\"%s\" key=%s->%s button=0x%04x",
				screen.c_str(), keyName(tap.m_sourceID).c_str(),
				keyName(tap.m_aloneID).c_str(), tap.m_button));
			events.push_back(down);
			events.push_back(up);
			return events;
		}
	}

	KeyEvent after = remapKey(normalizedScreen, id, mask, 0, button,
		KeyEvent::kUp);

	ScreenPressedKeyMap::iterator screenIndex = m_pressedKeys.find(normalizedScreen);
	if (screenIndex != m_pressedKeys.end()) {
		screenIndex->second.erase(button);
		if (screenIndex->second.empty()) {
			m_pressedKeys.erase(screenIndex);
		}
	}

	logRemap("up", screen, before, after);
	events.push_back(after);
	return events;
}

KeyRemapper::KeyEventList
KeyRemapper::remapKeyRepeat(const std::string& screen, KeyID id,
		KeyModifierMask mask, SInt32 count, KeyButton button)
{
	KeyEvent before(KeyEvent::kRepeat, id, mask, button, count);
	KeyEventList events;
	std::string normalizedScreen = KeyRemapConfig::normalizeScreen(screen);

	ScreenSuppressedChordMap::const_iterator suppressedScreen =
		m_suppressedChords.find(normalizedScreen);
	if (suppressedScreen != m_suppressedChords.end() &&
		suppressedScreen->second.find(button) != suppressedScreen->second.end()) {
		LOG((CLOG_DEBUG1 "key remap suppress chord repeat screen=\"%s\" key=%s button=0x%04x",
			screen.c_str(), keyName(id).c_str(), button));
		return events;
	}

	flushPendingTaps(normalizedScreen, 0, events);

	KeyEvent after = remapKey(normalizedScreen, id, mask, count, button,
		KeyEvent::kRepeat);
	logRemap("repeat", screen, before, after);
	events.push_back(after);
	return events;
}

void
KeyRemapper::setConfig(const KeyRemapConfig& config)
{
	m_config = config;
	reset();
}

void
KeyRemapper::reset()
{
	m_pressedKeys.clear();
	m_pendingTaps.clear();
	m_suppressedChords.clear();
}

void
KeyRemapper::resetScreen(const std::string& screen)
{
	std::string normalizedScreen = KeyRemapConfig::normalizeScreen(screen);
	m_pressedKeys.erase(normalizedScreen);
	m_pendingTaps.erase(normalizedScreen);
	m_suppressedChords.erase(normalizedScreen);
}

void
KeyRemapper::resetPending()
{
	m_pendingTaps.clear();
	m_suppressedChords.clear();
}

void
KeyRemapper::resetPendingScreen(const std::string& screen)
{
	std::string normalizedScreen = KeyRemapConfig::normalizeScreen(screen);
	m_pendingTaps.erase(normalizedScreen);
	m_suppressedChords.erase(normalizedScreen);
}

KeyRemapper::KeyEvent
KeyRemapper::remapKey(const std::string& screen, KeyID id,
		KeyModifierMask mask, SInt32 count, KeyButton button,
		KeyEvent::Type type) const
{
	KeyID remappedID = id;

	ScreenPressedKeyMap::const_iterator screenIndex = m_pressedKeys.find(screen);
	if (screenIndex != m_pressedKeys.end()) {
		PressedKeyMap::const_iterator keyIndex = screenIndex->second.find(button);
		if (keyIndex != screenIndex->second.end() && keyIndex->second.m_remapped) {
			remappedID = keyIndex->second.m_remappedID;
		}
	}

	if (remappedID == id) {
		const KeyRemapConfig::KeyRule* rule = m_config.findRule(screen, id);
		if (rule != NULL) {
			remappedID = rule->m_toID;
		}
	}

	return KeyEvent(type, remappedID, translateMask(screen, mask), button, count);
}

void
KeyRemapper::emitChordTap(const std::string& screen,
		const KeyRemapConfig::ChordRule& rule, KeyModifierMask mask,
		KeyButton button, KeyEventList& events) const
{
	KeyModifierMask releaseMask = mask & ~rule.m_toMask;
	KeyModifierMask currentMask = mask;
	KeyEventList restoreEvents;

	ScreenPressedKeyMap::const_iterator screenIndex = m_pressedKeys.find(screen);
	if (screenIndex != m_pressedKeys.end()) {
		for (PressedKeyMap::const_iterator i = screenIndex->second.begin();
				i != screenIndex->second.end(); ++i) {
			const PressedKey& key = i->second;
			if (key.m_remappedModifier == 0 ||
				(key.m_remappedModifier & releaseMask) == 0) {
				continue;
			}

			currentMask &= ~key.m_remappedModifier;
			events.push_back(KeyEvent(KeyEvent::kUp, key.m_remappedID,
				currentMask, i->first));
			restoreEvents.insert(restoreEvents.begin(),
				KeyEvent(KeyEvent::kDown, key.m_remappedID,
					currentMask | key.m_remappedModifier, i->first));
		}
	}

	events.push_back(KeyEvent(KeyEvent::kDown, rule.m_toID,
		rule.m_toMask, button));
	events.push_back(KeyEvent(KeyEvent::kUp, rule.m_toID,
		rule.m_toMask, button));
	events.insert(events.end(), restoreEvents.begin(), restoreEvents.end());
}

void
KeyRemapper::flushPendingTaps(const std::string& screen, KeyButton exceptButton,
		KeyEventList& events)
{
	ScreenPendingTapMap::iterator pendingScreen = m_pendingTaps.find(screen);
	if (pendingScreen == m_pendingTaps.end()) {
		return;
	}

	PendingTapMap& pendingTaps = pendingScreen->second;
	for (PendingTapMap::iterator i = pendingTaps.begin();
			i != pendingTaps.end(); ) {
		if (exceptButton != 0 && i->first == exceptButton) {
			++i;
			continue;
		}

		PendingTap tap = i->second;
		m_pressedKeys[screen][tap.m_button] =
			PressedKey(tap.m_sourceID, tap.m_holdID);
		KeyEvent event(KeyEvent::kDown, tap.m_holdID,
			translateMask(screen, tap.m_mask), tap.m_button);
		LOG((CLOG_DEBUG1 "key remap flush tap hold screen=\"%s\" key=%s->%s button=0x%04x",
			screen.c_str(), keyName(tap.m_sourceID).c_str(),
			keyName(tap.m_holdID).c_str(), tap.m_button));
		events.push_back(event);
		pendingTaps.erase(i++);
	}

	if (pendingTaps.empty()) {
		m_pendingTaps.erase(pendingScreen);
	}
}

KeyModifierMask
KeyRemapper::translateMask(const std::string& screen, KeyModifierMask mask) const
{
	ScreenPressedKeyMap::const_iterator screenIndex = m_pressedKeys.find(screen);
	if (screenIndex == m_pressedKeys.end()) {
		return mask;
	}

	KeyModifierMask translated = mask;
	for (PressedKeyMap::const_iterator i = screenIndex->second.begin();
			i != screenIndex->second.end(); ++i) {
		const PressedKey& key = i->second;
		if (!key.m_remapped || key.m_sourceModifier == 0) {
			continue;
		}

		bool keepSourceModifier = false;
		for (PressedKeyMap::const_iterator j = screenIndex->second.begin();
				j != screenIndex->second.end(); ++j) {
			if (j == i) {
				continue;
			}
			const PressedKey& otherKey = j->second;
			if (otherKey.m_sourceModifier == key.m_sourceModifier &&
				otherKey.m_remappedModifier != key.m_remappedModifier) {
				keepSourceModifier = true;
				break;
			}
		}

		if (!keepSourceModifier) {
			translated &= ~key.m_sourceModifier;
		}
		translated |= key.m_remappedModifier;
	}

	return translated;
}

KeyModifierMask
KeyRemapper::modifierForKey(KeyID id)
{
	switch (id) {
	case kKeyShift_L:
	case kKeyShift_R:
		return KeyModifierShift;

	case kKeyControl_L:
	case kKeyControl_R:
		return KeyModifierControl;

	case kKeyAlt_L:
	case kKeyAlt_R:
		return KeyModifierAlt;

	case kKeyMeta_L:
	case kKeyMeta_R:
		return KeyModifierMeta;

	case kKeySuper_L:
	case kKeySuper_R:
		return KeyModifierSuper;

	case kKeyAltGr:
		return KeyModifierAltGr;

	default:
		return 0;
	}
}

void
KeyRemapper::logRemap(const char* eventName, const std::string& screen,
		const KeyEvent& before, const KeyEvent& after)
{
	if (before.m_id == after.m_id && before.m_mask == after.m_mask) {
		return;
	}

	LOG((CLOG_DEBUG1 "key remap %s screen=\"%s\" key=%s->%s mask=%s->%s maskBits=0x%04x->0x%04x button=0x%04x",
		eventName, screen.c_str(), keyName(before.m_id).c_str(),
		keyName(after.m_id).c_str(), maskName(before.m_mask).c_str(),
		maskName(after.m_mask).c_str(), before.m_mask, after.m_mask,
		before.m_button));
}
