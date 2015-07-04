/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_COMPHELPER_DEBUGLOGGER_HXX
#define INCLUDED_COMPHELPER_DEBUGLOGGER_HXX

#include <sal/detail/log.h>
#include <sal/types.h>
#include <rtl/ustring.hxx>
#include <comphelper/comphelperdllapi.h>

namespace comphelper {
    COMPHELPER_DLLPUBLIC void initDebugLog();
    COMPHELPER_DLLPUBLIC void createAbsoluteLogFileSystemName(OUString & inFileNameRadix,  OUString & aSystemPathReturned, bool createAbsoluteLogFileSystemName );
    COMPHELPER_DLLPUBLIC void deleteOlderLogFiles(int nFileAgeInHours, OUString & sFileNameRadix);
    COMPHELPER_DLLPUBLIC void writeDebugLogDefaultName();
    COMPHELPER_DLLPUBLIC void writeDebugLog(char const* fileName);
    COMPHELPER_DLLPUBLIC void writeDebugLogDefaultNameIfNotEmpty();
    COMPHELPER_DLLPUBLIC void writeDebugLogIfNotEmpty(char const* fileName);
}


#endif
