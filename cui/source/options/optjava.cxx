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

#include <sal/config.h>

#include <memory>
#include <vector>

#include <config_features.h>

#include "optaboutconfig.hxx"
#include "optjava.hxx"
#include <dialmgr.hxx>

#include <officecfg/Office/Common.hxx>
#include <svtools/miscopt.hxx>

#include <cuires.hrc>
#include "helpid.hrc"
#include <vcl/svapp.hxx>
#include <vcl/help.hxx>
#include <tools/urlobj.hxx>
#include <vcl/layout.hxx>
#include <vcl/waitobj.hxx>
#include <unotools/pathoptions.hxx>
#include <svtools/imagemgr.hxx>
#include <svtools/restartdialog.hxx>
#include "svtools/treelistentry.hxx"
#include <sfx2/filedlghelper.hxx>
#include <sfx2/inputdlg.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/string.hxx>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/ui/dialogs/XAsynchronousExecutableDialog.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/FolderPicker.hpp>
#include <com/sun/star/ucb/XContentProvider.hpp>
#if HAVE_FEATURE_JAVA
#include <jvmfwk/framework.hxx>
#endif

// define ----------------------------------------------------------------

#define CLASSPATH_DELIMITER SAL_PATHSEPARATOR

#include <comphelper/solarmutex.hxx>

using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::uno;

class SvxJavaListBox : public svx::SvxRadioButtonListBox
{
private:
    const OUString m_sAccessibilityText;
public:
    SvxJavaListBox(SvSimpleTableContainer& rParent, const OUString &rAccessibilityText)
        : SvxRadioButtonListBox(rParent, 0)
        , m_sAccessibilityText(rAccessibilityText)
    {
    }
    void setColSizes()
    {
        HeaderBar &rBar = GetTheHeaderBar();
        if (rBar.GetItemCount() < 4)
            return;
        long nCheckWidth = std::max(GetControlColumnWidth() + 12,
            rBar.LogicToPixel(Size(15, 0), MapUnit::MapAppFont).Width());
        long nVersionWidth = 12 +
            std::max(rBar.GetTextWidth(rBar.GetItemText(3)),
            GetTextWidth("0.0.0_00-icedtea"));
        long nFeatureWidth = 12 +
            std::max(rBar.GetTextWidth(rBar.GetItemText(4)),
            GetTextWidth(m_sAccessibilityText));
        long nVendorWidth =
            std::max(GetSizePixel().Width() - (nCheckWidth + nVersionWidth + nFeatureWidth),
            6 + std::max(rBar.GetTextWidth(rBar.GetItemText(2)),
            GetTextWidth("Sun Microsystems Inc.")));
        long aStaticTabs[]= { 4, 0, 0, 0, 0, 0 };
        aStaticTabs[2] = nCheckWidth;
        aStaticTabs[3] = aStaticTabs[2] + nVendorWidth;
        aStaticTabs[4] = aStaticTabs[3] + nVersionWidth;
        SvSimpleTable::SetTabs(aStaticTabs, MapUnit::MapPixel);
    }
    virtual void Resize() override
    {
        svx::SvxRadioButtonListBox::Resize();
        setColSizes();
    }
};

// class SvxJavaOptionsPage ----------------------------------------------

