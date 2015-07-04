/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_TOOLS_DEBUGLOGGER_HXX
#define INCLUDED_TOOLS_DEBUGLOGGER_HXX

#include <tools/toolsdllapi.h>

#include <sal/detail/log.h>
#include <sal/types.h>

namespace tools {
    TOOLS_DLLPUBLIC void initDebugLog();
    TOOLS_DLLPUBLIC void writeDebugLogDefaultName();
    TOOLS_DLLPUBLIC void writeDebugLog(char const* fileName);
    TOOLS_DLLPUBLIC void writeDebugLogDefaultNameIfNotEmpty();
    TOOLS_DLLPUBLIC void writeDebugLogIfNotEmpty(char const* fileName);
}


#endif
