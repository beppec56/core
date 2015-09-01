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
        m_bResourceFound( false ),
        m_bDAVCapabilitiesValid( false ),
        m_bClass1( false ),
        m_bClass2( false ),
        m_bClass3( false ),
        m_bFPServerExtensions( false )
{
}


DAVCapabilities::DAVCapabilities( const DAVCapabilities & rOther ) :
        m_bResourceFound( rOther.m_bResourceFound ),
        m_bDAVCapabilitiesValid( rOther.m_bDAVCapabilitiesValid ),
        m_bClass1( rOther.m_bClass2 ),
        m_bClass2( rOther.m_bClass2 ),
        m_bClass3( rOther.m_bClass3 ),
        m_bFPServerExtensions( rOther.m_bFPServerExtensions )
{
}


DAVCapabilities::~DAVCapabilities()
{
}


// DAVCapabilitiesCache implementation

DAVCapabilitiesCache::DAVCapabilitiesCache()
{
}

DAVCapabilitiesCache::~DAVCapabilitiesCache()
{
}

bool DAVCapabilitiesCache::restoreDAVCapabilities( const OUString & rURL, DAVCapabilities & rDAVCapabilities )
{
    osl::MutexGuard aGuard( m_aMutex );
    OUString aEncodedUrl( ucb_impl::urihelper::encodeURI( NeonUri::unescape( rURL ) ) );
    // static std::map< std::string, std::unique_ptr< DAVCapabilities > > aDAVCapabilitiesCache;
    // search the URL in the static map
    DAVCapabilitiesMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it == m_aTheCache.end() )
        return false;
    else
    {
        // check if the capabilities are stale, before restoring
        TimeValue t1;
        osl_getSystemTime( &t1 );
        if ( (*it).second.m_nStaleTime < t1.Seconds )
        {
            // if stale, remove from cache, do not restore
            removeDAVCapabilities( rURL );
            return false;
            // return false instead
        }
        rDAVCapabilities = (*it).second;
        return true;
    }
}


void DAVCapabilitiesCache::removeDAVCapabilities( const OUString & rURL )
{
    osl::MutexGuard aGuard( m_aMutex );
    OUString aEncodedUrl( ucb_impl::urihelper::encodeURI( NeonUri::unescape( rURL ) ) );

    DAVCapabilitiesMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it != m_aTheCache.end() )
    {
        m_aTheCache.erase( it );
    }
}


void DAVCapabilitiesCache::addDAVCapabilities( const OUString & rURL, DAVCapabilities & rDAVCapabilities )
{
    osl::MutexGuard aGuard( m_aMutex );
    OUString aEncodedUrl( ucb_impl::urihelper::encodeURI( NeonUri::unescape( rURL ) ) );
    rDAVCapabilities.m_sURL = aEncodedUrl;

    // set max age to 1 minute
    TimeValue t1;
    osl_getSystemTime( &t1 );
    rDAVCapabilities.m_nStaleTime = t1.Seconds + 60;

    m_aTheCache[ aEncodedUrl ] = rDAVCapabilities;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