SvxJavaOptionsPage::SvxJavaOptionsPage( vcl::Window* pParent, const SfxItemSet& rSet )
    : SfxTabPage(pParent, "OptAdvancedPage", "cui/ui/optadvancedpage.ui", &rSet)
    , m_pParamDlg(nullptr)
    , m_pPathDlg(nullptr)
    , m_aResetIdle("cui options SvxJavaOptionsPage Reset")
    , xDialogListener(new ::svt::DialogClosedListener())
{
    get(m_pJavaEnableCB, "javaenabled");
    get(m_pJavaBox, "javabox");
    get(m_pJavaPathText, "javapath");
    m_sInstallText = m_pJavaPathText->GetText();
    get(m_pAddBtn, "add");
    get(m_pParameterBtn, "parameters");
    get(m_pClassPathBtn, "classpath");
    get(m_pExperimentalCB, "experimental");
    get(m_pMacroCB, "macrorecording");
    get(m_pExpertConfigBtn, "expertconfig");
    m_sAccessibilityText = get<FixedText>("a11y")->GetText();
    m_sAddDialogText = get<FixedText>("selectruntime")->GetText();

    SvSimpleTableContainer *pJavaListContainer = get<SvSimpleTableContainer>("javas");
    Size aControlSize(177, 60);
    aControlSize = LogicToPixel(aControlSize, MapUnit::MapAppFont);
    pJavaListContainer->set_width_request(aControlSize.Width());
    pJavaListContainer->set_height_request(aControlSize.Height());
    m_pJavaList = VclPtr<SvxJavaListBox>::Create(*pJavaListContainer, m_sAccessibilityText);

    long aStaticTabs[]= { 4, 0, 0, 0, 0 };

    m_pJavaList->SvSimpleTable::SetTabs( aStaticTabs );

    OUString sHeader ( "\t" + get<FixedText>("vendor")->GetText() +
        "\t" + get<FixedText>("version")->GetText() +
        "\t" + get<FixedText>("features")->GetText() +
        "\t" );
    m_pJavaList->InsertHeaderEntry(sHeader, HEADERBAR_APPEND, HeaderBarItemBits::LEFT);
    m_pJavaList->setColSizes();

    m_pJavaEnableCB->SetClickHdl( LINK( this, SvxJavaOptionsPage, EnableHdl_Impl ) );
    m_pJavaList->SetCheckButtonHdl( LINK( this, SvxJavaOptionsPage, CheckHdl_Impl ) );
    m_pJavaList->SetSelectHdl( LINK( this, SvxJavaOptionsPage, SelectHdl_Impl ) );
    m_pAddBtn->SetClickHdl( LINK( this, SvxJavaOptionsPage, AddHdl_Impl ) );
    m_pParameterBtn->SetClickHdl( LINK( this, SvxJavaOptionsPage, ParameterHdl_Impl ) );
    m_pClassPathBtn->SetClickHdl( LINK( this, SvxJavaOptionsPage, ClassPathHdl_Impl ) );
    m_aResetIdle.SetInvokeHandler( LINK( this, SvxJavaOptionsPage, ResetHdl_Impl ) );
    m_aResetIdle.SetPriority(TaskPriority::LOWER);

    m_pExpertConfigBtn->SetClickHdl( LINK( this, SvxJavaOptionsPage, ExpertConfigHdl_Impl) );
    if (!officecfg::Office::Common::Security::EnableExpertConfiguration::get())
        m_pExpertConfigBtn->Disable();

    if (officecfg::Office::Common::Misc::MacroRecorderMode::isReadOnly())
        m_pMacroCB->Disable();

    if (officecfg::Office::Common::Misc::ExperimentalMode::isReadOnly())
        m_pExperimentalCB->Disable();

    xDialogListener->SetDialogClosedLink( LINK( this, SvxJavaOptionsPage, DialogClosedHdl ) );

    EnableHdl_Impl(m_pJavaEnableCB);
#if HAVE_FEATURE_JAVA
    jfw_lock();
#else
    get<vcl::Window>("javaframe")->Disable();
#endif
}


SvxJavaOptionsPage::~SvxJavaOptionsPage()
{
    disposeOnce();
}

void SvxJavaOptionsPage::dispose()
{
    m_pJavaList.disposeAndClear();
    m_pParamDlg.disposeAndClear();
    m_pPathDlg.disposeAndClear();
    ClearJavaInfo();
#if HAVE_FEATURE_JAVA
    m_aAddedInfos.clear();

    jfw_unlock();
#endif
    m_pJavaEnableCB.clear();
    m_pJavaBox.clear();
    m_pJavaPathText.clear();
    m_pAddBtn.clear();
    m_pParameterBtn.clear();
    m_pClassPathBtn.clear();
    m_pExpertConfigBtn.clear();
    m_pExperimentalCB.clear();
    m_pMacroCB.clear();
    SfxTabPage::dispose();
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, EnableHdl_Impl, Button*, void)
{
    bool bEnable = m_pJavaEnableCB->IsChecked();
    m_pJavaBox->Enable(bEnable);
    bEnable ? m_pJavaList->EnableTable() : m_pJavaList->DisableTable();
}


