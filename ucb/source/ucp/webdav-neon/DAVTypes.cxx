/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#include "osl/time.h"

#include "DAVTypes.hxx"
#include "../inc/urihelper.hxx"
#include "NeonUri.hxx"

using namespace webdav_ucp;
using namespace com::sun::star;

// DAVOptions implementation

DAVOptions::DAVOptions() :
    m_isClass1( false ),
    m_isClass2( false ),
    m_isClass3( false ),
    m_isHeadAllowed( true ),
    m_isLocked( false ),
    m_aAllowedMethods(),
    m_nStaleTime( 0 ),
    m_nRequestedTimeLife( 0 ),
    m_rURL( std::make_shared<DavURLObject>( DavURLObject() ) ),
    m_sRedirectedURL(),
    m_nHttpResponseStatusCode( 0 ),
    m_sHttpResponseStatusText()
{
    SAL_INFO( "ucb.ucp.webdav", "DAVOptions() ctor URL: <" <<m_rURL->GetMainURL() << ">");
}

DAVOptions::DAVOptions( const DAVOptions & rOther ) :
    m_isClass1( rOther.m_isClass1 ),
    m_isClass2( rOther.m_isClass2 ),
    m_isClass3( rOther.m_isClass3 ),
    m_isHeadAllowed( rOther.m_isHeadAllowed ),
    m_isLocked( rOther.m_isLocked ),
    m_aAllowedMethods( rOther.m_aAllowedMethods ),
    m_nStaleTime( rOther.m_nStaleTime ),
    m_nRequestedTimeLife( rOther.m_nRequestedTimeLife ),
    m_rURL( std::make_shared<DavURLObject>( *(rOther.m_rURL) ) ),
    m_sRedirectedURL( rOther.m_sRedirectedURL),
    m_nHttpResponseStatusCode( rOther.m_nHttpResponseStatusCode ),
    m_sHttpResponseStatusText( rOther.m_sHttpResponseStatusText )
{
    SAL_INFO( "ucb.ucp.webdav", "DAVOptions() COPY ctor URL: <" <<m_rURL->GetMainURL() << ">");
}

DAVOptions::~DAVOptions()
{
    SAL_INFO( "ucb.ucp.webdav", "DAVOptions() dtor URL: <" <<m_rURL->GetMainURL()  << ">");
}

DAVOptions & DAVOptions::operator=( const DAVOptions& rOpts )
{
    SAL_INFO("ucb.ucp.webdav","DAVOptions::operator=");
    m_isClass1 = rOpts.m_isClass1;
    m_isClass2 = rOpts.m_isClass2;
    m_isClass3 = rOpts.m_isClass3;
    m_isLocked = rOpts.m_isLocked;
    m_isHeadAllowed = rOpts.m_isHeadAllowed;
    m_aAllowedMethods = rOpts.m_aAllowedMethods;
    m_nStaleTime = rOpts.m_nStaleTime;
    m_nRequestedTimeLife = rOpts.m_nRequestedTimeLife;
    m_rURL = std::make_shared<DavURLObject>( *rOpts.m_rURL );
    m_sRedirectedURL = rOpts.m_sRedirectedURL;
    m_nHttpResponseStatusCode = rOpts.m_nHttpResponseStatusCode;
    m_sHttpResponseStatusText = rOpts.m_sHttpResponseStatusText;
    return *this;
}

bool DAVOptions::operator==( const DAVOptions& rOpts ) const
{
    bool ret =
        m_isClass1 == rOpts.m_isClass1 &&
        m_isClass2 == rOpts.m_isClass2 &&
        m_isClass3 == rOpts.m_isClass3 &&
        m_isLocked == rOpts.m_isLocked &&
        m_isHeadAllowed == rOpts.m_isHeadAllowed &&
        m_aAllowedMethods == rOpts.m_aAllowedMethods &&
        m_nStaleTime == rOpts.m_nStaleTime &&
        m_nRequestedTimeLife == rOpts.m_nRequestedTimeLife &&
        *m_rURL == *rOpts.m_rURL &&
        m_sRedirectedURL == rOpts.m_sRedirectedURL &&
        m_nHttpResponseStatusCode == rOpts.m_nHttpResponseStatusCode &&
        m_sHttpResponseStatusText == rOpts.m_sHttpResponseStatusText;

    std::shared_ptr<std::string> log = toString();
    std::shared_ptr<std::string> logOther = rOpts.toString();
    SAL_INFO( "ucb.ucp.webdav", "operator==\n" << *log   << "\nOther:\n" << *logOther << "\nret = " << ret);
    return ret;
}


