/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <comphelper/processfactory.hxx>
#include <com/sun/star/logging/XDebugLogRing.hpp>
#include <com/sun/star/logging/DebugLogRing.hpp>
#include <com/sun/star/logging/SingleDebugLogRing.hpp>

#include <stdio.h> // snprintf, vsnprintf

#include "osl/time.h"

#include "tools/debuglogger.hxx"

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

namespace tools
{
    static uno::Reference< logging::XDebugLogRing > m_xDebugLogRing;
    static osl::Mutex   aMutex;

//    static pfunc_osl_printDebugMessage pOriginal_osl_DebugMessageFunc = NULL;

    static void sendLoggerLineMessage(char const* pszMessage)
    {

        if ( !m_xDebugLogRing.is() )
        {
            try
            {
                uno::Reference<XComponentContext> xContext( ::comphelper::getProcessComponentContext() );
                m_xDebugLogRing.set( logging::SingleDebugLogRing::get(xContext) );
            }
            catch( const uno::Exception& )
            {}
        }

        if ( m_xDebugLogRing.is() )
        {
            OUString aMessage(pszMessage, std::strlen(pszMessage), RTL_TEXTENCODING_ASCII_US);
            m_xDebugLogRing->logString( aMessage );
        }
    }

    void initDebugLog()
    {
        osl_setLogMessageFunc(sendLoggerLineMessage);
    }

    static void writeLog(char const* fileName, sal_Bool writeAlways)
    {
        oslDateTime currentDate;
        TimeValue   currentTime;
        osl_getSystemTime(&currentTime);
        osl_getDateTimeFromTimeValue( &currentTime, &currentDate );
        osl::MutexGuard aGuard( aMutex );
        // prepend absolute datetime time to file name
        char buff[1024];
        snprintf( buff, sizeof buff, "%04d%02d%02d-%02d%02d%02d-%s",
                           currentDate.Year,currentDate.Month,currentDate.Day,
                           currentDate.Hours, currentDate.Minutes, currentDate.Seconds,
                           fileName );

        if (! m_xDebugLogRing.is() )
        {
            try
            {
                Reference<XComponentContext> xContext( ::comphelper::getProcessComponentContext() );
                m_xDebugLogRing.set( logging::SingleDebugLogRing::get(xContext) );
            }
            catch( uno::Exception& )
            { }
        }

        if ( m_xDebugLogRing.is() )
        {
            try
            {
                rtl::OUString aFileName(buff, std::strlen(buff), RTL_TEXTENCODING_ASCII_US);
                if(writeAlways || !m_xDebugLogRing->isEmpty())
                    m_xDebugLogRing->flushLog(aFileName);
            }
            catch( uno::Exception& )
            { }
        }
    }

 /**
 * write log to fileName
 * the file is created in <user dir>/temp
 * if a file with the same name exists, it's overwritten
 *
 * the internal logger content is unchanged
 */
    void writeDebugLog(char const* fileName)
    {
        writeLog(fileName, true);
    }

 /**
 * write log to fileName, but oly if the logger contains records
 * the file is created in <user dir>/temp
 * if a file with the same name exists, it's overwritten
 *
 * the internal logger content is unchanged
 */
    void writeDebugLogIfNotEmpty(char const* fileName)
    {
        writeLog(fileName, false);
    }

}; //namespace tools
