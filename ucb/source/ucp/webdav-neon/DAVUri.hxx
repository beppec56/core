/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVURI_HXX
#define INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVURI_HXX

#include <ne_uri.h>
#include <rtl/ustring.hxx>
#include <tools/urlobj.hxx>
#include "DAVException.hxx"

namespace webdav_ucp
{
#define DEFAULT_HTTP_PORT       80
#define DEFAULT_HTTPS_PORT      443
#define DEFAULT_FTP_PORT        21


    /// class to manage URI used in WebDAV
    // URI naming scheme (from URI RFC):
    //  foo://example.com:8042/over/there?name=ferret#nose
    //  \_/   \______________/\_________/ \_________/ \__/
    //   |           |            |            |        |
    // scheme     authority       path        query   fragment
    //
    // definition of http URI can be found in:
    // <https://tools.ietf.org/html/rfc7230#section-2.7>
    class DAVUri
    {
    private:
        INetURLObject m_TheURL;

    public:
        DAVUri();

        explicit DAVUri( const OUString  & rTheAbsRef );

        DAVUri( const DAVUri & rOther );

        virtual ~DAVUri();

        void clearUri() {
            OUString clr = "";
            m_TheURL.SetURL( clr );
        };

        OUString GetURI( INetURLObject::DecodeMechanism eMechanism = INetURLObject::NO_DECODE ) const {
            return m_TheURL.GetMainURL( eMechanism );
        }

        bool removeFinalSlash() { return m_TheURL.removeFinalSlash(); };

        bool HasError() { return m_TheURL.HasError(); };

        bool SetUri( const OUString & rTheAbsRef,
                     INetURLObject::EncodeMechanism eMechanism = INetURLObject::WAS_ENCODED,
                     rtl_TextEncoding eCharset = RTL_TEXTENCODING_UTF8 ) {
            return m_TheURL.SetURL( rTheAbsRef, eMechanism, eCharset );
        };

        inline OUString GetPath(
            INetURLObject::DecodeMechanism eMechanism = INetURLObject::NO_DECODE ) const;

        inline OUString GetPathBaseName() const;

        inline OUString GetPathBaseNameUnescaped() const;

        OUString GetHost() const {
            return m_TheURL.GetHost();
        };

        inline OUString GetScheme() const
            throw ( DAVException );

        inline sal_uInt32 GetPort() const
            throw ( DAVException );

        inline void verifyUri() const
            throw ( DAVException );

        DAVUri & operator=( const DAVUri & rOther ) {
            m_TheURL = rOther.m_TheURL;
            return *this;
        };

        bool operator==( const DAVUri& rOther ) const { return m_TheURL == rOther.m_TheURL; };

        void SetScheme ( const OUString& rScheme );

        /// this function is meant to add the target folder the last segment
        // of the path.
        // It's used in transfert operation only!
        void AppendPath (const OUString& rPath);

        /// This function escapes only the last segment,
        // please do not use for anything else!
        static OUString escapeSegment( const OUString& rTheSegment );

        /// This function unescapes only the last segment,
        // please do not use for anything else!
        static OUString unescape( const OUString& rTheSegment );

        static OUString makeConnectionEndPointString( const OUString & rHostName,
                                                      int nPort );
    };

    inline OUString DAVUri::GetPath( INetURLObject::DecodeMechanism eMechanism ) const
    {
        OUString PathQueryFragment( m_TheURL.GetURLPath( eMechanism ) );
        if ( m_TheURL.HasParam() )
        {
            PathQueryFragment += "?";
            PathQueryFragment += m_TheURL.GetParam( eMechanism );
        }

        if ( m_TheURL.HasMark() )
        {
            PathQueryFragment += "#";
            PathQueryFragment += m_TheURL.GetMark( eMechanism );
        }
        return PathQueryFragment;
    };

    /// return the last segment of the path, escaped
    inline OUString DAVUri::GetPathBaseName() const
    {
        OUString aRet( m_TheURL.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::NO_DECODE ) );
        if ( m_TheURL.HasParam() )
        {
            aRet += "?";
            aRet += m_TheURL.GetParam( INetURLObject::NO_DECODE );
        }

        if ( m_TheURL.HasMark() )
        {
            aRet += "#";
            aRet += m_TheURL.GetMark( INetURLObject::NO_DECODE );
        }
        return aRet;
    }

    /// return the last segment of the path, percent-decoded
    inline OUString DAVUri::GetPathBaseNameUnescaped() const
    {
        OUString aRet( m_TheURL.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DECODE_UNAMBIGUOUS ) );
        if ( m_TheURL.HasParam() )
        {
            aRet += "?";
            aRet += m_TheURL.GetParam( INetURLObject::DECODE_UNAMBIGUOUS );
        }

        if ( m_TheURL.HasMark() )
        {
            aRet += "#";
            aRet += m_TheURL.GetMark( INetURLObject::DECODE_UNAMBIGUOUS );
        }
        return aRet;
    }

    // Specific function member to be adapted to the support library
    // used for http transaction management.
    // This is for neon library.
    inline void DAVUri::verifyUri() const
        throw ( DAVException )
    {
        if ( m_TheURL.HasError() )
            throw DAVException( DAVException::DAV_INVALID_ARG );

        // verify the URL against neon libray needs:
        OUString aEscapedUri( GetURI() );
        OString theInputUri(
            aEscapedUri.getStr(), aEscapedUri.getLength(), RTL_TEXTENCODING_UTF8 );

        ne_uri theUri;
        if ( ne_uri_parse( theInputUri.getStr(), &theUri ) != 0 )
        {
            ne_uri_free( &theUri );
            throw DAVException( DAVException::DAV_INVALID_ARG );
        }
        ne_uri_free( &theUri );
    }

    inline OUString DAVUri::GetScheme() const
        throw ( DAVException )
    {
        INetProtocol theProto = m_TheURL.GetProtocol();

        if ( theProto == INetProtocol::NotValid )
            throw DAVException( DAVException::DAV_INVALID_ARG );

        return INetURLObject::GetSchemeName( theProto );
    }

    inline sal_uInt32 DAVUri::GetPort() const
        throw ( DAVException )
    {
        sal_uInt32 port = m_TheURL.GetPort();
        if ( port == 0 )
        {
            // check the protocol
            if ( m_TheURL.isSchemeEqualTo( INetProtocol::Http ) )
                return 80;
            else if ( m_TheURL.isSchemeEqualTo( INetProtocol::Https ) )
                return 443;
            else // we need a port, 0 is not good
                throw DAVException( DAVException::DAV_INVALID_ARG );
        }
        return port;
    }

}


#endif // INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVURI_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