IMPL_LINK( SvxJavaOptionsPage, CheckHdl_Impl, SvTreeListBox*, pList, void )
{
    SvTreeListEntry* pEntry = pList ? m_pJavaList->GetEntry( m_pJavaList->GetCurMousePoint() )
                                : m_pJavaList->FirstSelected();
    if ( pEntry )
        m_pJavaList->HandleEntryChecked( pEntry );
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, SelectHdl_Impl, SvTreeListBox*, void)
{
    // set installation directory info
    SvTreeListEntry* pEntry = m_pJavaList->FirstSelected();
    DBG_ASSERT( pEntry, "SvxJavaOptionsPage::SelectHdl_Impl(): no entry" );
    OUString* pLocation = static_cast< OUString* >( pEntry->GetUserData() );
    DBG_ASSERT( pLocation, "invalid location string" );
    OUString sInfo = m_sInstallText;
    if ( pLocation )
        sInfo += *pLocation;
    m_pJavaPathText->SetText(sInfo);
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, AddHdl_Impl, Button*, void)
{
    try
    {
        Reference < XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
        xFolderPicker = FolderPicker::create(xContext);

        OUString sWorkFolder = SvtPathOptions().GetWorkPath();
        xFolderPicker->setDisplayDirectory( sWorkFolder );
        xFolderPicker->setDescription( m_sAddDialogText );

        Reference< XAsynchronousExecutableDialog > xAsyncDlg( xFolderPicker, UNO_QUERY );
        if ( xAsyncDlg.is() )
            xAsyncDlg->startExecuteModal( xDialogListener.get() );
        else if ( xFolderPicker.is() && xFolderPicker->execute() == ExecutableDialogResults::OK )
            AddFolder( xFolderPicker->getDirectory() );
    }
    catch (const Exception& e)
    {
        SAL_WARN( "cui.options", "SvxJavaOptionsPage::AddHdl_Impl(): caught exception: " << e.Message);
    }
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, ParameterHdl_Impl, Button*, void)
{
#if HAVE_FEATURE_JAVA
    std::vector< OUString > aParameterList;
    if ( !m_pParamDlg )
    {
        m_pParamDlg = VclPtr<SvxJavaParameterDlg>::Create( this );
        javaFrameworkError eErr = jfw_getVMParameters( &m_parParameters );
        if ( JFW_E_NONE == eErr && !m_parParameters.empty() )
        {
            aParameterList = m_parParameters;
            m_pParamDlg->SetParameters( aParameterList );
        }
    }
    else
    {
        aParameterList = m_pParamDlg->GetParameters();
        m_pParamDlg->DisableButtons();   //disable add, edit and remove button when dialog is reopened
    }

    if ( m_pParamDlg->Execute() == RET_OK )
    {
        if ( aParameterList != m_pParamDlg->GetParameters() )
        {
            aParameterList = m_pParamDlg->GetParameters();
            if ( jfw_isVMRunning() )
            {
                SolarMutexGuard aGuard;
                svtools::executeRestartDialog(comphelper::getProcessComponentContext(), nullptr, svtools::RESTART_REASON_ASSIGNING_JAVAPARAMETERS);
            }
        }
    }
    else
        m_pParamDlg->SetParameters( aParameterList );
#else
    (void) this;                // Silence loplugin:staticmethods
#endif
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, ClassPathHdl_Impl, Button*, void)
{
#if HAVE_FEATURE_JAVA
    OUString sClassPath;

    if ( !m_pPathDlg )
    {
          m_pPathDlg = VclPtr<SvxJavaClassPathDlg>::Create( this );
        javaFrameworkError eErr = jfw_getUserClassPath( &m_pClassPath );
        if ( JFW_E_NONE == eErr )
        {
            sClassPath = m_pClassPath;
            m_pPathDlg->SetClassPath( sClassPath );
        }
    }
    else
        sClassPath = m_pPathDlg->GetClassPath();

    m_pPathDlg->SetFocus();
    if ( m_pPathDlg->Execute() == RET_OK )
    {

        if ( m_pPathDlg->GetClassPath() != sClassPath )
        {
            sClassPath = m_pPathDlg->GetClassPath();
            if ( jfw_isVMRunning() )
            {
                SolarMutexGuard aGuard;
                svtools::executeRestartDialog(comphelper::getProcessComponentContext(), nullptr, svtools::RESTART_REASON_ASSIGNING_FOLDERS);
            }
        }
    }
    else
        m_pPathDlg->SetClassPath( sClassPath );
#else
    (void) this;
#endif
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, ResetHdl_Impl, Timer *, void)
{
    LoadJREs();
}


IMPL_LINK_NOARG(SvxJavaOptionsPage, StartFolderPickerHdl, void*, void)
{
    try
    {
        Reference< XAsynchronousExecutableDialog > xAsyncDlg( xFolderPicker, UNO_QUERY );
        if ( xAsyncDlg.is() )
            xAsyncDlg->startExecuteModal( xDialogListener.get() );
        else if ( xFolderPicker.is() && xFolderPicker->execute() == ExecutableDialogResults::OK )
            AddFolder( xFolderPicker->getDirectory() );
    }
    catch ( Exception& )
    {
        SAL_WARN( "cui.options", "SvxJavaOptionsPage::StartFolderPickerHdl(): caught exception" );
    }
}


IMPL_LINK( SvxJavaOptionsPage, DialogClosedHdl, DialogClosedEvent*, pEvt, void )
{
    if ( RET_OK == pEvt->DialogResult )
    {
        DBG_ASSERT( xFolderPicker.is(), "SvxJavaOptionsPage::DialogClosedHdl(): no folder picker" );

        AddFolder( xFolderPicker->getDirectory() );
    }
}


IMPL_LINK_NOARG( SvxJavaOptionsPage, ExpertConfigHdl_Impl, Button*, void )
{
    ScopedVclPtrInstance< CuiAboutConfigTabPage > pExpertConfigDlg(this);
    pExpertConfigDlg->Reset();//initialize and reset function

    if( RET_OK == pExpertConfigDlg->Execute() )
    {
        pExpertConfigDlg->FillItemSet();//save changes if there are any
    }

    pExpertConfigDlg.disposeAndClear();
}


void SvxJavaOptionsPage::ClearJavaInfo()
{
#if HAVE_FEATURE_JAVA
    m_parJavaInfo.clear();
#else
    (void) this;
#endif
}