std::shared_ptr<std::string> DAVOptions::toString() const
{
    std::stringstream logstring;

    logstring << "m_isClass1: " << m_isClass1;
    logstring << ", m_isClass2: " <<          m_isClass2;
    logstring << ", m_isClass3: " <<          m_isClass3;
    logstring << ", m_isHeadAllowed: " <<     m_isHeadAllowed;
    logstring << ", m_isLocked: " <<          m_isLocked << std::endl;
    logstring << "m_aAllowedMethods: " <<  m_aAllowedMethods;
    logstring << ", m_nStaleTime: " <<       m_nStaleTime  << std::endl;
    logstring << "m_rURL <" <<              m_rURL->GetMainURL() << ">" << std::endl;
    logstring << "m_sRedirectedURL <" <<    m_sRedirectedURL << ">" << std::endl;
    logstring << "m_nHttpResponseStatusCode: " <<m_nHttpResponseStatusCode;
    logstring << ", m_sHttpResponseStatusText: " <<m_sHttpResponseStatusText;
    TimeValue t1;
    osl_getSystemTime( &t1 );
    logstring << ", lifetime: " << (m_nStaleTime - t1.Seconds);

    std::shared_ptr<std::string>ret;
    ret.reset( new std::string( logstring.str() ) );
    return ret;
}

// DAVOptionsCache implementation

DAVOptionsCache::DAVOptionsCache()
{
}

DAVOptionsCache::~DAVOptionsCache()
{
}

bool DAVOptionsCache::getDAVOptions( const OUString & rURL, DAVOptions & rDAVOptions )
{
    osl::MutexGuard aGuard( m_aMutex );
    DavURLObject aDavURLObject( rURL );
    aDavURLObject.removeFinalSlash();
    OUString aEncodedUrl( aDavURLObject.GetMainURL() );

    // search the URL in the static map
    DAVOptionsMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it == m_aTheCache.end() )
    {
        SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::getDAVOptions - not found:  URL <"<< rURL <<">");
        return false;
    }
    else
    {
        // check if the capabilities are stale, before restoring
        TimeValue t1;
        osl_getSystemTime( &t1 );
        if ( (*it).second.getStaleTime() < t1.Seconds )
        {
            // if stale, remove from cache, do not restore
            m_aTheCache.erase( it );
            SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::getDAVOptions - removed stale:  URL <"<< rURL <<">");
            return false;
            // return false instead
        }
        rDAVOptions = (*it).second;

        std::shared_ptr<std::string> log = rDAVOptions.toString();
        SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::getDAVOptions - got: "<< *log);
        return true;
    }
}

void DAVOptionsCache::removeDAVOptions( const OUString & rURL )
{
    osl::MutexGuard aGuard( m_aMutex );
    DavURLObject aDavURLObject( rURL );
    aDavURLObject.removeFinalSlash();
    OUString aEncodedUrl( aDavURLObject.GetMainURL() );

    DAVOptionsMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it != m_aTheCache.end() )
    {
        m_aTheCache.erase( it );
        SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::removeDAVOptions - removed:  URL <"<< rURL <<">");
    }
}

void DAVOptionsCache::addDAVOptions( DAVOptions & rDAVOptions, const sal_uInt32 nLifeTime )
{
    osl::MutexGuard aGuard( m_aMutex );
    rDAVOptions.getURL()->removeFinalSlash();
    OUString aEncodedUrl( rDAVOptions.getURL()->GetMainURL() );

    // check if already cached
    DAVOptionsMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it != m_aTheCache.end() )
    { // already in cache, check LifeTime
        if ( (*it).second.getRequestedTimeLife() == nLifeTime )
        {
            SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::addDAVOptions - already cached, not added, URL <"<< aEncodedUrl <<">");
            return; // same lifetime, do nothing
        }
    }
    // not in cache, add it
    TimeValue t1;
    osl_getSystemTime( &t1 );
    rDAVOptions.setStaleTime( t1.Seconds + nLifeTime );

    m_aTheCache[ aEncodedUrl ] = rDAVOptions;
    std::shared_ptr<std::string> log = m_aTheCache[ aEncodedUrl ].toString();
    SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::addDAVOptions - added: "<< *log);
}

void DAVOptionsCache::setHeadAllowed( const OUString & rURL, const bool HeadAllowed )
{
    osl::MutexGuard aGuard( m_aMutex );
    DavURLObject aDavURLObject( rURL );
    aDavURLObject.removeFinalSlash();
    OUString aEncodedUrl( aDavURLObject.GetMainURL() );

    DAVOptionsMap::iterator it;
    it = m_aTheCache.find( aEncodedUrl );
    if ( it != m_aTheCache.end() )
    {
        // first check for stale
        TimeValue t1;
        osl_getSystemTime( &t1 );
        if( (*it).second.getStaleTime() < t1.Seconds )
        {
            SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::setHeadAllowed - removed stale:  URL <"<< rURL <<">");
            m_aTheCache.erase( it );
            return;
        }
        // check if the resource was present on server
        (*it).second.setHeadAllowed( HeadAllowed );
    }
    else
        SAL_INFO("ucb.ucp.webdav","DAVOptionsCache::setHeadAllowed - NOT FOUND:  URL <"<< rURL <<">");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
