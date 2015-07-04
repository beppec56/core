/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_folders.h>

#include <comphelper/processfactory.hxx>
#include <com/sun/star/logging/XDebugLogRing.hpp>
#include <com/sun/star/logging/DebugLogRing.hpp>
#include <com/sun/star/logging/SingleDebugLogRing.hpp>

#include <stdio.h> // snprintf, vsnprintf

#include "osl/time.h"
#include <osl/file.hxx>
#include <rtl/bootstrap.hxx>

#include "tools/debuglogger.hxx"

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

namespace tools
{
    static uno::Reference< logging::XDebugLogRing > m_xDebugLogRing;
    static osl::Mutex   aMutex;

    static void sendLoggerLineMessage(char const* pszMessage)
    {
        oslDateTime currentDate;
        TimeValue   currentTime;
        osl_getSystemTime(&currentTime);
        osl_getDateTimeFromTimeValue( &currentTime, &currentDate );

        char buf[8192];
        int n1 = snprintf( buf, sizeof buf, "%02d.%02d.%02d.%03d:%s",
                           currentDate.Hours, currentDate.Minutes, currentDate.Seconds, currentDate.NanoSeconds / 1000000,
                           pszMessage );
        const char *aLogLine = ( n1 > 0 && n1 < 8192 )? buf:pszMessage;

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
            OUString aMessage( aLogLine ,strlen(aLogLine), RTL_TEXTENCODING_ASCII_US );
            m_xDebugLogRing->logString( aMessage );
        }
    }

    // in the directory <user private LO conf>/user/temp do a dir of the
    // file names that contains 'debug-logger.log'
    // for every one of them, check the date, if the date is older
    // then current_time - fileAgeInHours
    // remove it
    static void deleteOlderFiles(int nFileAgeInHours )
    {
        // grab the DebugLoggerFileAgeInHours from the setup.ini/setuprc file in current installation
        // to set the file age, you can add the following parameter to setup(rc or .ini), assigning it
        // the number of hours of file age, for example 96 means 4 days:
        // DebugLoggerFileAgeInHours=96
        // please maintain the capital letters

        OUString aSetupFileAgeInHours = ( "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE("setup") ":DebugLoggerFileAgeInHours}"  );
        ::rtl::Bootstrap::expandMacros( aSetupFileAgeInHours );
        if(aSetupFileAgeInHours.getLength())
            nFileAgeInHours = ( aSetupFileAgeInHours.toInt32() == 0 )? nFileAgeInHours :
                aSetupFileAgeInHours.toInt32();

        int nFileAgeInSeconds = nFileAgeInHours*3600;

        OUString path = "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE( "bootstrap") "::UserInstallation}";
        rtl::Bootstrap::expandMacros( path );
        path += "/user/temp/";
        osl::Directory dir( path );
         // Get the current file Status
        TimeValue TargetRemoveTime = {0,0};
        osl_getSystemTime(&TargetRemoveTime);
        // compute older file max time
        TargetRemoveTime.Seconds -= nFileAgeInSeconds;
        if( dir.reset() == osl::Directory::E_None )
        {
            for(;;)
            {
                osl::DirectoryItem item;
                if( dir.getNextItem( item ) != osl::Directory::E_None )
                    break;
                osl::FileStatus status( osl_FileStatus_Mask_FileURL );
                if( item.getFileStatus( status ) == osl::File::E_None )
                {
                    OUString afile = status.getFileURL();
                    if( afile.endsWith( "debug-logger.log" ) )
                    {
                        osl::FileStatus statusModTime( osl_FileStatus_Mask_ModifyTime );
                        item.getFileStatus( statusModTime );
                        TimeValue ModifyTime = statusModTime.getModifyTime();
                        if( ModifyTime.Seconds < TargetRemoveTime.Seconds )
                            osl::File::remove( afile );
                    }
                }
            }
        }
    }

    void initDebugLog()
    {
        deleteOlderFiles(24*5); // latest 5 days logs are retained, other removed
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
                rtl::OUString aFileName(buff, strlen(buff), RTL_TEXTENCODING_ASCII_US);
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
    void writeDebugLogDefaultName()
    {
        writeLog("debug-logger.log", true);
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
 * write log to fileName
 * the file is created in <user dir>/temp
 * if a file with the same name exists, it's overwritten
 *
 * the internal logger content is unchanged
 */
    void writeDebugLogDefaultNameIfNotEmpty()
    {
        writeLog("debug-logger.log", false);
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