void SvxJavaOptionsPage::ClearJavaList()
{
    SvTreeListEntry* pEntry = m_pJavaList->First();
    while ( pEntry )
    {
        OUString* pLocation = static_cast< OUString* >( pEntry->GetUserData() );
        delete pLocation;
        pEntry = m_pJavaList->Next( pEntry );
    }
    m_pJavaList->Clear();
}


void SvxJavaOptionsPage::LoadJREs()
{
#if HAVE_FEATURE_JAVA
    WaitObject aWaitObj(m_pJavaList);
    javaFrameworkError eErr = jfw_findAllJREs( &m_parJavaInfo );
    if ( JFW_E_NONE == eErr )
    {
        for (auto const & pInfo: m_parJavaInfo)
        {
            AddJRE( pInfo.get() );
        }
    }

    for (auto const & pInfo: m_aAddedInfos)
    {
        AddJRE( pInfo.get() );
    }

    std::unique_ptr<JavaInfo> pSelectedJava;
    eErr = jfw_getSelectedJRE( &pSelectedJava );
    if ( JFW_E_NONE == eErr && pSelectedJava )
    {
        sal_Int32 i = 0;
        for (auto const & pCmpInfo: m_parJavaInfo)
        {
            if ( jfw_areEqualJavaInfo( pCmpInfo.get(), pSelectedJava.get() ) )
            {
                SvTreeListEntry* pEntry = m_pJavaList->GetEntry(i);
                if ( pEntry )
                    m_pJavaList->HandleEntryChecked( pEntry );
                break;
            }
            ++i;
        }
    }
#else
    (void) this;
#endif
}


void SvxJavaOptionsPage::AddJRE( JavaInfo const * _pInfo )
{
#if HAVE_FEATURE_JAVA
    OUStringBuffer sEntry;
    sEntry.append('\t');
    sEntry.append(_pInfo->sVendor);
    sEntry.append('\t');
    sEntry.append(_pInfo->sVersion);
    sEntry.append('\t');
    if ( ( _pInfo->nFeatures & JFW_FEATURE_ACCESSBRIDGE ) == JFW_FEATURE_ACCESSBRIDGE )
        sEntry.append(m_sAccessibilityText);
    SvTreeListEntry* pEntry = m_pJavaList->InsertEntry(sEntry.makeStringAndClear());
    INetURLObject aLocObj( _pInfo->sLocation );
    OUString* pLocation = new OUString( aLocObj.getFSysPath( FSysStyle::Detect ) );
    pEntry->SetUserData( pLocation );
#else
    (void) this;
    (void)_pInfo;
#endif
}


void SvxJavaOptionsPage::HandleCheckEntry( SvTreeListEntry* _pEntry )
{
    m_pJavaList->Select( _pEntry );
    SvButtonState eState = m_pJavaList->GetCheckButtonState( _pEntry );

    if ( SvButtonState::Checked == eState )
    {
        // we have radio button behavior -> so uncheck the other entries
        SvTreeListEntry* pEntry = m_pJavaList->First();
        while ( pEntry )
        {
            if ( pEntry != _pEntry )
                m_pJavaList->SetCheckButtonState( pEntry, SvButtonState::Unchecked );
            pEntry = m_pJavaList->Next( pEntry );
        }
    }
    else
        m_pJavaList->SetCheckButtonState( _pEntry, SvButtonState::Checked );
}


void SvxJavaOptionsPage::AddFolder( const OUString& _rFolder )
{
#if HAVE_FEATURE_JAVA
    bool bStartAgain = true;
    std::unique_ptr<JavaInfo> pInfo;
    javaFrameworkError eErr = jfw_getJavaInfoByPath( _rFolder, &pInfo );
    if ( JFW_E_NONE == eErr && pInfo )
    {
        sal_Int32 nPos = 0;
        bool bFound = false;
        for (auto const & pCmpInfo: m_parJavaInfo)
        {
            if ( jfw_areEqualJavaInfo( pCmpInfo.get(), pInfo.get() ) )
            {
                bFound = true;
                break;
            }
            ++nPos;
        }

        if ( !bFound )
        {
            for (auto const & pCmpInfo: m_aAddedInfos)
            {
                if ( jfw_areEqualJavaInfo( pCmpInfo.get(), pInfo.get() ) )
                {
                    bFound = true;
                    break;
                }
                ++nPos;
            }
        }

        if ( !bFound )
        {
            jfw_addJRELocation( pInfo->sLocation );
            AddJRE( pInfo.get() );
            m_aAddedInfos.push_back( std::move(pInfo) );
            nPos = m_pJavaList->GetEntryCount() - 1;
        }

        SvTreeListEntry* pEntry = m_pJavaList->GetEntry( nPos );
        m_pJavaList->Select( pEntry );
        m_pJavaList->SetCheckButtonState( pEntry, SvButtonState::Checked );
        HandleCheckEntry( pEntry );
        bStartAgain = false;
    }
    else if ( JFW_E_NOT_RECOGNIZED == eErr )
    {
        ScopedVclPtrInstance< MessageDialog > aErrBox( this, CuiResId( RID_SVXSTR_JRE_NOT_RECOGNIZED ) );
        aErrBox->Execute();
    }
    else if ( JFW_E_FAILED_VERSION == eErr )
    {
        ScopedVclPtrInstance< MessageDialog > aErrBox( this, CuiResId( RID_SVXSTR_JRE_FAILED_VERSION ) );
        aErrBox->Execute();
    }

    if ( bStartAgain )
    {
        xFolderPicker->setDisplayDirectory( _rFolder );
        Application::PostUserEvent( LINK( this, SvxJavaOptionsPage, StartFolderPickerHdl ) );
    }
#else
    (void) this;
    (void)_rFolder;
#endif
}


