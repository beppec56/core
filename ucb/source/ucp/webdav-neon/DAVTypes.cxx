/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */


#include "osl/time.h"

#include "DAVTypes.hxx"
#include "../inc/urihelper.hxx"
#include "NeonUri.hxx"

using namespace webdav_ucp;
using namespace com::sun::star;

// DAVCapabilities implementation

DAVCapabilities::DAVCapabilities() :
        m_isResourceFound( false ),
        m_isDAVCapabilitiesValid( false ),
        m_isClass1( false ),
        m_isClass2( false ),
        m_isClass3( false ),
        m_hasFPServerExtensions( false ),
        m_aAllowedMethods()
{
}


DAVCapabilities::DAVCapabilities( const DAVCapabilities & rOther ) :
        m_isResourceFound( rOther.m_isResourceFound ),
        m_isDAVCapabilitiesValid( rOther.m_isDAVCapabilitiesValid ),
        m_isClass1( rOther.m_isClass2 ),
        m_isClass2( rOther.m_isClass2 ),
        m_isClass3( rOther.m_isClass3 ),
        m_hasFPServerExtensions( rOther.m_hasFPServerExtensions ),
        m_aAllowedMethods( rOther.m_aAllowedMethods )
{
}


DAVCapabilities::~DAVCapabilities()
{
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
