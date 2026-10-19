// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_PRESENCEVALIDATION_H
#define CHIAKI_DISCORDPRESENCE_PRESENCEVALIDATION_H

#include <QString>

// Everything coming from the console or from the metadata site is checked here
// before it reaches Discord or the cache file.
namespace PresenceValidation
{

// Four uppercase letters and five digits, e.g. PPSA03977, CUSA00265, NPXS39041.
inline bool IsValidTitleId(const QString &s)
{
	if(s.size() != 9)
		return false;
	for(int i = 0; i < 4; i++)
		if(s[i] < 'A' || s[i] > 'Z')
			return false;
	for(int i = 4; i < 9; i++)
		if(s[i] < '0' || s[i] > '9')
			return false;
	return true;
}

// Only retail PS5 (PPSA) and PS4 (CUSA) titles are known to the metadata sites.
inline bool IsRetailTitleId(const QString &s)
{
	return IsValidTitleId(s) && (s.startsWith("PPSA") || s.startsWith("CUSA"));
}

inline bool IsSafeHttpsUrl(const QString &s)
{
	const QString prefix = QStringLiteral("https://");
	if(s.size() < prefix.size() + 3 || s.size() > 1024 || !s.startsWith(prefix))
		return false;
	for(QChar c : s)
		if(c.unicode() < 0x20 || c.unicode() == 0x7f)
			return false;
	int end = prefix.size();
	while(end < s.size() && s[end] != '/' && s[end] != '?' && s[end] != '#')
		end++;
	QString host = s.mid(prefix.size(), end - prefix.size());
	return !host.isEmpty() && !host.contains('@') && !host.contains(' ') && !host.contains('\\');
}

inline QString SanitizeText(const QString &s, int max_chars)
{
	QString cleaned;
	cleaned.reserve(s.size());
	for(QChar c : s)
		if(c.category() != QChar::Other_Control)
			cleaned.append(c);
	return cleaned.trimmed().left(max_chars);
}

}

#endif // CHIAKI_DISCORDPRESENCE_PRESENCEVALIDATION_H
