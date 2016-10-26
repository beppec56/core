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

        inline void verifyUri() const
            throw ( DAVException );

        DAVUri & operator=( const DAVUri & rOther ) {
            m_TheURL = rOther.m_TheURL;
            return *this;
        };

        bool operator==( const DAVUri& rOther ) const { return m_TheURL == rOther.m_TheURL; };
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

        // the next code verify the URL against neon libray needs
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

}

#endif // INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVURI_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
