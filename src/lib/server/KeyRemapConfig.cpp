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

#include "server/KeyRemapConfig.h"

#include "barrier/KeyMap.h"

#include <algorithm>
#include <cctype>
#include <ostream>

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

KeyRemapConfig
KeyRemapConfig::makeDefault()
{
	KeyRemapConfig config;
	config.addRule("mac", kKeyAlt_R, kKeySuper_R);
	config.addTapRule("mac", kKeySuper_R, kKeyF19, kKeySuper_R);
	config.addRule("windows", kKeySuper_L, kKeyControl_L);
	return config;
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
KeyRemapConfig::clear()
{
	m_keyRules.clear();
	m_tapRules.clear();
}

const KeyRemapConfig::KeyRule*
KeyRemapConfig::findRule(const std::string& screen, KeyID id) const
{
	ScreenKeyRules::const_iterator rules =
		m_keyRules.find(normalizeScreen(screen));
	if (rules == m_keyRules.end()) {
		return NULL;
	}

	for (KeyRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return NULL;
}

const KeyRemapConfig::TapRule*
KeyRemapConfig::findTapRule(const std::string& screen, KeyID id) const
{
	ScreenTapRules::const_iterator rules =
		m_tapRules.find(normalizeScreen(screen));
	if (rules == m_tapRules.end()) {
		return NULL;
	}

	for (TapRuleList::const_iterator rule = rules->second.begin();
			rule != rules->second.end(); ++rule) {
		if (rule->m_fromID == id) {
			return &*rule;
		}
	}

	return NULL;
}

bool
KeyRemapConfig::empty() const
{
	return m_keyRules.empty() && m_tapRules.empty();
}

void
KeyRemapConfig::write(std::ostream& out) const
{
	for (ScreenKeyRules::const_iterator screen = m_keyRules.begin();
			screen != m_keyRules.end(); ++screen) {
		out << "\t" << screen->first << ":\n";
		for (KeyRuleList::const_iterator rule = screen->second.begin();
				rule != screen->second.end(); ++rule) {
			out << "\t\t" << barrier::KeyMap::formatKey(rule->m_fromID, 0)
				<< " = " << barrier::KeyMap::formatKey(rule->m_toID, 0) << "\n";
		}

		ScreenTapRules::const_iterator tapScreen = m_tapRules.find(screen->first);
		if (tapScreen != m_tapRules.end()) {
			for (TapRuleList::const_iterator rule = tapScreen->second.begin();
					rule != tapScreen->second.end(); ++rule) {
				out << "\t\t" << barrier::KeyMap::formatKey(rule->m_fromID, 0)
					<< ".alone = " << barrier::KeyMap::formatKey(rule->m_aloneID, 0) << "\n";
				out << "\t\t" << barrier::KeyMap::formatKey(rule->m_fromID, 0)
					<< ".hold = " << barrier::KeyMap::formatKey(rule->m_holdID, 0) << "\n";
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
			out << "\t\t" << barrier::KeyMap::formatKey(rule->m_fromID, 0)
				<< ".alone = " << barrier::KeyMap::formatKey(rule->m_aloneID, 0) << "\n";
			out << "\t\t" << barrier::KeyMap::formatKey(rule->m_fromID, 0)
				<< ".hold = " << barrier::KeyMap::formatKey(rule->m_holdID, 0) << "\n";
		}
	}
}

bool
KeyRemapConfig::operator==(const KeyRemapConfig& config) const
{
	return m_keyRules == config.m_keyRules && m_tapRules == config.m_tapRules;
}

bool
KeyRemapConfig::operator!=(const KeyRemapConfig& config) const
{
	return !operator==(config);
}
