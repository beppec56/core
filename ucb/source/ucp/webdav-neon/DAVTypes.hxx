/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*************************************************************************
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * Copyright 2000, 2010 Oracle and/or its affiliates.
 *
 * OpenOffice.org - a multi-platform office productivity suite
 *
 * This file is part of OpenOffice.org.
 *
 * OpenOffice.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * only, as published by the Free Software Foundation.
 *
 * OpenOffice.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 3 for more details
 * (a copy is included in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with OpenOffice.org.  If not, see
 * <http://www.openoffice.org/license.html>
 * for a copy of the LGPLv3 License.
 *
 ************************************************************************/

#ifndef INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVTYPES_HXX
#define INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVTYPES_HXX

#include <config_lgpl.h>
#include <memory>
#include <list>
#include <map>
#include "osl/mutex.hxx"
#include <rtl/uri.hxx>
#include <rtl/ustring.hxx>
#include <com/sun/star/uno/Any.hxx>

namespace webdav_ucp
{
/* RFC 2518

   15.1 Class 1

   A class 1 compliant resource MUST meet all "MUST" requirements in all
   sections of this document.

   Class 1 compliant resources MUST return, at minimum, the value "1" in
   the DAV header on all responses to the OPTIONS method.

   15.2 Class 2

   A class 2 compliant resource MUST meet all class 1 requirements and
   support the LOCK method, the supportedlock property, the
   lockdiscovery property, the Time-Out response header and the Lock-
   Token request header.  A class "2" compliant resource SHOULD also
   support the Time-Out request header and the owner XML element.

   Class 2 compliant resources MUST return, at minimum, the values "1"
   and "2" in the DAV header on all responses to the OPTIONS method.
*/

    class DAVCapabilities
    {
    private:
        bool    m_bResourceFound;   // true if the resource was found, else false
        bool    m_bDAVCapabilitiesValid; // true if this struct contains valid data
        bool    m_bClass1;
        bool    m_bClass2;
        bool    m_bClass3;
// contains the value of header ms-author-via
// needed to detect if the server is Sharepoint-like
        bool    m_bFPServerExtensions;

    public:
        /// target time when this capability becomes stale
        sal_uInt32 m_nStaleTime;
        OUString  m_sURL;

        DAVCapabilities();

        DAVCapabilities( const DAVCapabilities & rOther );

        ~DAVCapabilities();

        bool isDAVCapabilitiesValid() { return m_bDAVCapabilitiesValid; };
        void setDAVCapabilitiesValid( bool bValueToSet = true  ) { m_bDAVCapabilitiesValid = bValueToSet; };

        bool isResourceFound() { return m_bResourceFound; };
        void setResourceFound( bool bValueToSet = true ) { m_bResourceFound = bValueToSet; };

        bool isClass1() { return m_bClass1; };
        void setClass1( bool bValueToSet = true ) { m_bClass1 = bValueToSet; };

        bool isClass2() { return m_bClass2; };
        void setClass2( bool bValueToSet = true ) { m_bClass2 = bValueToSet; };

        bool isClass3() { return m_bClass3; };
        void setClass3( bool bValueToSet = true ) { m_bClass3 = bValueToSet; };

        bool isFPServerExtensions() { return  m_bFPServerExtensions; };
        void setFPServerExtensions( bool bValueToSet = true ) { m_bFPServerExtensions = bValueToSet; };

        void reset() {
            m_bResourceFound = false;
            m_bDAVCapabilitiesValid = false;
            m_bClass1 = false;
            m_bClass2 = false;
            m_bClass3 = false;
            m_bFPServerExtensions = false;
        };
    };

    typedef std::map< OUString, DAVCapabilities > DAVCapabilitiesMap;

    class DAVCapabilitiesCache
    {
        DAVCapabilitiesMap m_aTheCache;
        osl::Mutex         m_aMutex;
    public:
        explicit DAVCapabilitiesCache();
        ~DAVCapabilitiesCache();

        bool restoreDAVCapabilities( const OUString & rURL, DAVCapabilities & rDAVCapabilities );
        void removeDAVCapabilities( const OUString & rURL );
        void addDAVCapabilities( const OUString & rURL, DAVCapabilities & rDAVCapabilities );
    };

    enum Depth { DAVZERO = 0, DAVONE = 1, DAVINFINITY = -1 };

    enum ProppatchOperation { PROPSET = 0, PROPREMOVE = 1 };

    struct ProppatchValue
    {
        ProppatchOperation  operation;
        OUString            name;
        css::uno::Any       value;

        ProppatchValue( const ProppatchOperation o,
                        const OUString & n,
                        const css::uno::Any & v )
            : operation( o ), name( n ), value( v ) {}
    };

} // namespace webdav_ucp

#endif // INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVTYPES_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
