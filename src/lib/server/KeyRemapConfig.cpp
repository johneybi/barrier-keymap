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

// Keep this first: MSVC otherwise resolves generated config.h to server/Config.h.
#include "server/Config.h"
#include "server/KeyRemapConfig.h"

#include "inputleap/KeyMap.h"

#include <algorithm>
#include <cctype>
#include <ostream>

namespace inputleap {

namespace {

std::int32_t
countModifiers(KeyModifierMask mask)
{
	std::int32_t count = 0;
	while (mask != 0) {
		if ((mask & 1u) != 0) {
			++count;
		}
		mask >>= 1;
	}
	return count;
}

KeyModifierMask
commandModifierMask()
{
	return KeyModifierShift | KeyModifierControl | KeyModifierAlt |
		KeyModifierMeta | KeyModifierSuper | KeyModifierAltGr;
}

}

KeyRemapConfig::KeyRule::KeyRule() :
	m_fromID(kKeyNone),
	m_toID(kKeyNone)
{
}

KeyRemapConfig::KeyRule::KeyRule(KeyID fromID, KeyID toID) :
	m_fromID(fromID),
	m_toID(toID)
{
}

KeyRemapConfig::TapRule::TapRule() :
	m_fromID(kKeyNone),
	m_aloneID(kKeyNone),
	m_holdID(kKeyNone)
{
}

KeyRemapConfig::TapRule::TapRule(KeyID fromID, KeyID aloneID,
		KeyID holdID) :
	m_fromID(fromID),
	m_aloneID(aloneID),
	m_holdID(holdID)
{
}

KeyRemapConfig::ChordRule::ChordRule() :
	m_fromMask(0),
	m_fromID(kKeyNone),
	m_toMask(0),
	m_toID(kKeyNone)
{
}

KeyRemapConfig::ChordRule::ChordRule(KeyModifierMask fromMask, KeyID fromID,
		KeyModifierMask toMask, KeyID toID) :
	m_fromMask(fromMask),
	m_fromID(fromID),
	m_toMask(toMask),
	m_toID(toID)
{
}

bool
operator==(const KeyRemapConfig::KeyRule& a, const KeyRemapConfig::KeyRule& b)
{
	return a.m_fromID == b.m_fromID && a.m_toID == b.m_toID;
}

bool
operator==(const KeyRemapConfig::TapRule& a, const KeyRemapConfig::TapRule& b)
{
	return a.m_fromID == b.m_fromID &&
		a.m_aloneID == b.m_aloneID &&
		a.m_holdID == b.m_holdID;
}

bool
operator==(const KeyRemapConfig::ChordRule& a, const KeyRemapConfig::ChordRule& b)
{
	return a.m_fromMask == b.m_fromMask &&
		a.m_fromID == b.m_fromID &&
		a.m_toMask == b.m_toMask &&
		a.m_toID == b.m_toID;
}

std::string
KeyRemapConfig::normalizeScreen(const std::string& screen)
{
	std::string normalized(screen);
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return normalized;
}

void
KeyRemapConfig::addRule(const std::string& screen, KeyID fromID, KeyID toID)
{
	m_keyRules[normalizeScreen(screen)].push_back(KeyRule(fromID, toID));
}

void
KeyRemapConfig::addTapRule(const std::string& screen, KeyID fromID,
		KeyID aloneID, KeyID holdID)
{
	m_tapRules[normalizeScreen(screen)].push_back(
		TapRule(fromID, aloneID, holdID));
}

void
KeyRemapConfig::addChordRule(const std::string& screen,
		KeyModifierMask fromMask, KeyID fromID,
		KeyModifierMask toMask, KeyID toID)
{
	m_chordRules[normalizeScreen(screen)].push_back(
		ChordRule(fromMask, fromID, toMask, toID));
}

void
KeyRemapConfig::clear()
{
	m_keyRules.clear();
	m_tapRules.clear();
	m_chordRules.clear();
}

const KeyRemapConfig::KeyRule*
KeyRemapConfig::findRule(const std::string& screen, KeyID id) const
{
	ScreenKeyRules::const_iterator rules =
		m_keyRules.find(normalizeScreen(screen));
	if (rules == m_keyRules.end()) {
		return nullptr;
	}

	for (KeyRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return nullptr;
}

const KeyRemapConfig::TapRule*
KeyRemapConfig::findTapRule(const std::string& screen, KeyID id) const
{
	ScreenTapRules::const_iterator rules =
		m_tapRules.find(normalizeScreen(screen));
	if (rules == m_tapRules.end()) {
		return nullptr;
	}

	for (TapRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return nullptr;
}

const KeyRemapConfig::ChordRule*
KeyRemapConfig::findChordRule(const std::string& screen, KeyID id,
		KeyModifierMask mask) const
{
	ScreenChordRules::const_iterator rules =
		m_chordRules.find(normalizeScreen(screen));
	if (rules == m_chordRules.end()) {
		return nullptr;
	}

	const ChordRule* best = nullptr;
	std::int32_t bestModifiers = -1;
	for (ChordRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID != id ||
			(mask & commandModifierMask()) != rule->m_fromMask) {
			continue;
		}

		std::int32_t modifiers = countModifiers(rule->m_fromMask);
		if (modifiers > bestModifiers) {
			best = &*rule;
			bestModifiers = modifiers;
		}
	}

	return best;
}

bool
KeyRemapConfig::empty() const
{
	return m_keyRules.empty() && m_tapRules.empty() && m_chordRules.empty();
}

void
KeyRemapConfig::write(std::ostream& out) const
{
	for (ScreenKeyRules::const_iterator screen = m_keyRules.begin();
			screen != m_keyRules.end(); ++screen) {
		out << "\t" << screen->first << ":\n";
		for (KeyRuleList::const_iterator rule = screen->second.begin();
				rule != screen->second.end(); ++rule) {
			out << "\t\t" << KeyMap::formatKey(rule->m_fromID, 0)
				<< " = " << KeyMap::formatKey(rule->m_toID, 0) << "\n";
		}

		ScreenTapRules::const_iterator tapScreen = m_tapRules.find(screen->first);
		if (tapScreen != m_tapRules.end()) {
			for (TapRuleList::const_iterator rule = tapScreen->second.begin();
					rule != tapScreen->second.end(); ++rule) {
				out << "\t\t" << KeyMap::formatKey(rule->m_fromID, 0)
					<< ".alone = " << KeyMap::formatKey(rule->m_aloneID, 0) << "\n";
				out << "\t\t" << KeyMap::formatKey(rule->m_fromID, 0)
					<< ".hold = " << KeyMap::formatKey(rule->m_holdID, 0) << "\n";
			}
		}

		ScreenChordRules::const_iterator chordScreen =
			m_chordRules.find(screen->first);
		if (chordScreen != m_chordRules.end()) {
			for (ChordRuleList::const_iterator rule = chordScreen->second.begin();
					rule != chordScreen->second.end(); ++rule) {
				out << "\t\t" << KeyMap::formatKey(rule->m_fromID,
						rule->m_fromMask)
					<< " = " << KeyMap::formatKey(rule->m_toID,
						rule->m_toMask) << "\n";
			}
		}
	}

	for (ScreenTapRules::const_iterator screen = m_tapRules.begin();
			screen != m_tapRules.end(); ++screen) {
		if (m_keyRules.find(screen->first) != m_keyRules.end()) {
			continue;
		}

		out << "\t" << screen->first << ":\n";
		for (TapRuleList::const_iterator rule = screen->second.begin();
				rule != screen->second.end(); ++rule) {
			out << "\t\t" << KeyMap::formatKey(rule->m_fromID, 0)
				<< ".alone = " << KeyMap::formatKey(rule->m_aloneID, 0) << "\n";
				out << "\t\t" << KeyMap::formatKey(rule->m_fromID, 0)
					<< ".hold = " << KeyMap::formatKey(rule->m_holdID, 0) << "\n";
		}
	}

	for (ScreenChordRules::const_iterator screen = m_chordRules.begin();
			screen != m_chordRules.end(); ++screen) {
		if (m_keyRules.find(screen->first) != m_keyRules.end() ||
			m_tapRules.find(screen->first) != m_tapRules.end()) {
			continue;
		}

		out << "\t" << screen->first << ":\n";
		for (ChordRuleList::const_iterator rule = screen->second.begin();
				rule != screen->second.end(); ++rule) {
			out << "\t\t" << KeyMap::formatKey(rule->m_fromID,
					rule->m_fromMask)
				<< " = " << KeyMap::formatKey(rule->m_toID,
					rule->m_toMask) << "\n";
		}
	}
}

bool
KeyRemapConfig::operator==(const KeyRemapConfig& config) const
{
	return m_keyRules == config.m_keyRules &&
		m_tapRules == config.m_tapRules &&
		m_chordRules == config.m_chordRules;
}

bool
KeyRemapConfig::operator!=(const KeyRemapConfig& config) const
{
	return !operator==(config);
}

} // namespace inputleap