VclPtr<SfxTabPage> SvxJavaOptionsPage::Create( vcl::Window* pParent, const SfxItemSet* rAttrSet )
{
    return VclPtr<SvxJavaOptionsPage>::Create( pParent, *rAttrSet );
}


bool SvxJavaOptionsPage::FillItemSet( SfxItemSet* /*rCoreSet*/ )
{
    bool bModified = false;

    if ( m_pExperimentalCB->IsValueChangedFromSaved() )
    {
        SvtMiscOptions aMiscOpt;
        aMiscOpt.SetExperimentalMode( m_pExperimentalCB->IsChecked() );
        bModified = true;
        SolarMutexGuard aGuard;
        svtools::executeRestartDialog(comphelper::getProcessComponentContext(), nullptr, svtools::RESTART_REASON_EXP_FEATURES);
    }

    if ( m_pMacroCB->IsValueChangedFromSaved() )
    {
        SvtMiscOptions aMiscOpt;
        aMiscOpt.SetMacroRecorderMode( m_pMacroCB->IsChecked() );
        bModified = true;
    }

#if HAVE_FEATURE_JAVA
    javaFrameworkError eErr = JFW_E_NONE;
    if ( m_pParamDlg )
    {
        eErr = jfw_setVMParameters( m_pParamDlg->GetParameters() );
        SAL_WARN_IF(JFW_E_NONE != eErr, "cui.options", "SvxJavaOptionsPage::FillItemSet(): error in jfw_setVMParameters"); (void)eErr;
        bModified = true;
    }

    if ( m_pPathDlg )
    {
        OUString sPath( m_pPathDlg->GetClassPath() );
        if ( m_pPathDlg->GetOldPath() != sPath )
        {
            eErr = jfw_setUserClassPath( sPath );
            SAL_WARN_IF(JFW_E_NONE != eErr, "cui.options", "SvxJavaOptionsPage::FillItemSet(): error in jfw_setUserClassPath"); (void)eErr;
            bModified = true;
        }
    }

    sal_uLong nCount = m_pJavaList->GetEntryCount();
    for ( sal_uLong i = 0; i < nCount; ++i )
    {
        if ( m_pJavaList->GetCheckButtonState( m_pJavaList->GetEntry(i) ) == SvButtonState::Checked )
        {
            JavaInfo const * pInfo;
            if ( i < m_parJavaInfo.size() )
                pInfo = m_parJavaInfo[i].get();
            else
                pInfo = m_aAddedInfos[ i - m_parJavaInfo.size() ].get();

            std::unique_ptr<JavaInfo> pSelectedJava;
            eErr = jfw_getSelectedJRE( &pSelectedJava );
            if ( JFW_E_NONE == eErr || JFW_E_INVALID_SETTINGS == eErr )
            {
                if (!pSelectedJava || !jfw_areEqualJavaInfo( pInfo, pSelectedJava.get() ) )
                {
                    if ( jfw_isVMRunning() ||
                        ( ( pInfo->nRequirements & JFW_REQUIRE_NEEDRESTART ) == JFW_REQUIRE_NEEDRESTART ) )
                    {
                        svtools::executeRestartDialog(
                            comphelper::getProcessComponentContext(), this,
                            svtools::RESTART_REASON_JAVA);
                    }

                    eErr = jfw_setSelectedJRE( pInfo );
                    SAL_WARN_IF(JFW_E_NONE != eErr, "cui.options", "SvxJavaOptionsPage::FillItemSet(): error in jfw_setSelectedJRE"); (void)eErr;
                    bModified = true;
                }
            }
            break;
        }
    }

    bool bEnabled = false;
    eErr = jfw_getEnabled( &bEnabled );
    DBG_ASSERT( JFW_E_NONE == eErr,
                "SvxJavaOptionsPage::FillItemSet(): error in jfw_getEnabled" ); (void)eErr;
    if ( bEnabled != m_pJavaEnableCB->IsChecked() )
    {
        eErr = jfw_setEnabled( m_pJavaEnableCB->IsChecked() );
        DBG_ASSERT( JFW_E_NONE == eErr,
                    "SvxJavaOptionsPage::FillItemSet(): error in jfw_setEnabled" );
        bModified = true;
    }
#endif

    return bModified;
}


