/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <cppunit/plugin/TestPlugIn.h>
#include "DAVResourceAccess.hxx"
#include "DAVException.hxx"
#include "DavURLObject.hxx"

using namespace webdav_ucp;

namespace
{

    class webdav_resource_access_test: public test::BootstrapFixture
    {

    public:
        webdav_resource_access_test() : BootstrapFixture( true, true ) {}

        // initialise your test code values here.
        void setUp() override;

        void tearDown() override;

        void DAVCheckRetries();
        void DAVCheckURLObjectHelper( OUString& theURL,
                                      OUString& thePercEncodedURL,
                                      OUString& thePercEncodedTitle,
                                      OUString& thePercDecodedTitle,
                                      OUString& thePercEncodedPath );
        void DAVCheckURLObject();

        // Change the following lines only, if you add, remove or rename
        // member functions of the current class,
        // because these macros are need by auto register mechanism.

        CPPUNIT_TEST_SUITE( webdav_resource_access_test );
        CPPUNIT_TEST( DAVCheckRetries );
        CPPUNIT_TEST( DAVCheckURLObject );
        CPPUNIT_TEST_SUITE_END();
    };                          // class webdav_local_test

    // initialise your test code values here.
    void webdav_resource_access_test::setUp()
    {
    }

    void webdav_resource_access_test::tearDown()
    {
    }

    // test when http connection should retry
    void webdav_resource_access_test::DAVCheckRetries()
    {
        // instantiate a resource access class
        DAVResourceAccess ResourceAccess(nullptr, nullptr, "http://url");
        // first check: all http errors from 100 to 399 should return true, to force a retry
        for (auto i = SC_CONTINUE; i < SC_BAD_REQUEST; i++)
        {
            const DAVException aTheException(DAVException::DAV_HTTP_ERROR, "http error code", i );
            CPPUNIT_ASSERT_EQUAL( true , ResourceAccess.handleException( aTheException, 1 ) );
        }
        // http error code from 400 to 499 should NOT force a retry
        for (auto i = SC_BAD_REQUEST; i < SC_INTERNAL_SERVER_ERROR; i++)
        {
            const DAVException aTheException(DAVException::DAV_HTTP_ERROR, "http error code", i );
            CPPUNIT_ASSERT_EQUAL( false , ResourceAccess.handleException( aTheException, 1 ) );
        }

        // http error code from 500 (SC_INTERNAL_SERVER_ERROR) up should force a retry
        // except in special value
        // 1999 as high limit is just a current (2016-09-25) choice.
        // RFC poses no limit to the max value of response status code
        for (auto i = SC_INTERNAL_SERVER_ERROR; i < 2000; i++)
        {
            const DAVException aTheException(DAVException::DAV_HTTP_ERROR, "http error code", i );
            switch ( i )
            {
                // the HTTP response status codes that can be retried
                case SC_BAD_GATEWAY:
                case SC_GATEWAY_TIMEOUT:
                case SC_SERVICE_UNAVAILABLE:
                case SC_INSUFFICIENT_STORAGE:
                    CPPUNIT_ASSERT_EQUAL( true , ResourceAccess.handleException( aTheException, 1 ) );
                    break;
                    // default is NOT retry
                default:
                    CPPUNIT_ASSERT_EQUAL( false , ResourceAccess.handleException( aTheException, 1 ) );
            }
        }

        // check the retry request
        {
            const DAVException aTheException(DAVException::DAV_HTTP_RETRY, "the-host-name", 8080 );
            CPPUNIT_ASSERT_EQUAL( true , ResourceAccess.handleException( aTheException, 1 ) );
        }
    }

    void webdav_resource_access_test::DAVCheckURLObjectHelper( OUString& theURL,
                                                               OUString& thePercEncodedURL,
                                                               OUString& thePercEncodedTitle,
                                                               OUString& thePercDecodedTitle,
                                                               OUString& thePercEncodedPath )
    {
        // test percent-encode from human readable to URL
        DavURLObject aDavURL( theURL );
        // test returned URL
        CPPUNIT_ASSERT_EQUAL( thePercEncodedURL, aDavURL.GetMainURL() );
        // check percent-encoded title
        CPPUNIT_ASSERT_EQUAL( thePercEncodedTitle, aDavURL.GetPathBaseName() );
        // check percent-decoded title
        CPPUNIT_ASSERT_EQUAL( thePercDecodedTitle, aDavURL.GetPercDecodedPathBaseName() );
        // get path to be used in Web method operations
        CPPUNIT_ASSERT_EQUAL( thePercEncodedPath, aDavURL.GetPathQueryFragment() );
    }

