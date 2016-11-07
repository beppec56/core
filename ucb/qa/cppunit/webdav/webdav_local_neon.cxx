/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>
#include "NeonUri.hxx"

using namespace webdav_ucp;

namespace
{

    class webdav_local_test: public CppUnit::TestFixture
    {

    public:
        void NeonUriTest();

        // Change the following lines only, if you add, remove or rename
        // member functions of the current class,
        // because these macros are need by auto register mechanism.

        CPPUNIT_TEST_SUITE( webdav_local_test );
        CPPUNIT_TEST( NeonUriTest );
        CPPUNIT_TEST_SUITE_END();
    };                          // class webdav_local_test

    void webdav_local_test::NeonUriTest()
    {
        //try URL decomposition
        OUString aURL( "http://user%40anothername@server.biz:8040/aService/asegment/nextsegment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0" );
        NeonUri aURI( aURL );
        CPPUNIT_ASSERT_EQUAL( OUString( "http" ), aURI.GetScheme() );
        CPPUNIT_ASSERT_EQUAL( OUString( "server.biz" ), aURI.GetHost() );
        CPPUNIT_ASSERT_EQUAL( OUString( "user%40anothername" ), aURI.GetUserInfo() );
        CPPUNIT_ASSERT_EQUAL( sal_Int32( 8040 ), aURI.GetPort() );
        CPPUNIT_ASSERT_EQUAL( OUString( "http://user%40anothername@server.biz:8040/aService/asegment/nextsegment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0"),
                              aURI.GetURI() );
        CPPUNIT_ASSERT_EQUAL( OUString( "user%40anothername"), aURI.GetUserInfo() );
        CPPUNIT_ASSERT_EQUAL( OUString( "/aService/asegment/nextsegment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0" ), aURI.GetPath( ) );
        aURL = OStringToOUString( "Máquina de Turing: desambiguación.test", RTL_TEXTENCODING_UTF8 );
        // NOTE: escapeSegment is only meant to be used to escape the last segment of the path without query and/or fragment
        CPPUNIT_ASSERT_EQUAL( OUString( "M%C3%A1quina%20de%20Turing:%20desambiguaci%C3%B3n.test"), NeonUri::escapeSegment( aURL ) );
        aURL = "M%C3%A1quina%20de%20Turing:%20desambiguaci%C3%B3n.test";
        OUString aResult = OStringToOUString( "Máquina de Turing: desambiguación.test", RTL_TEXTENCODING_UTF8 );
        CPPUNIT_ASSERT_EQUAL( aResult, NeonUri::unescape( aURL ) );
    }

    CPPUNIT_TEST_SUITE_REGISTRATION( webdav_local_test );
}                               // namespace rtl_random

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