void SvxJavaOptionsPage::Reset( const SfxItemSet* /*rSet*/ )
{
    ClearJavaInfo();
    ClearJavaList();

    SvtMiscOptions aMiscOpt;

#if HAVE_FEATURE_JAVA
    bool bEnabled = false;
    javaFrameworkError eErr = jfw_getEnabled( &bEnabled );
    if ( eErr != JFW_E_NONE )
        bEnabled = false;
    m_pJavaEnableCB->Check( bEnabled );
    EnableHdl_Impl(m_pJavaEnableCB);
#else
    m_pJavaEnableCB->Check( false );
    m_pJavaEnableCB->Disable();
#endif

    m_pExperimentalCB->Check( aMiscOpt.IsExperimentalMode() );
    m_pExperimentalCB->SaveValue();
    m_pMacroCB->Check( aMiscOpt.IsMacroRecorderMode() );
    m_pMacroCB->SaveValue();

    m_aResetIdle.Start();
}


void SvxJavaOptionsPage::FillUserData()
{
    OUString aUserData;
    SetUserData( aUserData );
}

// class SvxJavaParameterDlg ---------------------------------------------

SvxJavaParameterDlg::SvxJavaParameterDlg( vcl::Window* pParent ) :

    ModalDialog( pParent, "JavaStartParameters",
                 "cui/ui/javastartparametersdialog.ui" )
{
    get( m_pParameterEdit, "parameterfield");
    get( m_pAssignBtn, "assignbtn");
    get( m_pAssignedList, "assignlist");
    m_pAssignedList->SetDropDownLineCount(6);
    m_pAssignedList->set_width_request(m_pAssignedList->approximate_char_width() * 54);
    get( m_pRemoveBtn, "removebtn");
    get( m_pEditBtn, "editbtn");

    m_pParameterEdit->SetModifyHdl( LINK( this, SvxJavaParameterDlg, ModifyHdl_Impl ) );
    m_pAssignBtn->SetClickHdl( LINK( this, SvxJavaParameterDlg, AssignHdl_Impl ) );
    m_pRemoveBtn->SetClickHdl( LINK( this, SvxJavaParameterDlg, RemoveHdl_Impl ) );
    m_pEditBtn->SetClickHdl( LINK( this, SvxJavaParameterDlg, EditHdl_Impl ) );
    m_pAssignedList->SetSelectHdl( LINK( this, SvxJavaParameterDlg, SelectHdl_Impl ) );
    m_pAssignedList->SetDoubleClickHdl( LINK( this, SvxJavaParameterDlg, DblClickHdl_Impl ) );

    ModifyHdl_Impl( *m_pParameterEdit );
    EnableEditButton();
    EnableRemoveButton();
}

SvxJavaParameterDlg::~SvxJavaParameterDlg()
{
    disposeOnce();
}

void SvxJavaParameterDlg::dispose()
{
    m_pParameterEdit.clear();
    m_pAssignBtn.clear();
    m_pAssignedList.clear();
    m_pRemoveBtn.clear();
    m_pEditBtn.clear();
    ModalDialog::dispose();
}


IMPL_LINK_NOARG(SvxJavaParameterDlg, ModifyHdl_Impl, Edit&, void)
{
    OUString sParam = comphelper::string::strip(m_pParameterEdit->GetText(), ' ');
    m_pAssignBtn->Enable(!sParam.isEmpty());
}


IMPL_LINK_NOARG(SvxJavaParameterDlg, AssignHdl_Impl, Button*, void)
{
    OUString sParam = comphelper::string::strip(m_pParameterEdit->GetText(), ' ');
    if (!sParam.isEmpty())
    {
        sal_Int32 nPos = m_pAssignedList->GetEntryPos( sParam );
        if ( LISTBOX_ENTRY_NOTFOUND == nPos )
            nPos = m_pAssignedList->InsertEntry( sParam );
        m_pAssignedList->SelectEntryPos( nPos );
        m_pParameterEdit->SetText( OUString() );
        ModifyHdl_Impl( *m_pParameterEdit );
        EnableEditButton();
        EnableRemoveButton();
    }
}

IMPL_LINK_NOARG(SvxJavaParameterDlg, EditHdl_Impl, Button*, void)
{
    EditParameter();
}


IMPL_LINK_NOARG(SvxJavaParameterDlg, SelectHdl_Impl, ListBox&, void)
{
    EnableEditButton();
    EnableRemoveButton();
}


IMPL_LINK_NOARG(SvxJavaParameterDlg, DblClickHdl_Impl, ListBox&, void)
{
    EditParameter();
}


IMPL_LINK_NOARG(SvxJavaParameterDlg, RemoveHdl_Impl, Button*, void)
{
    sal_Int32 nPos = m_pAssignedList->GetSelectEntryPos();
    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
    {
        m_pAssignedList->RemoveEntry( nPos );
        sal_Int32 nCount = m_pAssignedList->GetEntryCount();
        if ( nCount )
        {
            if ( nPos >= nCount )
                nPos = ( nCount - 1 );
            m_pAssignedList->SelectEntryPos( nPos );
        }
        else
        {
            DisableEditButton();
        }
    }
    EnableRemoveButton();
}

