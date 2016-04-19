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

#include <comphelper/simplefileaccessinteraction.hxx>
#include <com/sun/star/task/XInteractionAbort.hpp>
#include <com/sun/star/task/XInteractionApprove.hpp>
#include <com/sun/star/ucb/AuthenticationRequest.hpp>
#include <com/sun/star/ucb/CertificateValidationRequest.hpp>
#include <com/sun/star/ucb/InteractiveIOException.hpp>
#include <com/sun/star/ucb/InteractiveNetworkException.hpp>

namespace comphelper{

SimpleFileAccessInteraction::SimpleFileAccessInteraction(const css::uno::Reference< css::task::XInteractionHandler >& xHandler,
                                                     const css::uno::Reference< css::task::XInteractionHandler >& xAuthenticationHandler)
             : m_bUsed                    (false)
             , m_bHandledByMySelf         (false)
             , m_bHandledByInternalHandler(false)
             , m_xAuthenticationHandler(xAuthenticationHandler)
{
    ::std::vector< ::ucbhelper::InterceptedInteraction::InterceptedRequest > lInterceptions;
    ::ucbhelper::InterceptedInteraction::InterceptedRequest                  aInterceptedRequest;

    //intercept standard IO error exception (local file)
    aInterceptedRequest.Handle = HANDLE_INTERACTIVEIOEXCEPTION;
    aInterceptedRequest.Request <<= css::ucb::InteractiveIOException();
    aInterceptedRequest.Continuation = cppu::UnoType<css::task::XInteractionAbort>::get();
    aInterceptedRequest.MatchExact = false;
    lInterceptions.push_back(aInterceptedRequest);

    //intercept network error exception (WebDAV ucp provider)
    aInterceptedRequest.Handle = HANDLE_INTERACTIVENETWORKEXCEPTION;
    aInterceptedRequest.Request <<= css::ucb::InteractiveNetworkException();
    aInterceptedRequest.Continuation = cppu::UnoType<css::task::XInteractionAbort>::get();
    aInterceptedRequest.MatchExact = false;
    lInterceptions.push_back(aInterceptedRequest);

    //intercept certificate validation request (WebDAV ucp provider)
    aInterceptedRequest.Handle = HANDLE_CERTIFICATEREQUEST;
    aInterceptedRequest.Request <<= css::ucb::CertificateValidationRequest();
    aInterceptedRequest.Continuation = cppu::UnoType<css::task::XInteractionApprove>::get();
    aInterceptedRequest.MatchExact = false;
    lInterceptions.push_back(aInterceptedRequest);

    //intercept authentication request (WebDAV ucp provider)
    aInterceptedRequest.Handle = HANDLE_AUTHENTICATIONREQUEST;
    aInterceptedRequest.Request <<= css::ucb::AuthenticationRequest();
    aInterceptedRequest.Continuation = cppu::UnoType<css::task::XInteractionApprove>::get();
    aInterceptedRequest.MatchExact = false;
    lInterceptions.push_back(aInterceptedRequest);

    setInterceptedHandler(xHandler);
    setInterceptions(lInterceptions);
}

void SimpleFileAccessInteraction::resetInterceptions()
{
    setInterceptions(::std::vector< ::ucbhelper::InterceptedInteraction::InterceptedRequest >());
}

void SimpleFileAccessInteraction::resetErrorStates()
{
    m_bUsed                     = false;
    m_bHandledByMySelf          = false;
    m_bHandledByInternalHandler = false;
}


ucbhelper::InterceptedInteraction::EInterceptionState SimpleFileAccessInteraction::intercepted(const ::ucbhelper::InterceptedInteraction::InterceptedRequest&                         aRequest,
                                                                  const css::uno::Reference< css::task::XInteractionRequest >& xRequest)
{
    // we are used!
    m_bUsed = true;

    // check if it's a real interception - might some parameters are not the right ones ...
    bool bAbort = false;
    switch(aRequest.Handle)
    {
        case HANDLE_INTERACTIVENETWORKEXCEPTION:
        case HANDLE_INTERACTIVEIOEXCEPTION:
        {
            bAbort = true;
        }
        break;

        case HANDLE_CERTIFICATEREQUEST:
        {
            // use default internal handler.
            if (m_xInterceptedHandler.is())
            {
                m_bHandledByInternalHandler = true;
                m_xInterceptedHandler->handle(xRequest);
                return ::ucbhelper::InterceptedInteraction::E_INTERCEPTED;
            }
            else
                bAbort = true;
            break;
        }
        case HANDLE_AUTHENTICATIONREQUEST:
        {
//use internal authentication dedicated handler and return
           if (m_xAuthenticationHandler.is())
           {
               m_xAuthenticationHandler->handle(xRequest);
               return ::ucbhelper::InterceptedInteraction::E_INTERCEPTED;
           }
           else //simply abort
               bAbort = true;
       }
       break;
    }

    // handle interaction by ourself
    if (bAbort)
    {
        m_bHandledByMySelf = true;
        css::uno::Reference< css::task::XInteractionContinuation > xAbort = ::ucbhelper::InterceptedInteraction::extractContinuation(
            xRequest->getContinuations(),
            cppu::UnoType<css::task::XInteractionAbort>::get() );
        if (!xAbort.is())
            return ::ucbhelper::InterceptedInteraction::E_NO_CONTINUATION_FOUND;
        xAbort->select();
        return ::ucbhelper::InterceptedInteraction::E_INTERCEPTED;
    }

    return ::ucbhelper::InterceptedInteraction::E_INTERCEPTED;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