    // test DavURLObject behaviour
    // percent-encode percent-decode
    void webdav_resource_access_test::DAVCheckURLObject()
    {

        // test percent-encode from human readable to URL
        OUString theURL =              "http://server.biz:8040/aService/a segment/next segment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0";
        OUString thePercEncodedURL =   "http://server.biz:8040/aService/a%20segment/next%20segment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0";
        OUString thePercEncodedTitle = "check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0";
        OUString thePercDecodedTitle = "check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0";
        OUString thePercEncodedPath  = "/aService/a%20segment/next%20segment/check.this?test=true&link=http://anotherserver.com/%3Fcheck=theapplication%26os=linuxintel%26lang=en-US%26version=5.2.0";

        DAVCheckURLObjectHelper( theURL, thePercEncodedURL, thePercEncodedTitle, thePercDecodedTitle, thePercEncodedPath );

        // unicode string path tests
        theURL =              OStringToOUString( "https://upload.some_server.org/helper/commons/thumb/7/7d/Teoría de autómatas.svg/300px-Teoría de autómatas.svg.png", RTL_TEXTENCODING_UTF8 );
        thePercEncodedURL =   "https://upload.some_server.org/helper/commons/thumb/7/7d/Teor%C3%ADa%20de%20aut%C3%B3matas.svg/300px-Teor%C3%ADa%20de%20aut%C3%B3matas.svg.png";
        thePercEncodedTitle = "300px-Teor%C3%ADa%20de%20aut%C3%B3matas.svg.png";
        thePercDecodedTitle = OStringToOUString( "300px-Teoría de autómatas.svg.png", RTL_TEXTENCODING_UTF8 );
        thePercEncodedPath  = "/helper/commons/thumb/7/7d/Teor%C3%ADa%20de%20aut%C3%B3matas.svg/300px-Teor%C3%ADa%20de%20aut%C3%B3matas.svg.png";
        DAVCheckURLObjectHelper( theURL, thePercEncodedURL, thePercEncodedTitle, thePercDecodedTitle, thePercEncodedPath );

        // test specific query ,from tdf#99499 (1)
        theURL =              "http://www.digikey.com/web export/common/mkt/en/help.png?requestedName=help?requestedName=help?requestedName=help?requestedName=help?requestedName=help";
        thePercEncodedURL =   "http://www.digikey.com/web%20export/common/mkt/en/help.png?requestedName=help?requestedName=help?requestedName=help?requestedName=help?requestedName=help";
        thePercEncodedTitle = "help.png?requestedName=help?requestedName=help?requestedName=help?requestedName=help?requestedName=help";
        thePercDecodedTitle = "help.png?requestedName=help?requestedName=help?requestedName=help?requestedName=help?requestedName=help";
        thePercEncodedPath  = "/web%20export/common/mkt/en/help.png?requestedName=help?requestedName=help?requestedName=help?requestedName=help?requestedName=help";
        DAVCheckURLObjectHelper( theURL, thePercEncodedURL, thePercEncodedTitle, thePercDecodedTitle, thePercEncodedPath );

        // test specific query ,from tdf#99499 (2)
        theURL =              "https://sealserver.trustkeeper.net/seal_image.php?customerId=84EDAB68F81B2B31985E5E20392A8AC1&size=105x54&style=normal?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image";
        thePercEncodedURL =   "https://sealserver.trustkeeper.net/seal_image.php?customerId=84EDAB68F81B2B31985E5E20392A8AC1&size=105x54&style=normal?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image";
        thePercEncodedTitle = "seal_image.php?customerId=84EDAB68F81B2B31985E5E20392A8AC1&size=105x54&style=normal?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image";
        thePercDecodedTitle = "seal_image.php?customerId=84EDAB68F81B2B31985E5E20392A8AC1&size=105x54&style=normal?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image";
        thePercEncodedPath  = "/seal_image.php?customerId=84EDAB68F81B2B31985E5E20392A8AC1&size=105x54&style=normal?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image?requestedName=seal_image";
        DAVCheckURLObjectHelper( theURL, thePercEncodedURL, thePercEncodedTitle, thePercDecodedTitle, thePercEncodedPath );
    }

    CPPUNIT_TEST_SUITE_REGISTRATION( webdav_resource_access_test );
}                               // namespace rtl_random

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
