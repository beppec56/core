/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "DAVUri.hxx"

using namespace webdav_ucp;

DAVUri::DAVUri()
    : m_TheURL( INetURLObject() )
{
}

DAVUri::DAVUri( const OUString  & TheAbsRef )
    : m_TheURL( INetURLObject( TheAbsRef ) )
{

}

DAVUri::DAVUri( const DAVUri & rOther )
    : m_TheURL( rOther.m_TheURL )
{
}

DAVUri::~DAVUri()
{
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