void SvxJavaParameterDlg::EditParameter()
{
    sal_Int32 nPos = m_pAssignedList->GetSelectEntryPos();
    m_pParameterEdit->SetText( OUString() );

    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
    {
        ScopedVclPtrInstance< InputDialog > pParamEditDlg(CuiResId(RID_SVXSTR_JAVA_START_PARAM), this);
        OUString editableClassPath = m_pAssignedList->GetSelectEntry();
        pParamEditDlg->SetEntryText( editableClassPath );
        pParamEditDlg->HideHelpBtn();

        if(!pParamEditDlg->Execute())
            return;
        OUString editedClassPath = comphelper::string::strip( pParamEditDlg->GetEntryText(), ' ');

        if ( !editedClassPath.isEmpty() && editableClassPath != editedClassPath )
        {
            m_pAssignedList->RemoveEntry( nPos );
            m_pAssignedList->InsertEntry( editedClassPath, nPos );
            m_pAssignedList->SelectEntryPos( nPos );
        }
    }
}

short SvxJavaParameterDlg::Execute()
{
    m_pParameterEdit->GrabFocus();
    m_pAssignedList->SetNoSelection();
    return ModalDialog::Execute();
}


std::vector< OUString > SvxJavaParameterDlg::GetParameters() const
{
    sal_Int32 nCount = m_pAssignedList->GetEntryCount();
    std::vector< OUString > aParamList;
     for ( sal_Int32 i = 0; i < nCount; ++i )
         aParamList.push_back( m_pAssignedList->GetEntry(i) );
    return aParamList;
}


void SvxJavaParameterDlg::DisableButtons()
{
    DisableAssignButton();
    DisableEditButton();
    DisableRemoveButton();
}

void SvxJavaParameterDlg::SetParameters( std::vector< OUString > const & rParams )
{
    m_pAssignedList->Clear();
    for (auto const & sParam: rParams)
    {
        m_pAssignedList->InsertEntry( sParam );
    }
    DisableEditButton();
    DisableRemoveButton();
}


// class SvxJavaClassPathDlg ---------------------------------------------

SvxJavaClassPathDlg::SvxJavaClassPathDlg(vcl::Window* pParent)
    : ModalDialog(pParent, "JavaClassPath", "cui/ui/javaclasspathdialog.ui")
{
    get( m_pPathList, "paths");
    m_pPathList->SetDropDownLineCount(8);
    m_pPathList->set_width_request(m_pPathList->approximate_char_width() * 54);
    get( m_pAddArchiveBtn, "archive");
    get( m_pAddPathBtn, "folder");
    get( m_pRemoveBtn, "remove");

    m_pAddArchiveBtn->SetClickHdl( LINK( this, SvxJavaClassPathDlg, AddArchiveHdl_Impl ) );
    m_pAddPathBtn->SetClickHdl( LINK( this, SvxJavaClassPathDlg, AddPathHdl_Impl ) );
    m_pRemoveBtn->SetClickHdl( LINK( this, SvxJavaClassPathDlg, RemoveHdl_Impl ) );
    m_pPathList->SetSelectHdl( LINK( this, SvxJavaClassPathDlg, SelectHdl_Impl ) );

    // set initial focus to path list
    m_pPathList->GrabFocus();
}


SvxJavaClassPathDlg::~SvxJavaClassPathDlg()
{
    disposeOnce();
}

void SvxJavaClassPathDlg::dispose()
{
    if (m_pPathList)
    {
        sal_Int32 i, nCount = m_pPathList->GetEntryCount();
        for ( i = 0; i < nCount; ++i )
            delete static_cast< OUString* >( m_pPathList->GetEntryData(i) );
        m_pPathList = nullptr;
    }
    m_pPathList.clear();
    m_pAddArchiveBtn.clear();
    m_pAddPathBtn.clear();
    m_pRemoveBtn.clear();
    ModalDialog::dispose();
}

IMPL_LINK_NOARG(SvxJavaClassPathDlg, AddArchiveHdl_Impl, Button*, void)
{
    sfx2::FileDialogHelper aDlg( TemplateDescription::FILEOPEN_SIMPLE );
    aDlg.SetTitle( CuiResId( RID_SVXSTR_ARCHIVE_TITLE ) );
    aDlg.AddFilter( CuiResId( RID_SVXSTR_ARCHIVE_HEADLINE ), "*.jar;*.zip" );
    OUString sFolder;
    if ( m_pPathList->GetSelectEntryCount() > 0 )
    {
        INetURLObject aObj( m_pPathList->GetSelectEntry(), FSysStyle::Detect );
        sFolder = aObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );
    }
    else
         sFolder = SvtPathOptions().GetWorkPath();
    aDlg.SetDisplayDirectory( sFolder );
    if ( aDlg.Execute() == ERRCODE_NONE )
    {
        OUString sURL = aDlg.GetPath();
        INetURLObject aURL( sURL );
        OUString sFile = aURL.getFSysPath( FSysStyle::Detect );
        if ( !IsPathDuplicate( sURL ) )
        {
            sal_Int32 nPos = m_pPathList->InsertEntry( sFile, SvFileInformationManager::GetImage( aURL ) );
            m_pPathList->SelectEntryPos( nPos );
        }
        else
        {
            OUString sMsg( CuiResId( RID_SVXSTR_MULTIFILE_DBL_ERR ) );
            sMsg = sMsg.replaceFirst( "%1", sFile );
            ScopedVclPtrInstance<MessageDialog>(this, sMsg)->Execute();
        }
    }
    EnableRemoveButton();
}


