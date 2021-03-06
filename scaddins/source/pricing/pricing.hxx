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

// option pricing functions add in

// most parts of this files are technical UNO details which are
// all copied from ../datefunc/datefunc.hxx
// to avoid having to rename all classes to do with UNO
// technicalities we use our own namespace

#ifndef INCLUDED_SCADDINS_SOURCE_PRICING_PRICING_HXX
#define INCLUDED_SCADDINS_SOURCE_PRICING_PRICING_HXX


#include <string.h>
#include <vector>
#include <com/sun/star/lang/XServiceName.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/sheet/XAddIn.hpp>
#include <com/sun/star/sheet/XCompatibilityNames.hpp>
#include <com/sun/star/sheet/addin/XPricingFunctions.hpp>
#include <cppuhelper/implbase.hxx>
#include <tools/resid.hxx>
#include <tools/resary.hxx>

#define RETURN_FINITE(d)    if( ::rtl::math::isFinite( d ) ) return d; else throw css::lang::IllegalArgumentException()


namespace sca {
namespace pricing {

class ScaResId : public ResId
{
public:
    ScaResId( sal_uInt16 nResId, ResMgr& rResMgr );
};

enum class ScaCategory
{
    DateTime,
    Text,
    Finance,
    Inf,
    Math,
    Tech
};

struct ScaFuncDataBase
{
    const sal_Char*             pIntName;           // internal name (get***)
    sal_uInt16                  nUINameID;          // resource ID to UI name
    sal_uInt16                  nDescrID;           // resource ID to description, parameter names and ~ description
    sal_uInt16                  nCompListID;        // resource ID to list of valid names
    sal_uInt16                  nParamCount;        // number of named / described parameters
    ScaCategory                 eCat;               // function category
    bool                        bDouble;            // name already exist in Calc
    bool                        bWithOpt;           // first parameter is internal
};

class ScaFuncData final
{
private:
    OUString             aIntName;           // internal name (get***)
    sal_uInt16                  nUINameID;          // resource ID to UI name
    sal_uInt16                  nDescrID;           // leads also to parameter descriptions!
    sal_uInt16                  nParamCount;        // num of parameters
    std::vector<OUString>       aCompList;          // list of all valid names
    ScaCategory                 eCat;               // function category
    bool                    bDouble;            // name already exist in Calc
    bool                    bWithOpt;           // first parameter is internal

public:
                                ScaFuncData( const ScaFuncDataBase& rBaseData, ResMgr& rRscMgr );
                                ~ScaFuncData();

    sal_uInt16           GetUINameID() const     { return nUINameID; }
    sal_uInt16           GetDescrID() const      { return nDescrID; }
    ScaCategory          GetCategory() const     { return eCat; }
    bool                 IsDouble() const        { return bDouble; }

    sal_uInt16                  GetStrIndex( sal_uInt16 nParam ) const;
    bool                 Is( const OUString& rCompare ) const
                                                    { return aIntName == rCompare; }

    const std::vector<OUString>& GetCompNameList() const { return aCompList; }
};


typedef std::vector<ScaFuncData> ScaFuncDataList;

void InitScaFuncDataList ( ScaFuncDataList& rMap, ResMgr& rResMgr );

// Predicate for use with std::find_if
struct FindScaFuncData
{
    const OUString& m_rId;
    explicit FindScaFuncData( const OUString& rId ) : m_rId(rId) {}
    bool operator() ( ScaFuncData& rCandidate ) const { return rCandidate.Is(m_rId); }
};

} // namespace pricing
} // namespace sca


css::uno::Reference< css::uno::XInterface > SAL_CALL PricingFunctionAddIn_CreateInstance(
    const css::uno::Reference< css::lang::XMultiServiceFactory >& );


// AddIn class for pricing functions

class ScaPricingAddIn : public ::cppu::WeakImplHelper<
                                css::sheet::XAddIn,
                                css::sheet::XCompatibilityNames,
                                css::sheet::addin::XPricingFunctions,
                                css::lang::XServiceName,
                                css::lang::XServiceInfo >
{
private:
    css::lang::Locale  aFuncLoc;
    css::lang::Locale* pDefLocales;
    ResMgr*                     pResMgr;
    sca::pricing::ScaFuncDataList*            pFuncDataList;


    void                        InitDefLocales();
    const css::lang::Locale& GetLocale( sal_uInt32 nIndex );
    /// @throws css::uno::RuntimeException
    ResMgr&                     GetResMgr();
    void                        InitData();

    /// @throws css::uno::RuntimeException
    OUString             GetFuncDescrStr( sal_uInt16 nResId, sal_uInt16 nStrIndex );

public:
                                ScaPricingAddIn();
    virtual                     ~ScaPricingAddIn() override;

    static OUString      getImplementationName_Static();
    static css::uno::Sequence< OUString > getSupportedServiceNames_Static();

                                // XAddIn
    virtual OUString SAL_CALL getProgrammaticFuntionName( const OUString& aDisplayName ) override;
    virtual OUString SAL_CALL getDisplayFunctionName( const OUString& aProgrammaticName ) override;
    virtual OUString SAL_CALL getFunctionDescription( const OUString& aProgrammaticName ) override;
    virtual OUString SAL_CALL getDisplayArgumentName( const OUString& aProgrammaticName, sal_Int32 nArgument ) override;
    virtual OUString SAL_CALL getArgumentDescription( const OUString& aProgrammaticName, sal_Int32 nArgument ) override;
    virtual OUString SAL_CALL getProgrammaticCategoryName( const OUString& aProgrammaticName ) override;
    virtual OUString SAL_CALL getDisplayCategoryName( const OUString& aProgrammaticName ) override;

                                // XCompatibilityNames
    virtual css::uno::Sequence< css::sheet::LocalizedName > SAL_CALL getCompatibilityNames( const OUString& aProgrammaticName ) override;

                                // XLocalizable
    virtual void SAL_CALL       setLocale( const css::lang::Locale& eLocale ) override;
    virtual css::lang::Locale SAL_CALL getLocale() override;

                                // XServiceName
    virtual OUString SAL_CALL getServiceName() override;

                                // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL   supportsService( const OUString& ServiceName ) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;


    //  methods from own interfaces start here


    virtual double SAL_CALL getOptBarrier( double spot, double vol,
            double r, double rf, double T, double strike,
            double barrier_low, double barrier_up, double rebate,
            const OUString& put_call, const OUString& in_out,
            const OUString& continuous, const css::uno::Any& greek ) override;

   virtual double SAL_CALL getOptTouch( double spot, double vol,
            double r, double rf, double T,
            double barrier_low, double barrier_up,
            const OUString& for_dom, const OUString& in_out,
            const OUString& barriercont, const css::uno::Any& greekstr ) override;

   virtual double SAL_CALL getOptProbHit( double spot, double vol,
            double mu, double T,
            double barrier_low, double barrier_up ) override;

   virtual double SAL_CALL getOptProbInMoney( double spot, double vol,
            double mu, double T,
            double barrier_low, double barrier_up,
            const css::uno::Any& strikeval, const css::uno::Any& put_call ) override;

};


#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
