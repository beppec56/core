/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_COMPHELPER_SOURCE_MISC_DEBUGLOGRING_HXX_
#define INCLUDED_COMPHELPER_SOURCE_MISC_DEBUGLOGRING_HXX_

#include <com/sun/star/logging/XDebugLogRing.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XInitialization.hpp>

#include <osl/mutex.hxx>
#include <cppuhelper/implbase.hxx>

#define DEBUGLOGRING_SIZE 4096

namespace comphelper
{

class ODebugLogRing : public ::cppu::WeakImplHelper< ::com::sun::star::logging::XDebugLogRing,
                                                     ::com::sun::star::lang::XInitialization,
                                                     ::com::sun::star::lang::XServiceInfo >
{
    ::osl::Mutex m_aMutex;
    ::com::sun::star::uno::Sequence< OUString > m_aMessages;

    bool m_bInitialized;
    bool m_bFull;
    sal_Int32 m_nPos;

public:
    ODebugLogRing();
    virtual ~ODebugLogRing();

    static ::com::sun::star::uno::Sequence< OUString > SAL_CALL
    getSupportedServiceNames_static();

    static OUString SAL_CALL getImplementationName_static();

    static OUString SAL_CALL getSingletonName_static();

    static OUString SAL_CALL getServiceName_static();

    static ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface > SAL_CALL
        Create( const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >& rxContext );

// XDebugLogRing
    virtual void SAL_CALL logString( const OUString& aMessage ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Sequence< OUString > SAL_CALL getCollectedLog() throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual sal_Bool SAL_CALL isEmpty() throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual void SAL_CALL flushLog( const OUString& aFileName ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

// XInitialization
    virtual void SAL_CALL initialize( const ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >& aArguments ) throw (::com::sun::star::uno::Exception, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

// XServiceInfo
    virtual OUString SAL_CALL getImplementationName(  ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual sal_Bool SAL_CALL supportsService( const OUString& ServiceName ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames(  ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

};

} // namespace comphelper

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