IMPL_LINK_NOARG(SvxJavaClassPathDlg, AddPathHdl_Impl, Button*, void)
{
    Reference < XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
    Reference < XFolderPicker2 > xFolderPicker = FolderPicker::create(xContext);

    OUString sOldFolder;
    if ( m_pPathList->GetSelectEntryCount() > 0 )
    {
        INetURLObject aObj( m_pPathList->GetSelectEntry(), FSysStyle::Detect );
        sOldFolder = aObj.GetMainURL( INetURLObject::DecodeMechanism::NONE );
    }
    else
        sOldFolder = SvtPathOptions().GetWorkPath();
    xFolderPicker->setDisplayDirectory( sOldFolder );
    if ( xFolderPicker->execute() == ExecutableDialogResults::OK )
    {
        OUString sFolderURL( xFolderPicker->getDirectory() );
        INetURLObject aURL( sFolderURL );
        OUString sNewFolder = aURL.getFSysPath( FSysStyle::Detect );
        if ( !IsPathDuplicate( sFolderURL ) )
        {
            sal_Int32 nPos = m_pPathList->InsertEntry( sNewFolder, SvFileInformationManager::GetImage( aURL ) );
            m_pPathList->SelectEntryPos( nPos );
        }
        else
        {
            OUString sMsg( CuiResId( RID_SVXSTR_MULTIFILE_DBL_ERR ) );
            sMsg = sMsg.replaceFirst( "%1", sNewFolder );
            ScopedVclPtrInstance<MessageDialog>(this, sMsg)->Execute();
        }
    }
    EnableRemoveButton();
}


IMPL_LINK_NOARG(SvxJavaClassPathDlg, RemoveHdl_Impl, Button*, void)
{
    sal_Int32 nPos = m_pPathList->GetSelectEntryPos();
    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
    {
        m_pPathList->RemoveEntry( nPos );
        sal_Int32 nCount = m_pPathList->GetEntryCount();
        if ( nCount )
        {
            if ( nPos >= nCount )
                nPos = ( nCount - 1 );
            m_pPathList->SelectEntryPos( nPos );
        }
    }

    EnableRemoveButton();
}


IMPL_LINK_NOARG(SvxJavaClassPathDlg, SelectHdl_Impl, ListBox&, void)
{
    EnableRemoveButton();
}


bool SvxJavaClassPathDlg::IsPathDuplicate( const OUString& _rPath )
{
    bool bRet = false;
    INetURLObject aFileObj( _rPath );
    sal_Int32 nCount = m_pPathList->GetEntryCount();
    for ( sal_Int32 i = 0; i < nCount; ++i )
    {
        INetURLObject aOtherObj( m_pPathList->GetEntry(i), FSysStyle::Detect );
        if ( aOtherObj == aFileObj )
        {
            bRet = true;
            break;
        }
    }

    return bRet;
}


OUString SvxJavaClassPathDlg::GetClassPath() const
{
    OUString sPath;
    sal_Int32 nCount = m_pPathList->GetEntryCount();
    for ( sal_Int32 i = 0; i < nCount; ++i )
    {
        if ( !sPath.isEmpty() )
            sPath += OUStringLiteral1(CLASSPATH_DELIMITER);
        OUString* pFullPath = static_cast< OUString* >( m_pPathList->GetEntryData(i) );
        if ( pFullPath )
            sPath += *pFullPath;
        else
            sPath += m_pPathList->GetEntry(i);
    }
    return sPath;
}


void SvxJavaClassPathDlg::SetClassPath( const OUString& _rPath )
{
    if ( m_sOldPath.isEmpty() )
        m_sOldPath = _rPath;
    m_pPathList->Clear();
    sal_Int32 nIdx = 0;
    sal_Int32 nCount = comphelper::string::getTokenCount(_rPath, CLASSPATH_DELIMITER);
    for ( sal_Int32 i = 0; i < nCount; ++i )
    {
        OUString sToken = _rPath.getToken( 0, CLASSPATH_DELIMITER, nIdx );
        INetURLObject aURL( sToken, FSysStyle::Detect );
        OUString sPath = aURL.getFSysPath( FSysStyle::Detect );
        m_pPathList->InsertEntry( sPath, SvFileInformationManager::GetImage( aURL ) );
    }
    // select first entry
    m_pPathList->SelectEntryPos(0);
    SelectHdl_Impl( *m_pPathList );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
