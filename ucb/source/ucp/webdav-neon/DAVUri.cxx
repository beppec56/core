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

void DAVUri::AppendPath ( const OUString& rPath )
{
    m_TheURL.Append( rPath );
}

// void DAVUri::SetScheme ( const OUString& rScheme )
// {
//     // create a new INetUrlObject using the current, but changing the protocols

//     INetURLObject aURL = m_TheURL;


// }

// static
OUString DAVUri::escapeSegment( const OUString& rTheSegment )
{
    // private base class function member
    return INetURLObject::encode( rTheSegment, INetURLObject::PART_PCHAR, INetURLObject::WAS_ENCODED );
}

// static
// unescape
OUString DAVUri::unescape( const OUString& rTheSegment )
{
    // private base class function member
    return INetURLObject::decode( rTheSegment, INetURLObject::DECODE_UNAMBIGUOUS );
}

//static
OUString DAVUri::makeConnectionEndPointString( const OUString & rHostName,
                                               int nPort )
{
    OUStringBuffer aBuf;

    // Is host a numeric IPv6 address?
    if ( ( rHostName.indexOf( ':' ) != -1 ) &&
         ( rHostName[ 0 ] != '[' ) )
    {
        aBuf.append( "[" );
        aBuf.append( rHostName );
        aBuf.append( "]" );
    }
    else
    {
        aBuf.append( rHostName );
    }

    if ( ( nPort != DEFAULT_HTTP_PORT ) && ( nPort != DEFAULT_HTTPS_PORT ) )
    {
        aBuf.append( ":" );
        aBuf.append( OUString::number( nPort ) );
    }
    return aBuf.makeStringAndClear();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
