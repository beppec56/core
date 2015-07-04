/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#include <config_features.h>
#include <config_folders.h>

#include <rtl/bootstrap.hxx>
#include <com/sun/star/frame/DoubleInitializationException.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/io/XTruncate.hpp>

#include <comphelper_module.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper_services.hxx>
#include <cppuhelper/supportsservice.hxx>

#include "debuglogring.hxx"

using namespace ::com::sun::star;

namespace comphelper
{


ODebugLogRing::ODebugLogRing()
: m_aMessages( DEBUGLOGRING_SIZE )
, m_bInitialized( false )
, m_bFull( false )
, m_nPos( 0 )
{
}


ODebugLogRing::~ODebugLogRing()
{
}


uno::Sequence< OUString > SAL_CALL ODebugLogRing::getSupportedServiceNames_static()
{
    uno::Sequence< OUString > aResult( 1 );
    aResult[0] = getServiceName_static();
    return aResult;
}


OUString SAL_CALL ODebugLogRing::getImplementationName_static()
{
    return OUString( "com.sun.star.comp.logging.DebugLogRing" );
}


OUString SAL_CALL ODebugLogRing::getSingletonName_static()
{
    return OUString( "com.sun.star.logging.SingleDebugLogRing" );
}


OUString SAL_CALL ODebugLogRing::getServiceName_static()
{
    return OUString( "com.sun.star.logging.DebugLogRing" );
}


uno::Reference< uno::XInterface > SAL_CALL ODebugLogRing::Create( SAL_UNUSED_PARAMETER  const uno::Reference< uno::XComponentContext >& )
{
    return static_cast< cppu::OWeakObject* >( new ODebugLogRing );
}

// XDebugLogRing implementation

void SAL_CALL ODebugLogRing::logString( const OUString& aMessage ) throw (uno::RuntimeException, std::exception)
{
    ::osl::MutexGuard aGuard( m_aMutex );

    m_aMessages[m_nPos] = aMessage;
    if ( ++m_nPos >= m_aMessages.getLength() )
    {
        m_nPos = 0;
        m_bFull = true;
    }

    // if used once then default initialized
    m_bInitialized = true;
}


uno::Sequence< OUString > SAL_CALL ODebugLogRing::getCollectedLog() throw (uno::RuntimeException, std::exception)
{
    ::osl::MutexGuard aGuard( m_aMutex );

    sal_Int32 nResLen = m_bFull ? m_aMessages.getLength() : m_nPos;
    sal_Int32 nStart = m_bFull ? m_nPos : 0;
    uno::Sequence< OUString > aResult( nResLen );

    for ( sal_Int32 nInd = 0; nInd < nResLen; nInd++ )
        aResult[nInd] = m_aMessages[ ( nStart + nInd ) % m_aMessages.getLength() ];

    // if used once then default initialized
    m_bInitialized = true;

    return aResult;
}

sal_Bool SAL_CALL ODebugLogRing::isEmpty() throw (uno::RuntimeException, std::exception)
{
    ::osl::MutexGuard aGuard( m_aMutex );

    if(m_nPos == 0 && !m_bFull)
        return true;
    return false;
}

namespace
{
    void lcl_writeStringInStream( const uno::Reference< io::XOutputStream >& xOutStream, const OUString& aString )
    {
        if ( xOutStream.is() )
        {
            OString aStrLog = ::rtl::OUStringToOString( aString, RTL_TEXTENCODING_UTF8 );
            uno::Sequence< sal_Int8 > aLogData( (const sal_Int8*)aStrLog.getStr(), aStrLog.getLength() );
            xOutStream->writeBytes( aLogData );
        }
    }
}

// ----------------------------------------------------------
void SAL_CALL ODebugLogRing::flushLog( const OUString& aFileName ) throw (uno::RuntimeException, std::exception)
{
    ::osl::MutexGuard aGuard( m_aMutex );

    OUString aFileURL = ( "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE("bootstrap") ":UserInstallation}"  );
    ::rtl::Bootstrap::expandMacros( aFileURL );
    OUString aBuildID = ( "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE("setup") ":buildid}"  );
    ::rtl::Bootstrap::expandMacros( aBuildID );

    if ( aFileURL.getLength() )
    {
        aFileURL += OUString( "/user/temp/" );
        aFileURL += aFileName;
        try
        {
            uno::Reference< uno::XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
            uno::Reference< ucb::XSimpleFileAccess3 > xSimpleFileAccess(ucb::SimpleFileAccess::create(xContext));
            uno::Reference< io::XStream > xStream( xSimpleFileAccess->openFileReadWrite( aFileURL ), uno::UNO_SET_THROW );
            uno::Reference< io::XOutputStream > xOutStream( xStream->getOutputStream(), uno::UNO_SET_THROW );
            uno::Reference< io::XTruncate > xTruncate( xOutStream, uno::UNO_QUERY_THROW );

            xTruncate->truncate();

            if ( aBuildID.getLength() )
            {
                aBuildID += OUString( "\n" );
                lcl_writeStringInStream( xOutStream, aBuildID );
            }

            sal_Int32 nResLen = m_bFull ? m_aMessages.getLength() : m_nPos;
            sal_Int32 nStart = m_bFull ? m_nPos : 0;
            uno::Sequence< OUString > aResult( nResLen );

            for ( sal_Int32 nInd = 0; nInd < nResLen; nInd++ )
                lcl_writeStringInStream( xOutStream, m_aMessages[ ( nStart + nInd ) % m_aMessages.getLength() ] );
            xOutStream->flush();
        }
        catch( uno::Exception& )
        {}
    }
}

// XInitialization

void SAL_CALL ODebugLogRing::initialize( const uno::Sequence< uno::Any >& aArguments ) throw (uno::Exception, uno::RuntimeException, std::exception)
{
    ::osl::MutexGuard aGuard( m_aMutex );
    if ( m_bInitialized )
        throw frame::DoubleInitializationException();

    if ( !m_refCount )
        throw uno::RuntimeException(); // the object must be refcounted already!

    if (aArguments.hasElements())
    {
        sal_Int32 nLen = 0;
        if ( aArguments.getLength() == 1 && ( aArguments[0] >>= nLen ) && nLen )
            m_aMessages.realloc( nLen );
        else
            throw lang::IllegalArgumentException(
                "Nonnull size is expected as the first argument!",
                uno::Reference< uno::XInterface >(),
                0 );
    }

    m_bInitialized = true;
}

// XServiceInfo
OUString SAL_CALL ODebugLogRing::getImplementationName() throw (uno::RuntimeException, std::exception)
{
    return getImplementationName_static();
}

sal_Bool SAL_CALL ODebugLogRing::supportsService( const OUString& aServiceName ) throw (uno::RuntimeException, std::exception)
{
    return cppu::supportsService(this, aServiceName);
}

uno::Sequence< OUString > SAL_CALL ODebugLogRing::getSupportedServiceNames() throw (uno::RuntimeException, std::exception)
{
    return getSupportedServiceNames_static();
}

} // namespace comphelper

void createRegistryInfo_ODebugLogRing()
{
    static ::comphelper::module::OAutoRegistration< ::comphelper::ODebugLogRing > aAutoRegistration;
    static ::comphelper::module::OSingletonRegistration< ::comphelper::ODebugLogRing > aSingletonRegistration;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
