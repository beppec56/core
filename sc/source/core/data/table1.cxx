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

#include "scitems.hxx"
#include <svx/algitem.hxx>
#include <editeng/justifyitem.hxx>
#include <unotools/textsearch.hxx>
#include <sfx2/objsh.hxx>

#include "attrib.hxx"
#include "patattr.hxx"
#include "formulacell.hxx"
#include "table.hxx"
#include "document.hxx"
#include "drwlayer.hxx"
#include "olinetab.hxx"
#include "stlsheet.hxx"
#include "global.hxx"
#include "globstr.hrc"
#include "refupdat.hxx"
#include "markdata.hxx"
#include "progress.hxx"
#include "hints.hxx"
#include "prnsave.hxx"
#include "tabprotection.hxx"
#include "sheetevents.hxx"
#include "segmenttree.hxx"
#include "dbdata.hxx"
#include "colorscale.hxx"
#include "conditio.hxx"
#include "globalnames.hxx"
#include "cellvalue.hxx"
#include "scmatrix.hxx"
#include "refupdatecontext.hxx"
#include <rowheightcontext.hxx>

#include <formula/vectortoken.hxx>

#include <vector>
#include <memory>

using ::std::vector;

namespace {

ScProgress* GetProgressBar(
    SCSIZE nCount, SCSIZE nTotalCount, ScProgress* pOuterProgress, ScDocument* pDoc)
{
    if (nTotalCount < 1000)
    {
        // if the total number of rows is less than 1000, don't even bother
        // with the progress bar because drawing progress bar can be very
        // expensive especially in GTK.
        return nullptr;
    }

    if (pOuterProgress)
        return pOuterProgress;

    if (nCount > 1)
        return new ScProgress(
            pDoc->GetDocumentShell(), ScGlobal::GetRscString(STR_PROGRESS_HEIGHTING), nTotalCount, true);

    return nullptr;
}

void GetOptimalHeightsInColumn(
    sc::RowHeightContext& rCxt, ScColContainer& rCol, SCROW nStartRow, SCROW nEndRow,
    ScProgress* pProgress, sal_uInt32 nProgressStart )
{
    assert(nStartRow <= nEndRow);

    //  first, one time over the whole range
    //  (with the last column in the hope that they most likely still are
    //  on standard format)


    rCol.back().GetOptimalHeight(rCxt, nStartRow, nEndRow, 0, 0);

    //  from there search for the standard height that is in use in the lower part

    ScFlatUInt16RowSegments& rHeights = rCxt.getHeightArray();
    sal_uInt16 nMinHeight = rHeights.getValue(nEndRow);
    SCSIZE nPos = nEndRow-1;
    ScFlatUInt16RowSegments::RangeData aRangeData;
    while ( nPos && rHeights.getRangeData(nPos-1, aRangeData) )
    {
        if (aRangeData.mnValue >= nMinHeight)
            nPos = std::max<SCSIZE>(0, aRangeData.mnRow1);
        else
            break;
    }

    SCROW nMinStart = nPos;

    sal_uLong nWeightedCount = 0;
    for (SCCOL nCol=0; nCol<(rCol.size()-1); nCol++)     // last col done already above
    {
        rCol[nCol].GetOptimalHeight(rCxt, nStartRow, nEndRow, nMinHeight, nMinStart);

        if (pProgress)
        {
            sal_uLong nWeight = rCol[nCol].GetWeightedCount();
            if (nWeight)        // does not have to be the same Status
            {
                nWeightedCount += nWeight;
                pProgress->SetState( nWeightedCount + nProgressStart );
            }
        }
    }
}

struct OptimalHeightsFuncObjBase
{
    virtual ~OptimalHeightsFuncObjBase() {}
    virtual bool operator() (SCROW nStartRow, SCROW nEndRow, sal_uInt16 nHeight) = 0;
};

struct SetRowHeightOnlyFunc : public OptimalHeightsFuncObjBase
{
    ScTable* mpTab;
    explicit SetRowHeightOnlyFunc(ScTable* pTab) :
        mpTab(pTab)
    {}

    virtual bool operator() (SCROW nStartRow, SCROW nEndRow, sal_uInt16 nHeight) override
    {
        mpTab->SetRowHeightOnly(nStartRow, nEndRow, nHeight);
        return false;
    }
};

struct SetRowHeightRangeFunc : public OptimalHeightsFuncObjBase
{
    ScTable* mpTab;
    double mnPPTY;

    SetRowHeightRangeFunc(ScTable* pTab, double nPPTY) :
        mpTab(pTab),
        mnPPTY(nPPTY)
    {}

    virtual bool operator() (SCROW nStartRow, SCROW nEndRow, sal_uInt16 nHeight) override
    {
        return mpTab->SetRowHeightRange(nStartRow, nEndRow, nHeight, mnPPTY);
    }
};

bool SetOptimalHeightsToRows(
    sc::RowHeightContext& rCxt,
    OptimalHeightsFuncObjBase& rFuncObj,
    ScBitMaskCompressedArray<SCROW, CRFlags>* pRowFlags, SCROW nStartRow, SCROW nEndRow )
{
    bool bChanged = false;
    SCROW nRngStart = 0;
    SCROW nRngEnd = 0;
    sal_uInt16 nLast = 0;
    sal_uInt16 nExtraHeight = rCxt.getExtraHeight();
    for (SCSIZE i = nStartRow; i <= static_cast<SCSIZE>(nEndRow); i++)
    {
        size_t nIndex;
        SCROW nRegionEndRow;
        CRFlags nRowFlag = pRowFlags->GetValue( i, nIndex, nRegionEndRow );
        if ( nRegionEndRow > nEndRow )
            nRegionEndRow = nEndRow;
        SCSIZE nMoreRows = nRegionEndRow - i;     // additional equal rows after first

        bool bAutoSize = !(nRowFlag & CRFlags::ManualSize);
        if (bAutoSize || rCxt.isForceAutoSize())
        {
            if (nExtraHeight)
            {
                if (bAutoSize)
                    pRowFlags->SetValue( i, nRegionEndRow, nRowFlag | CRFlags::ManualSize);
            }
            else if (!bAutoSize)
                pRowFlags->SetValue( i, nRegionEndRow, nRowFlag & ~CRFlags::ManualSize);

            for (SCSIZE nInner = i; nInner <= i + nMoreRows; ++nInner)
            {
                if (nLast)
                {
                    ScFlatUInt16RowSegments::RangeData aRangeData;
                    (void)rCxt.getHeightArray().getRangeData(nInner, aRangeData);
                    if (aRangeData.mnValue + nExtraHeight == nLast)
                    {
                        nRngEnd = std::min<SCSIZE>(i + nMoreRows, aRangeData.mnRow2);
                        nInner = aRangeData.mnRow2;
                    }
                    else
                    {
                        bChanged |= rFuncObj(nRngStart, nRngEnd, nLast);
                        nLast = 0;
                    }
                }
                if (!nLast)
                {
                    nLast = rCxt.getHeightArray().getValue(nInner) + rCxt.getExtraHeight();
                    nRngStart = nInner;
                    nRngEnd = nInner;
                }
            }
        }
        else
        {
            if (nLast)
                bChanged |= rFuncObj(nRngStart, nRngEnd, nLast);
            nLast = 0;
        }
        i += nMoreRows;     // already handled - skip
    }
    if (nLast)
        bChanged |= rFuncObj(nRngStart, nRngEnd, nLast);

    return bChanged;
}

}

ScTable::ScTable( ScDocument* pDoc, SCTAB nNewTab, const OUString& rNewName,
                    bool bColInfo, bool bRowInfo ) :
    aCol( pDoc, MAXCOLCOUNT ),
    aName( rNewName ),
    aCodeName( rNewName ),
    nLinkRefreshDelay( 0 ),
    nLinkMode( ScLinkMode::NONE ),
    aPageStyle( ScGlobal::GetRscString(STR_STYLENAME_STANDARD) ),
    nRepeatStartX( SCCOL_REPEAT_NONE ),
    nRepeatEndX( SCCOL_REPEAT_NONE ),
    nRepeatStartY( SCROW_REPEAT_NONE ),
    nRepeatEndY( SCROW_REPEAT_NONE ),
    pTabProtection( nullptr ),
    pColWidth( nullptr ),
    mpRowHeights( static_cast<ScFlatUInt16RowSegments*>(nullptr) ),
    pColFlags( nullptr ),
    pRowFlags( nullptr ),
    mpHiddenCols(new ScFlatBoolColSegments),
    mpHiddenRows(new ScFlatBoolRowSegments),
    mpFilteredCols(new ScFlatBoolColSegments),
    mpFilteredRows(new ScFlatBoolRowSegments),
    pOutlineTable( nullptr ),
    pSheetEvents( nullptr ),
    nTableAreaX( 0 ),
    nTableAreaY( 0 ),
    nTab( nNewTab ),
    pDocument( pDoc ),
    pSearchText ( nullptr ),
    pSortCollator( nullptr ),
    pRepeatColRange( nullptr ),
    pRepeatRowRange( nullptr ),
    nLockCount( 0 ),
    pScenarioRanges( nullptr ),
    aScenarioColor( COL_LIGHTGRAY ),
    aTabBgColor( COL_AUTO ),
    nScenarioFlags(ScScenarioFlags::NONE),
    pDBDataNoName(nullptr),
    mpRangeName(nullptr),
    mpCondFormatList( new ScConditionalFormatList() ),
    bScenario(false),
    bLayoutRTL(false),
    bLoadingRTL(false),
    bPageSizeValid(false),
    bTableAreaValid(false),
    bVisible(true),
    bStreamValid(false),
    bPendingRowHeights(false),
    bCalcNotification(false),
    bGlobalKeepQuery(false),
    bPrintEntireSheet(true),
    bActiveScenario(false),
    mbPageBreaksValid(false),
    mbForceBreaks(false),
    aDefaultColAttrArray(static_cast<SCCOL>(-1), nNewTab, pDoc, nullptr)
{

    if (bColInfo)
    {
        pColWidth  = new sal_uInt16[ MAXCOL+1 ];
        pColFlags  = new CRFlags[ MAXCOL+1 ];

        for (SCCOL i=0; i<=MAXCOL; i++)
        {
            pColWidth[i] = STD_COL_WIDTH;
            pColFlags[i] = CRFlags::NONE;
        }
    }

    if (bRowInfo)
    {
        mpRowHeights.reset(new ScFlatUInt16RowSegments(ScGlobal::nStdRowHeight));
        pRowFlags  = new ScBitMaskCompressedArray<SCROW, CRFlags>( MAXROW, CRFlags::NONE);
    }

    if ( pDocument->IsDocVisible() )
    {
        //  when a sheet is added to a visible document,
        //  initialize its RTL flag from the system locale
        bLayoutRTL = ScGlobal::IsSystemRTL();
    }

    ScDrawLayer* pDrawLayer = pDocument->GetDrawLayer();
    if (pDrawLayer)
    {
        if ( pDrawLayer->ScAddPage( nTab ) )    // sal_False (not inserted) during Undo
        {
            pDrawLayer->ScRenamePage( nTab, aName );
            sal_uLong nx = (sal_uLong) ((double) (MAXCOL+1) * STD_COL_WIDTH           * HMM_PER_TWIPS );
            sal_uLong ny = (sal_uLong) ((double) (MAXROW+1) * ScGlobal::nStdRowHeight * HMM_PER_TWIPS );
            pDrawLayer->SetPageSize( static_cast<sal_uInt16>(nTab), Size( nx, ny ), false );
        }
    }

    for (SCCOL k=0; k < aCol.size(); k++)
        aCol[k].Init( k, nTab, pDocument, true );
}

ScTable::~ScTable() COVERITY_NOEXCEPT_FALSE
{
    if (!pDocument->IsInDtorClear())
    {
        for (SCCOL nCol = 0; nCol < (aCol.size() - 1); ++nCol)
        {
            aCol[nCol].FreeNotes();
        }
        //  In the dtor, don't delete the pages in the wrong order.
        //  (or else nTab does not reflect the page number!)
        //  In ScDocument::Clear is afterwards used from Clear at the Draw Layer to delete everything.

        ScDrawLayer* pDrawLayer = pDocument->GetDrawLayer();
        if (pDrawLayer)
            pDrawLayer->ScRemovePage( nTab );
    }

    delete[] pColWidth;
    delete[] pColFlags;
    delete pRowFlags;
    delete pSheetEvents;
    delete pOutlineTable;
    delete pSearchText;
    delete pRepeatColRange;
    delete pRepeatRowRange;
    delete pScenarioRanges;
    delete mpRangeName;
    delete pDBDataNoName;
    DestroySortCollator();
}

sal_Int64 ScTable::GetHashCode() const
{
    return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_IntPtr>(this));
}

void ScTable::GetName( OUString& rName ) const
{
    rName = aName;
}

void ScTable::SetName( const OUString& rNewName )
{
    aName = rNewName;
    aUpperName.clear(); // invalidated if the name is changed

    // SetStreamValid is handled in ScDocument::RenameTab
}

const OUString& ScTable::GetUpperName() const
{
    if (aUpperName.isEmpty() && !aName.isEmpty())
        aUpperName = ScGlobal::pCharClass->uppercase(aName);
    return aUpperName;
}

void ScTable::SetVisible( bool bVis )
{
    if (bVisible != bVis && IsStreamValid())
        SetStreamValid(false);

    bVisible = bVis;
}

void ScTable::SetStreamValid( bool bSet, bool bIgnoreLock )
{
    if ( bIgnoreLock || !pDocument->IsStreamValidLocked() )
        bStreamValid = bSet;
}

void ScTable::SetPendingRowHeights( bool bSet )
{
    bPendingRowHeights = bSet;
}

void ScTable::SetLayoutRTL( bool bSet )
{
    bLayoutRTL = bSet;
}

void ScTable::SetLoadingRTL( bool bSet )
{
    bLoadingRTL = bSet;
}

void ScTable::SetTabBgColor(const Color& rColor)
{
    if (aTabBgColor != rColor)
    {
        // The tab color has changed.  Set this table 'modified'.
        aTabBgColor = rColor;
        if (IsStreamValid())
            SetStreamValid(false);
    }
}

void ScTable::SetScenario( bool bFlag )
{
    bScenario = bFlag;
}

void ScTable::SetLink( ScLinkMode nMode,
                        const OUString& rDoc, const OUString& rFlt, const OUString& rOpt,
                        const OUString& rTab, sal_uLong nRefreshDelay )
{
    nLinkMode = nMode;
    aLinkDoc = rDoc;        // File
    aLinkFlt = rFlt;        // Filter
    aLinkOpt = rOpt;        // Filter options
    aLinkTab = rTab;        // Sheet name in source file
    nLinkRefreshDelay = nRefreshDelay;  // refresh delay in seconds, 0==off

    if (IsStreamValid())
        SetStreamValid(false);
}

sal_uInt16 ScTable::GetOptimalColWidth( SCCOL nCol, OutputDevice* pDev,
                                    double nPPTX, double nPPTY,
                                    const Fraction& rZoomX, const Fraction& rZoomY,
                                    bool bFormula, const ScMarkData* pMarkData,
                                    const ScColWidthParam* pParam )
{
    if ( nCol >= aCol.size() )
        return ( STD_COL_WIDTH - STD_EXTRA_WIDTH );

    return aCol[nCol].GetOptimalColWidth( pDev, nPPTX, nPPTY, rZoomX, rZoomY,
        bFormula, STD_COL_WIDTH - STD_EXTRA_WIDTH, pMarkData, pParam );
}

long ScTable::GetNeededSize( SCCOL nCol, SCROW nRow,
                                OutputDevice* pDev,
                                double nPPTX, double nPPTY,
                                const Fraction& rZoomX, const Fraction& rZoomY,
                                bool bWidth, bool bTotalSize )
{
    if ( nCol >= aCol.size() )
        return 0;

    ScNeededSizeOptions aOptions;
    aOptions.bSkipMerged = false;       // count merged cells
    aOptions.bTotalSize  = bTotalSize;

    return aCol[nCol].GetNeededSize
        ( nRow, pDev, nPPTX, nPPTY, rZoomX, rZoomY, bWidth, aOptions, nullptr );
}

bool ScTable::SetOptimalHeight(
    sc::RowHeightContext& rCxt, SCROW nStartRow, SCROW nEndRow,
    ScProgress* pOuterProgress, sal_uLong nProgressStart )
{
    assert(nStartRow <= nEndRow);

    OSL_ENSURE( rCxt.getExtraHeight() == 0 || rCxt.isForceAutoSize(),
        "automatic OptimalHeight with Extra" );

    if ( !pDocument->IsAdjustHeightEnabled() )
    {
        return false;
    }

    SCSIZE  nCount = static_cast<SCSIZE>(nEndRow-nStartRow+1);

    ScProgress* pProgress = GetProgressBar(nCount, GetWeightedCount(), pOuterProgress, pDocument);

    GetOptimalHeightsInColumn(rCxt, aCol, nStartRow, nEndRow, pProgress, nProgressStart);

    rCxt.getHeightArray().enableTreeSearch(true);
    SetRowHeightRangeFunc aFunc(this, rCxt.getPPTY());
    bool bChanged = SetOptimalHeightsToRows(rCxt, aFunc, pRowFlags, nStartRow, nEndRow);

    if ( pProgress != pOuterProgress )
        delete pProgress;

    return bChanged;
}

void ScTable::SetOptimalHeightOnly(
    sc::RowHeightContext& rCxt, SCROW nStartRow, SCROW nEndRow,
    ScProgress* pOuterProgress, sal_uLong nProgressStart )
{
    OSL_ENSURE( rCxt.getExtraHeight() == 0 || rCxt.isForceAutoSize(),
        "automatic OptimalHeight with Extra" );

    if ( !pDocument->IsAdjustHeightEnabled() )
        return;

    SCSIZE  nCount = static_cast<SCSIZE>(nEndRow-nStartRow+1);

    ScProgress* pProgress = GetProgressBar(nCount, GetWeightedCount(), pOuterProgress, pDocument);

    GetOptimalHeightsInColumn(rCxt, aCol, nStartRow, nEndRow, pProgress, nProgressStart);

    SetRowHeightOnlyFunc aFunc(this);

    rCxt.getHeightArray().enableTreeSearch(true);
    SetOptimalHeightsToRows(rCxt, aFunc, pRowFlags, nStartRow, nEndRow);

    if ( pProgress != pOuterProgress )
        delete pProgress;
}

bool ScTable::GetCellArea( SCCOL& rEndCol, SCROW& rEndRow ) const
{
    bool bFound = false;
    SCCOL nMaxX = 0;
    SCROW nMaxY = 0;
    for (SCCOL i=0; i<aCol.size(); i++)
        {
            if (!aCol[i].IsEmptyData())
            {
                bFound = true;
                nMaxX = i;
                SCROW nRow = aCol[i].GetLastDataPos();
                if (nRow > nMaxY)
                    nMaxY = nRow;
            }
            if ( aCol[i].HasCellNotes() )
            {
                SCROW maxNoteRow = aCol[i].GetCellNotesMaxRow();
                if (maxNoteRow >= nMaxY)
                {
                    bFound = true;
                    nMaxY = maxNoteRow;
                }
                if (i>nMaxX)
                {
                    bFound = true;
                    nMaxX = i;
                }
            }
        }

    rEndCol = nMaxX;
    rEndRow = nMaxY;
    return bFound;
}

bool ScTable::GetTableArea( SCCOL& rEndCol, SCROW& rEndRow ) const
{
    bool bRet = true;               //TODO: remember?
    if (!bTableAreaValid)
    {
        bRet = GetPrintArea(nTableAreaX, nTableAreaY, true);
        bTableAreaValid = true;
    }
    rEndCol = nTableAreaX;
    rEndRow = nTableAreaY;
    return bRet;
}

const SCCOL SC_COLUMNS_STOP = 30;

bool ScTable::GetPrintArea( SCCOL& rEndCol, SCROW& rEndRow, bool bNotes ) const
{
    bool bFound = false;
    SCCOL nMaxX = 0;
    SCROW nMaxY = 0;
    SCCOL i;

    for (i=0; i<aCol.size(); i++)               // Test data
        {
            if (!aCol[i].IsEmptyData())
            {
                bFound = true;
                if (i>nMaxX)
                    nMaxX = i;
                SCROW nColY = aCol[i].GetLastDataPos();
                if (nColY > nMaxY)
                    nMaxY = nColY;
            }
            if (bNotes)
            {
                if ( aCol[i].HasCellNotes() )
                {
                    SCROW maxNoteRow = aCol[i].GetCellNotesMaxRow();
                    if (maxNoteRow >= nMaxY)
                    {
                        bFound = true;
                        nMaxY = maxNoteRow;
                    }
                    if (i>nMaxX)
                    {
                        bFound = true;
                        nMaxX = i;
                    }
                }
            }
        }

    SCCOL nMaxDataX = nMaxX;

    for (i=0; i<aCol.size(); i++)               // Test attribute
    {
        SCROW nLastRow;
        if (aCol[i].GetLastVisibleAttr( nLastRow ))
        {
            bFound = true;
            nMaxX = i;
            if (nLastRow > nMaxY)
                nMaxY = nLastRow;
        }
    }

    if (nMaxX == MAXCOL)                    // omit attribute at the right
    {
        --nMaxX;
        while ( nMaxX>0 && aCol[nMaxX].IsVisibleAttrEqual(aCol[nMaxX+1]) )
            --nMaxX;
    }

    if ( nMaxX < nMaxDataX )
    {
        nMaxX = nMaxDataX;
    }
    else if ( nMaxX > nMaxDataX )
    {
        SCCOL nAttrStartX = nMaxDataX + 1;
        while ( nAttrStartX < (aCol.size()-1) )
        {
            SCCOL nAttrEndX = nAttrStartX;
            while ( nAttrEndX < (aCol.size()-1) && aCol[nAttrStartX].IsVisibleAttrEqual(aCol[nAttrEndX+1]) )
                ++nAttrEndX;
            if ( nAttrEndX + 1 - nAttrStartX >= SC_COLUMNS_STOP )
            {
                // found equally-formatted columns behind data -> stop before these columns
                nMaxX = nAttrStartX - 1;

                // also don't include default-formatted columns before that
                SCROW nDummyRow;
                while ( nMaxX > nMaxDataX && !aCol[nMaxX].GetLastVisibleAttr( nDummyRow ) )
                    --nMaxX;
                break;
            }
            nAttrStartX = nAttrEndX + 1;
        }
    }

    rEndCol = nMaxX;
    rEndRow = nMaxY;
    return bFound;
}

bool ScTable::GetPrintAreaHor( SCROW nStartRow, SCROW nEndRow,
                                SCCOL& rEndCol ) const
{
    bool bFound = false;
    SCCOL nMaxX = 0;
    SCCOL i;

    for (i=0; i<aCol.size(); i++)               // Test attribute
    {
        if (aCol[i].HasVisibleAttrIn( nStartRow, nEndRow ))
        {
            bFound = true;
            nMaxX = i;
        }
    }

    if (nMaxX == MAXCOL)                    // omit attribute at the right
    {
        --nMaxX;
        while ( nMaxX>0 && aCol[nMaxX].IsVisibleAttrEqual(aCol[nMaxX+1], nStartRow, nEndRow) )
            --nMaxX;
    }

    for (i=0; i<aCol.size(); i++)               // Daten testen
    {
        if (!aCol[i].IsEmptyBlock( nStartRow, nEndRow ))        //TODO: bNotes ??????
        {
            bFound = true;
            if (i>nMaxX)
                nMaxX = i;
        }
    }

    rEndCol = nMaxX;
    return bFound;
}

bool ScTable::GetPrintAreaVer( SCCOL nStartCol, SCCOL nEndCol,
                                SCROW& rEndRow, bool bNotes ) const
{
    nStartCol = std::min<SCCOL>( nStartCol, aCol.size()-1 );
    nEndCol   = std::min<SCCOL>( nEndCol,   aCol.size()-1 );
    bool bFound = false;
    SCROW nMaxY = 0;
    SCCOL i;

    for (i=nStartCol; i<=nEndCol; i++)              // Test attribute
    {
        SCROW nLastRow;
        if (aCol[i].GetLastVisibleAttr( nLastRow ))
        {
            bFound = true;
            if (nLastRow > nMaxY)
                nMaxY = nLastRow;
        }
    }

    for (i=nStartCol; i<=nEndCol; i++)              // Test data
    {
        if (!aCol[i].IsEmptyData())
        {
            bFound = true;
            SCROW nColY = aCol[i].GetLastDataPos();
            if (nColY > nMaxY)
                nMaxY = nColY;
        }
        if (bNotes)
        {
            if ( aCol[i].HasCellNotes() )
            {
                SCROW maxNoteRow =aCol[i].GetCellNotesMaxRow();
                if (maxNoteRow > nMaxY)
                {
                    bFound = true;
                    nMaxY = maxNoteRow;
                }
            }
        }
    }

    rEndRow = nMaxY;
    return bFound;
}

bool ScTable::GetDataStart( SCCOL& rStartCol, SCROW& rStartRow ) const
{
    bool bFound = false;
    SCCOL nMinX = aCol.size()-1;
    SCROW nMinY = MAXROW;
    SCCOL i;

    for (i=0; i<aCol.size(); i++)                   // Test attribute
    {
        SCROW nFirstRow;
        if (aCol[i].GetFirstVisibleAttr( nFirstRow ))
        {
            if (!bFound)
                nMinX = i;
            bFound = true;
            if (nFirstRow < nMinY)
                nMinY = nFirstRow;
        }
    }

    if (nMinX == 0)                                     // omit attribute at the right
    {
        if ( aCol.size() > 1 && aCol[0].IsVisibleAttrEqual(aCol[1]) )      // no single ones
        {
            ++nMinX;
            while ( nMinX<(aCol.size()-1) && aCol[nMinX].IsVisibleAttrEqual(aCol[nMinX-1]) )
                ++nMinX;
        }
    }

    bool bDatFound = false;
    for (i=0; i<aCol.size(); i++)                   // Test data
    {
        if (!aCol[i].IsEmptyData())
        {
            if (!bDatFound && i<nMinX)
                nMinX = i;
            bFound = bDatFound = true;
            SCROW nRow = aCol[i].GetFirstDataPos();
            if (nRow < nMinY)
                nMinY = nRow;
        }
        if ( aCol[i].HasCellNotes() )
        {
            SCROW minNoteRow = aCol[i].GetCellNotesMinRow();
            if (minNoteRow <= nMinY)
            {
                bFound = true;
                nMinY = minNoteRow;
            }
            if (i<nMinX)
            {
                bFound = true;
                nMinX = i;
            }
        }
    }
    rStartCol = nMinX;
    rStartRow = nMinY;
    return bFound;
}

void ScTable::GetDataArea( SCCOL& rStartCol, SCROW& rStartRow, SCCOL& rEndCol, SCROW& rEndRow,
                           bool bIncludeOld, bool bOnlyDown ) const
{
    // return the smallest area containing at least all contiguous cells having data. This area
    // is a square containing also empty cells. It may shrink or extend the area given as input
    // Flags as modifiers:
    //
    //     bIncludeOld = true ensure that the returned area contains at least the initial area,
    //                   independently of the emptiness of rows / columns (i.e. does not allow shrinking)
    //     bOnlyDown = true means extend / shrink the inputted area only down, i.e modify only rEndRow

    rStartCol = std::min<SCCOL>( rStartCol, aCol.size()-1 );
    rEndCol   = std::min<SCCOL>( rEndCol,   aCol.size()-1 );

    bool bLeft = false;
    bool bRight  = false;
    bool bTop = false;
    bool bBottom = false;
    bool bChanged = false;

    do
    {
        bChanged = false;

        if (!bOnlyDown)
        {
            SCROW nStart = rStartRow;
            SCROW nEnd = rEndRow;
            if (nStart>0) --nStart;
            if (nEnd<MAXROW) ++nEnd;

            if (rEndCol < (aCol.size()-1))
                if (!aCol[rEndCol+1].IsEmptyBlock(nStart,nEnd))
                {
                    ++rEndCol;
                    bChanged = true;
                    bRight = true;
                }

            if (rStartCol > 0)
                if (!aCol[rStartCol-1].IsEmptyBlock(nStart,nEnd))
                {
                    --rStartCol;
                    bChanged = true;
                    bLeft = true;
                }

            if (rStartRow > 0)
            {
                SCROW nTest = rStartRow-1;
                bool needExtend = false;
                for ( SCCOL i = rStartCol; i<=rEndCol && !needExtend; i++)
                    if (aCol[i].HasDataAt(nTest))
                        needExtend = true;
                if (needExtend)
                {
                    --rStartRow;
                    bChanged = true;
                    bTop = true;
                }
            }
        }

        if (rEndRow < MAXROW)
        {
            SCROW nTest = rEndRow+1;
            bool needExtend = false;
            for ( SCCOL i = rStartCol; i<=rEndCol && !needExtend; i++)
                if (aCol[i].HasDataAt(nTest))
                    needExtend = true;
            if (needExtend)
            {
                ++rEndRow;
                bChanged = true;
                bBottom = true;
            }
        }
    }
    while( bChanged );

    if ( !bIncludeOld && !bOnlyDown )
    {
        if ( !bLeft )
            while ( rStartCol < rEndCol && rStartCol < (aCol.size()-1) && aCol[rStartCol].IsEmptyBlock(rStartRow,rEndRow) )
                ++rStartCol;

        if ( !bRight )
            while ( rEndCol > 0 && rStartCol < rEndCol && aCol[rEndCol].IsEmptyBlock(rStartRow,rEndRow) )
                --rEndCol;

        if ( !bTop && rStartRow < MAXROW && rStartRow < rEndRow )
        {
            bool bShrink = true;
            do
            {
                for ( SCCOL i = rStartCol; i<=rEndCol && bShrink; i++)
                    if (aCol[i].HasDataAt(rStartRow))
                        bShrink = false;
                if (bShrink)
                    ++rStartRow;
            } while (bShrink && rStartRow < MAXROW && rStartRow < rEndRow);
        }
    }

    if ( !bIncludeOld )
    {
        if ( !bBottom && rEndRow > 0 && rStartRow < rEndRow )
        {
            SCROW nLastDataRow = GetLastDataRow( rStartCol, rEndCol, rEndRow);
            if (nLastDataRow < rEndRow)
                rEndRow = std::max( rStartRow, nLastDataRow);
        }
    }
}

bool ScTable::ShrinkToUsedDataArea( bool& o_bShrunk, SCCOL& rStartCol, SCROW& rStartRow,
        SCCOL& rEndCol, SCROW& rEndRow, bool bColumnsOnly, bool bStickyTopRow, bool bStickyLeftCol,
        bool bConsiderCellNotes ) const
{
    rStartCol = std::min<SCCOL>( rStartCol, aCol.size()-1 );
    // check for rEndCol is done below.

    o_bShrunk = false;

    PutInOrder( rStartCol, rEndCol);
    PutInOrder( rStartRow, rEndRow);
    if (rStartCol < 0)
    {
        rStartCol = 0;
        o_bShrunk = true;
    }
    if (rStartRow < 0)
    {
        rStartRow = 0;
        o_bShrunk = true;
    }
    if (rEndCol >= aCol.size())
    {
        rEndCol = aCol.size()-1;
        o_bShrunk = true;
    }
    if (rEndRow > MAXROW)
    {
        rEndRow = MAXROW;
        o_bShrunk = true;
    }

    while (rStartCol < rEndCol)
    {
        if (aCol[rEndCol].IsEmptyBlock( rStartRow, rEndRow))
        {
            if (bConsiderCellNotes && !aCol[rEndCol].IsNotesEmptyBlock( rStartRow, rEndRow ))
                break;
            --rEndCol;
            o_bShrunk = true;
        }
        else
            break;  // while
    }

    if (!bStickyLeftCol)
    {
        while (rStartCol < rEndCol)
        {
            if (aCol[rStartCol].IsEmptyBlock( rStartRow, rEndRow))
            {
                if (bConsiderCellNotes && !aCol[rStartCol].IsNotesEmptyBlock( rStartRow, rEndRow ))
                    break;

                ++rStartCol;
                o_bShrunk = true;
            }
            else
                break;  // while
        }
    }

    if (!bColumnsOnly)
    {
        if (!bStickyTopRow)
        {
            while (rStartRow < rEndRow)
            {
                bool bFound = false;
                for (SCCOL i=rStartCol; i<=rEndCol && !bFound; i++)
                {
                    if (aCol[i].HasDataAt( rStartRow))
                        bFound = true;
                }
                if (!bFound)
                {
                    ++rStartRow;
                    o_bShrunk = true;
                }
                else
                    break;  // while
            }
        }

        while (rStartRow < rEndRow)
        {
            SCROW nLastDataRow = GetLastDataRow( rStartCol, rEndCol, rEndRow);
            if (0 <= nLastDataRow && nLastDataRow < rEndRow)
            {
                rEndRow = std::max( rStartRow, nLastDataRow);
                o_bShrunk = true;
            }
            else
                break;  // while
        }
    }

    return rStartCol != rEndCol || (bColumnsOnly ?
            !aCol[rStartCol].IsEmptyBlock( rStartRow, rEndRow) :
            (rStartRow != rEndRow || aCol[rStartCol].HasDataAt( rStartRow)));
}

SCROW ScTable::GetLastDataRow( SCCOL nCol1, SCCOL nCol2, SCROW nLastRow ) const
{
    if ( !IsColValid( nCol1 ) || !ValidCol( nCol2 ) )
        return -1;

    nCol2 = std::min<SCCOL>( nCol2, aCol.size() - 1 );

    SCROW nNewLastRow = 0;
    for (SCCOL i = nCol1; i <= nCol2; ++i)
    {
        SCROW nThis = aCol[i].GetLastDataPos(nLastRow);
        if (nNewLastRow < nThis)
            nNewLastRow = nThis;
    }

    return nNewLastRow;
}

SCSIZE ScTable::GetEmptyLinesInBlock( SCCOL nStartCol, SCROW nStartRow,
                                        SCCOL nEndCol, SCROW nEndRow, ScDirection eDir ) const
{
    SCCOL nStartColOrig = nStartCol;
    SCCOL nEndColOrig   = nEndCol;
    nStartCol = std::min<SCCOL>( nStartCol, aCol.size()-1 );
    nEndCol   = std::min<SCCOL>( nEndCol,   aCol.size()-1 );

    // The region is not allocated and does not contain any data.
    if ( nStartColOrig != nStartCol )
        return ( ((eDir == DIR_BOTTOM) || (eDir == DIR_TOP)) ?
                 static_cast<SCSIZE>(nEndRow - nStartRow + 1) :
                 static_cast<SCSIZE>(nEndColOrig - nStartColOrig + 1) );

    SCSIZE nGapRight = static_cast<SCSIZE>(nEndColOrig - nEndCol);
    SCSIZE nCount = 0;
    SCCOL nCol;
    if ((eDir == DIR_BOTTOM) || (eDir == DIR_TOP))
    {
        nCount = static_cast<SCSIZE>(nEndRow - nStartRow + 1);
        for (nCol = nStartCol; nCol <= nEndCol; nCol++)
            nCount = std::min(nCount, aCol[nCol].GetEmptyLinesInBlock(nStartRow, nEndRow, eDir));
    }
    else if (eDir == DIR_RIGHT)
    {
        nCol = nEndCol;
        while ((nCol >= nStartCol) &&
                 aCol[nCol].IsEmptyBlock(nStartRow, nEndRow))
        {
            nCount++;
            nCol--;
        }
        nCount += nGapRight;
    }
    else
    {
        nCol = nStartCol;
        while ((nCol <= nEndCol) && aCol[nCol].IsEmptyBlock(nStartRow, nEndRow))
        {
            nCount++;
            nCol++;
        }

        // If the area between nStartCol and nEndCol are empty,
        // add the count of unallocated columns on the right.
        if ( nCol > nEndCol )
            nCount += nGapRight;
    }
    return nCount;
}

bool ScTable::IsEmptyLine( SCROW nRow, SCCOL nStartCol, SCCOL nEndCol ) const
{
    // The range of columns are unallocated hence empty.
    if ( nStartCol >= aCol.size() )
        return true;

    nEndCol   = std::min<SCCOL>( nEndCol,   aCol.size()-1 );

    bool bFound = false;
    for (SCCOL i=nStartCol; i<=nEndCol && !bFound; i++)
        if (aCol[i].HasDataAt(nRow))
            bFound = true;
    return !bFound;
}

void ScTable::LimitChartArea( SCCOL& rStartCol, SCROW& rStartRow, SCCOL& rEndCol, SCROW& rEndRow ) const
{
    rStartCol = std::min<SCCOL>( rStartCol, aCol.size()-1 );
    rEndCol   = std::min<SCCOL>( rEndCol,   aCol.size()-1 );

    while ( rStartCol<rEndCol && aCol[rStartCol].IsEmptyBlock(rStartRow,rEndRow) )
        ++rStartCol;

    while ( rStartCol<rEndCol && aCol[rEndCol].IsEmptyBlock(rStartRow,rEndRow) )
        --rEndCol;

    while ( rStartRow<rEndRow && IsEmptyLine(rStartRow, rStartCol, rEndCol) )
        ++rStartRow;

    while ( rStartRow<rEndRow && IsEmptyLine(rEndRow, rStartCol, rEndCol) )
        --rEndRow;
}

SCCOL ScTable::FindNextVisibleCol( SCCOL nCol, bool bRight ) const
{
    if(bRight)
    {
        nCol++;
        SCCOL nEnd = 0;
        bool bHidden = pDocument->ColHidden(nCol, nTab, nullptr, &nEnd);
        if(bHidden)
            nCol = nEnd +1;

        return std::min<SCCOL>(MAXCOL, nCol);
    }
    else
    {
        nCol--;
        SCCOL nStart = MAXCOL;
        bool bHidden = pDocument->ColHidden(nCol, nTab, &nStart);
        if(bHidden)
            nCol = nStart - 1;

        return std::max<SCCOL>(0, nCol);
    }
}

SCCOL ScTable::FindNextVisibleColWithContent( SCCOL nCol, bool bRight, SCROW nRow ) const
{
    if(bRight)
    {
        if(nCol == MAXCOL)
            return MAXCOL;

        do
        {
            nCol++;
            SCCOL nEndCol = 0;
            bool bHidden = pDocument->ColHidden( nCol, nTab, nullptr, &nEndCol );
            if(bHidden)
            {
                nCol = nEndCol +1;
                if(nEndCol >= MAXCOL)
                    return MAXCOL;
            }

            if(aCol[nCol].HasVisibleDataAt(nRow))
                return nCol;
        }
        while(nCol < MAXCOL);

        return MAXCOL;
    }
    else
    {
        if(nCol == 0)
            return 0;

        do
        {
            nCol--;
            SCCOL nStartCol = MAXCOL;
            bool bHidden = pDocument->ColHidden( nCol, nTab, &nStartCol );
            if(bHidden)
            {
                nCol = nStartCol -1;
                if(nCol <= 0)
                    return 0;
            }

            if(aCol[nCol].HasVisibleDataAt(nRow))
                return nCol;
        }
        while(nCol > 0);

        return 0;
    }
}

void ScTable::FindAreaPos( SCCOL& rCol, SCROW& rRow, ScMoveDirection eDirection ) const
{
    if (eDirection == SC_MOVE_LEFT || eDirection == SC_MOVE_RIGHT)
    {
        SCCOL nNewCol = rCol;
        bool bThere = aCol[nNewCol].HasVisibleDataAt(rRow);
        bool bRight = (eDirection == SC_MOVE_RIGHT);
        if (bThere)
        {
            if(nNewCol >= MAXCOL && eDirection == SC_MOVE_RIGHT)
                return;
            else if(nNewCol == 0 && eDirection == SC_MOVE_LEFT)
                return;

            SCCOL nNextCol = FindNextVisibleCol( nNewCol, bRight );

            if(aCol[nNextCol].HasVisibleDataAt(rRow))
            {
                bool bFound = false;
                nNewCol = nNextCol;
                do
                {
                    nNextCol = FindNextVisibleCol( nNewCol, bRight );
                    if(aCol[nNextCol].HasVisibleDataAt(rRow))
                        nNewCol = nNextCol;
                    else
                        bFound = true;
                }
                while(!bFound && nNextCol > 0 && nNextCol < MAXCOL);
            }
            else
            {
                nNewCol = FindNextVisibleColWithContent(nNewCol, bRight, rRow);
            }
        }
        else
        {
            nNewCol = FindNextVisibleColWithContent(nNewCol, bRight, rRow);
        }

        if (nNewCol<0)
            nNewCol=0;
        if (nNewCol>MAXCOL)
            nNewCol=MAXCOL;
        rCol = nNewCol;
    }
    else
    {
        aCol[rCol].FindDataAreaPos(rRow,eDirection == SC_MOVE_DOWN);
    }
}

bool ScTable::ValidNextPos( SCCOL nCol, SCROW nRow, const ScMarkData& rMark,
                                bool bMarked, bool bUnprotected ) const
{
    if (!ValidCol(nCol) || !ValidRow(nRow))
        return false;

    if (pDocument->HasAttrib(nCol, nRow, nTab, nCol, nRow, nTab, HasAttrFlags::Overlapped))
        // Skip an overlapped cell.
        return false;

    if (bMarked && !rMark.IsCellMarked(nCol,nRow))
        return false;

    if (bUnprotected && static_cast<const ScProtectionAttr*>(
                        GetAttr(nCol,nRow,ATTR_PROTECTION))->GetProtection())
        return false;

    if (bMarked || bUnprotected)        //TODO: also in other case ???
    {
        // Hidden cells must be skipped, as the cursor would end up on the next cell
        // even if it is protected or not marked.
        //TODO: control per Extra-Parameter, only for Cursor movement ???

        if (RowHidden(nRow))
            return false;

        if (ColHidden(nCol))
            return false;
    }

    return true;
}

void ScTable::GetNextPos( SCCOL& rCol, SCROW& rRow, SCCOL nMovX, SCROW nMovY,
                                bool bMarked, bool bUnprotected, const ScMarkData& rMark ) const
{
    if (bUnprotected && !IsProtected())     // Is sheet really protected?
        bUnprotected = false;

    sal_uInt16 nWrap = 0;
    SCCOL nCol = rCol;
    SCROW nRow = rRow;

    nCol = sal::static_int_cast<SCCOL>( nCol + nMovX );
    nRow = sal::static_int_cast<SCROW>( nRow + nMovY );

    OSL_ENSURE( !nMovY || !bUnprotected,
                "GetNextPos with bUnprotected horizontal not implemented" );

    if ( nMovY && bMarked )
    {
        bool bUp = ( nMovY < 0 );
        nRow = rMark.GetNextMarked( nCol, nRow, bUp );
        while ( ValidRow(nRow) &&
                (RowHidden(nRow) || pDocument->HasAttrib(nCol, nRow, nTab, nCol, nRow, nTab, HasAttrFlags::Overlapped)) )
        {
            //  skip hidden rows (see above)
            nRow += nMovY;
            nRow = rMark.GetNextMarked( nCol, nRow, bUp );
        }

        while ( nRow < 0 || nRow > MAXROW )
        {
            nCol = sal::static_int_cast<SCCOL>( nCol + static_cast<SCCOL>(nMovY) );
            while ( ValidCol(nCol) && ColHidden(nCol) )
                nCol = sal::static_int_cast<SCCOL>( nCol + static_cast<SCCOL>(nMovY) );   //  skip hidden rows (see above)
            if (nCol < 0)
            {
                nCol = MAXCOL;
                if (++nWrap >= 2)
                    return;
            }
            else if (nCol > MAXCOL)
            {
                nCol = 0;
                if (++nWrap >= 2)
                    return;
            }
            if (nRow < 0)
                nRow = MAXROW;
            else if (nRow > MAXROW)
                nRow = 0;
            nRow = rMark.GetNextMarked( nCol, nRow, bUp );
            while ( ValidRow(nRow) &&
                    (RowHidden(nRow) || pDocument->HasAttrib(nCol, nRow, nTab, nCol, nRow, nTab, HasAttrFlags::Overlapped)) )
            {
                //  skip hidden rows (see above)
                nRow += nMovY;
                nRow = rMark.GetNextMarked( nCol, nRow, bUp );
            }
        }
    }

    if ( nMovX && ( bMarked || bUnprotected ) )
    {
        // wrap initial skip counting:
        if (nCol<0)
        {
            nCol = MAXCOL;
            --nRow;
            if (nRow<0)
                nRow = MAXROW;
        }
        if (nCol>MAXCOL)
        {
            nCol = 0;
            ++nRow;
            if (nRow>MAXROW)
                nRow = 0;
        }

        if ( !ValidNextPos(nCol, nRow, rMark, bMarked, bUnprotected) )
        {
            std::unique_ptr<SCROW[]> pNextRows(new SCROW[MAXCOL+1]);
            SCCOL i;

            if ( nMovX > 0 )                            //  forward
            {
                for (i=0; i<=MAXCOL; i++)
                    pNextRows[i] = (i<nCol) ? (nRow+1) : nRow;
                do
                {
                    SCROW nNextRow = pNextRows[nCol] + 1;
                    if ( bMarked )
                        nNextRow = rMark.GetNextMarked( nCol, nNextRow, false );
                    if ( bUnprotected )
                        nNextRow = aCol[nCol].GetNextUnprotected( nNextRow, false );
                    pNextRows[nCol] = nNextRow;

                    SCROW nMinRow = MAXROW+1;
                    for (i=0; i<=MAXCOL; i++)
                        if (pNextRows[i] < nMinRow)     // when two equal on the left
                        {
                            nMinRow = pNextRows[i];
                            nCol = i;
                        }
                    nRow = nMinRow;

                    if ( nRow > MAXROW )
                    {
                        if (++nWrap >= 2) break;        // handle invalid value
                        nCol = 0;
                        nRow = 0;
                        for (i=0; i<=MAXCOL; i++)
                            pNextRows[i] = 0;           // do it all over again
                    }
                }
                while ( !ValidNextPos(nCol, nRow, rMark, bMarked, bUnprotected) );
            }
            else                                        //  backwards
            {
                for (i=0; i<=MAXCOL; i++)
                    pNextRows[i] = (i>nCol) ? (nRow-1) : nRow;
                do
                {
                    SCROW nNextRow = pNextRows[nCol] - 1;
                    if ( bMarked )
                        nNextRow = rMark.GetNextMarked( nCol, nNextRow, true );
                    if ( bUnprotected )
                        nNextRow = aCol[nCol].GetNextUnprotected( nNextRow, true );
                    pNextRows[nCol] = nNextRow;

                    SCROW nMaxRow = -1;
                    for (i=0; i<=MAXCOL; i++)
                        if (pNextRows[i] >= nMaxRow)    // when two equal on the right
                        {
                            nMaxRow = pNextRows[i];
                            nCol = i;
                        }
                    nRow = nMaxRow;

                    if ( nRow < 0 )
                    {
                        if (++nWrap >= 2) break;        // handle invalid value
                        nCol = MAXCOL;
                        nRow = MAXROW;
                        for (i=0; i<=MAXCOL; i++)
                            pNextRows[i] = MAXROW;      // do it all over again
                    }
                }
                while ( !ValidNextPos(nCol, nRow, rMark, bMarked, bUnprotected) );
            }
        }
    }

    //  Invalid values show up for instane for Tab, when nothing is selected and not
    //  protected (left / right edge), then leave values unchanged.

    if (ValidColRow(nCol,nRow))
    {
        rCol = nCol;
        rRow = nRow;
    }
}

bool ScTable::GetNextMarkedCell( SCCOL& rCol, SCROW& rRow, const ScMarkData& rMark ) const
{
    ++rRow;                 // next row

    while ( rCol <= MAXCOL )
    {
        ScMarkArray aArray( rMark.GetMarkArray( rCol ) );
        while ( rRow <= MAXROW )
        {
            SCROW nStart = aArray.GetNextMarked( rRow, false );
            if ( nStart <= MAXROW )
            {
                SCROW nEnd = aArray.GetMarkEnd( nStart, false );

                const sc::CellStoreType& rCells = aCol[rCol].maCells;
                std::pair<sc::CellStoreType::const_iterator,size_t> aPos = rCells.position(nStart);
                sc::CellStoreType::const_iterator it = aPos.first;
                SCROW nTestRow = nStart;
                if (it->type == sc::element_type_empty)
                {
                    // Skip the empty block.
                    nTestRow += it->size - aPos.second;
                    ++it;
                    if (it == rCells.end())
                    {
                        // No more block.  Move on to the next column.
                        rRow = MAXROW + 1;
                        continue;
                    }
                }

                if (nTestRow <= nEnd)
                {
                    // Cell found.
                    rRow = nTestRow;
                    return true;
                }

                rRow = nEnd + 1;                // Search for next selected range
            }
            else
                rRow = MAXROW + 1;              // End of column
        }
        rRow = 0;
        ++rCol;                                 // test next column
    }

    return false;                               // Through all columns
}

void ScTable::UpdateDrawRef( UpdateRefMode eUpdateRefMode, SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                                    SCCOL nCol2, SCROW nRow2, SCTAB nTab2,
                                    SCCOL nDx, SCROW nDy, SCTAB nDz, bool bUpdateNoteCaptionPos )
{
    if ( nTab >= nTab1 && nTab <= nTab2 && nDz == 0 )       // only within the table
    {
        ScDrawLayer* pDrawLayer = pDocument->GetDrawLayer();
        if ( eUpdateRefMode != URM_COPY && pDrawLayer )
        {
            if ( eUpdateRefMode == URM_MOVE )
            {                                               // source range
                nCol1 = sal::static_int_cast<SCCOL>( nCol1 - nDx );
                nRow1 = sal::static_int_cast<SCROW>( nRow1 - nDy );
                nCol2 = sal::static_int_cast<SCCOL>( nCol2 - nDx );
                nRow2 = sal::static_int_cast<SCROW>( nRow2 - nDy );
            }
            pDrawLayer->MoveArea( nTab, nCol1,nRow1, nCol2,nRow2, nDx,nDy,
                                    (eUpdateRefMode == URM_INSDEL), bUpdateNoteCaptionPos );
        }
    }
}

void ScTable::UpdateReference(
    sc::RefUpdateContext& rCxt, ScDocument* pUndoDoc, bool bIncludeDraw, bool bUpdateNoteCaptionPos )
{
    bool bUpdated = false;
    SCCOL i;
    SCCOL iMax;
    if (rCxt.meMode == URM_COPY )
    {
        i = rCxt.maRange.aStart.Col();
        iMax = rCxt.maRange.aEnd.Col();
    }
    else
    {
        i = 0;
        iMax = MAXCOL;
    }

    UpdateRefMode eUpdateRefMode = rCxt.meMode;
    SCCOL nDx = rCxt.mnColDelta;
    SCROW nDy = rCxt.mnRowDelta;
    SCTAB nDz = rCxt.mnTabDelta;
    SCCOL nCol1 = rCxt.maRange.aStart.Col(), nCol2 = rCxt.maRange.aEnd.Col();
    SCROW nRow1 = rCxt.maRange.aStart.Row(), nRow2 = rCxt.maRange.aEnd.Row();
    SCTAB nTab1 = rCxt.maRange.aStart.Tab(), nTab2 = rCxt.maRange.aEnd.Tab();

    // Named expressions need to be updated before formulas accessing them.
    if (mpRangeName)
        mpRangeName->UpdateReference(rCxt, nTab);

    for ( ; i<=iMax; i++)
        bUpdated |= aCol[i].UpdateReference(rCxt, pUndoDoc);

    if ( bIncludeDraw )
        UpdateDrawRef( eUpdateRefMode, nCol1, nRow1, nTab1, nCol2, nRow2, nTab2, nDx, nDy, nDz, bUpdateNoteCaptionPos );

    if ( nTab >= nTab1 && nTab <= nTab2 && nDz == 0 )       // print ranges: only within the table
    {
        SCTAB nSTab = nTab;
        SCTAB nETab = nTab;
        SCCOL nSCol = 0;
        SCROW nSRow = 0;
        SCCOL nECol = 0;
        SCROW nERow = 0;
        bool bRecalcPages = false;

        for ( ScRangeVec::iterator aIt = aPrintRanges.begin(), aEnd = aPrintRanges.end(); aIt != aEnd; ++aIt )
        {
            nSCol = aIt->aStart.Col();
            nSRow = aIt->aStart.Row();
            nECol = aIt->aEnd.Col();
            nERow = aIt->aEnd.Row();

            // do not try to modify sheet index of print range
            if ( ScRefUpdate::Update( pDocument, eUpdateRefMode,
                                      nCol1,nRow1,nTab, nCol2,nRow2,nTab,
                                      nDx,nDy,0,
                                      nSCol,nSRow,nSTab, nECol,nERow,nETab ) )
            {
                *aIt = ScRange( nSCol, nSRow, 0, nECol, nERow, 0 );
                bRecalcPages = true;
            }
        }

        if ( pRepeatColRange )
        {
            nSCol = pRepeatColRange->aStart.Col();
            nSRow = pRepeatColRange->aStart.Row();
            nECol = pRepeatColRange->aEnd.Col();
            nERow = pRepeatColRange->aEnd.Row();

            // do not try to modify sheet index of repeat range
            if ( ScRefUpdate::Update( pDocument, eUpdateRefMode,
                                      nCol1,nRow1,nTab, nCol2,nRow2,nTab,
                                      nDx,nDy,0,
                                      nSCol,nSRow,nSTab, nECol,nERow,nETab ) )
            {
                *pRepeatColRange = ScRange( nSCol, nSRow, 0, nECol, nERow, 0 );
                bRecalcPages = true;
                nRepeatStartX = nSCol;  // for UpdatePageBreaks
                nRepeatEndX = nECol;
            }
        }

        if ( pRepeatRowRange )
        {
            nSCol = pRepeatRowRange->aStart.Col();
            nSRow = pRepeatRowRange->aStart.Row();
            nECol = pRepeatRowRange->aEnd.Col();
            nERow = pRepeatRowRange->aEnd.Row();

            // do not try to modify sheet index of repeat range
            if ( ScRefUpdate::Update( pDocument, eUpdateRefMode,
                                      nCol1,nRow1,nTab, nCol2,nRow2,nTab,
                                      nDx,nDy,0,
                                      nSCol,nSRow,nSTab, nECol,nERow,nETab ) )
            {
                *pRepeatRowRange = ScRange( nSCol, nSRow, 0, nECol, nERow, 0 );
                bRecalcPages = true;
                nRepeatStartY = nSRow;  // for UpdatePageBreaks
                nRepeatEndY = nERow;
            }
        }

        //  updating print ranges is not necessary with multiple print ranges
        if ( bRecalcPages && GetPrintRangeCount() <= 1 )
        {
            UpdatePageBreaks(nullptr);

            pDocument->RepaintRange( ScRange(0,0,nTab,MAXCOL,MAXROW,nTab) );
        }
    }

    if (bUpdated && IsStreamValid())
        SetStreamValid(false);

    if(mpCondFormatList)
        mpCondFormatList->UpdateReference(rCxt);

    if (pTabProtection)
        pTabProtection->updateReference( eUpdateRefMode, pDocument, rCxt.maRange, nDx, nDy, nDz);
}

void ScTable::UpdateTranspose( const ScRange& rSource, const ScAddress& rDest,
                                    ScDocument* pUndoDoc )
{
    for ( SCCOL i=0; i<=MAXCOL; i++ )
        aCol[i].UpdateTranspose( rSource, rDest, pUndoDoc );
}

void ScTable::UpdateGrow( const ScRange& rArea, SCCOL nGrowX, SCROW nGrowY )
{
    for ( SCCOL i=0; i<=MAXCOL; i++ )
        aCol[i].UpdateGrow( rArea, nGrowX, nGrowY );
}

void ScTable::UpdateInsertTab( sc::RefUpdateInsertTabContext& rCxt )
{
    // Store the old tab number in sc::UpdatedRangeNames for
    // ScTokenArray::AdjustReferenceOnInsertedTab() to check with
    // isNameModified()
    if (mpRangeName)
        mpRangeName->UpdateInsertTab(rCxt, nTab);

    if (nTab >= rCxt.mnInsertPos)
    {
        nTab += rCxt.mnSheets;
        if (pDBDataNoName)
            pDBDataNoName->UpdateMoveTab(nTab - 1 ,nTab);
    }

    if (mpCondFormatList)
        mpCondFormatList->UpdateInsertTab(rCxt);

    if (pTabProtection)
        pTabProtection->updateReference( URM_INSDEL, pDocument,
                ScRange( 0, 0, rCxt.mnInsertPos, MAXCOL, MAXROW, MAXTAB),
                0, 0, rCxt.mnSheets);

    for (SCCOL i=0; i < aCol.size(); i++)
        aCol[i].UpdateInsertTab(rCxt);

    if (IsStreamValid())
        SetStreamValid(false);
}

void ScTable::UpdateDeleteTab( sc::RefUpdateDeleteTabContext& rCxt )
{
    // Store the old tab number in sc::UpdatedRangeNames for
    // ScTokenArray::AdjustReferenceOnDeletedTab() to check with
    // isNameModified()
    if (mpRangeName)
        mpRangeName->UpdateDeleteTab(rCxt, nTab);

    if (nTab > rCxt.mnDeletePos)
    {
        nTab -= rCxt.mnSheets;
        if (pDBDataNoName)
            pDBDataNoName->UpdateMoveTab(nTab + 1,nTab);
    }

    if (mpCondFormatList)
        mpCondFormatList->UpdateDeleteTab(rCxt);

    if (pTabProtection)
        pTabProtection->updateReference( URM_INSDEL, pDocument,
                ScRange( 0, 0, rCxt.mnDeletePos, MAXCOL, MAXROW, MAXTAB),
                0, 0, -rCxt.mnSheets);

    for (SCCOL i = 0; i < aCol.size(); ++i)
        aCol[i].UpdateDeleteTab(rCxt);

    if (IsStreamValid())
        SetStreamValid(false);
}

void ScTable::UpdateMoveTab(
    sc::RefUpdateMoveTabContext& rCxt, SCTAB nTabNo, ScProgress* pProgress )
{
    nTab = nTabNo;
    if (mpRangeName)
        mpRangeName->UpdateMoveTab(rCxt, nTab);

    if (pDBDataNoName)
        pDBDataNoName->UpdateMoveTab(rCxt.mnOldPos, rCxt.mnNewPos);

    if(mpCondFormatList)
        mpCondFormatList->UpdateMoveTab(rCxt);

    if (pTabProtection)
        pTabProtection->updateReference( URM_REORDER, pDocument,
                ScRange( 0, 0, rCxt.mnOldPos, MAXCOL, MAXROW, MAXTAB),
                0, 0, rCxt.mnNewPos - rCxt.mnOldPos);

    for ( SCCOL i=0; i < aCol.size(); i++ )
    {
        aCol[i].UpdateMoveTab(rCxt, nTabNo);
        if (pProgress)
            pProgress->SetState(pProgress->GetState() + aCol[i].GetCodeCount());
    }

    if (IsStreamValid())
        SetStreamValid(false);
}

void ScTable::UpdateCompile( bool bForceIfNameInUse )
{
    for (SCCOL i=0; i < aCol.size(); i++)
    {
        aCol[i].UpdateCompile( bForceIfNameInUse );
    }
}

void ScTable::SetTabNo(SCTAB nNewTab)
{
    nTab = nNewTab;
    for (SCCOL i=0; i < aCol.size(); i++)
        aCol[i].SetTabNo(nNewTab);
}

void ScTable::FindRangeNamesInUse(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                               sc::UpdatedRangeNames& rIndexes) const
{
    for (SCCOL i = nCol1; i <= nCol2 && IsColValid( i ); i++)
        aCol[i].FindRangeNamesInUse(nRow1, nRow2, rIndexes);
}

void ScTable::ExtendPrintArea( OutputDevice* pDev,
                    SCCOL /* nStartCol */, SCROW nStartRow, SCCOL& rEndCol, SCROW nEndRow )
{
    if ( !pColFlags || !pRowFlags )
    {
        OSL_FAIL("ExtendPrintArea: No ColInfo or RowInfo");
        return;
    }

    Point aPix1000 = pDev->LogicToPixel( Point(1000,1000), MapUnit::MapTwip );
    double nPPTX = aPix1000.X() / 1000.0;
    double nPPTY = aPix1000.Y() / 1000.0;

    // First, mark those columns that we need to skip i.e. hidden and empty columns.

    ScFlatBoolColSegments aSkipCols;
    aSkipCols.setFalse(0, MAXCOL);
    for (SCCOL i = 0; i <= MAXCOL; ++i)
    {
        SCCOL nLastCol = i;
        if (ColHidden(i, nullptr, &nLastCol))
        {
            // Columns are hidden in this range.
            aSkipCols.setTrue(i, nLastCol);
        }
        else
        {
            // These columns are visible.  Check for empty columns.
            for (SCCOL j = i; j <= nLastCol; ++j)
            {
                if ( j >= aCol.size() )
                {
                    aSkipCols.setTrue( j, MAXCOL );
                    break;
                }
                if (aCol[j].GetCellCount() == 0)
                    // empty
                    aSkipCols.setTrue(j,j);
            }
        }
        i = nLastCol;
    }

    ScFlatBoolColSegments::RangeData aColData;
    for (SCCOL nCol = rEndCol; nCol >= 0; --nCol)
    {
        if (!aSkipCols.getRangeData(nCol, aColData))
            // Failed to get the data.  This should never happen!
            return;

        if (aColData.mbValue)
        {
            // Skip these columns.
            nCol = aColData.mnCol1; // move toward 0.
            continue;
        }

        // These are visible and non-empty columns.
        for (SCCOL nDataCol = nCol; 0 <= nDataCol && nDataCol >= aColData.mnCol1; --nDataCol)
        {
            SCCOL nPrintCol = nDataCol;
            VisibleDataCellIterator aIter(*mpHiddenRows, aCol[nDataCol]);
            ScRefCellValue aCell = aIter.reset(nStartRow);
            if (aCell.isEmpty())
                // No visible cells found in this column.  Skip it.
                continue;

            while (!aCell.isEmpty())
            {
                SCCOL nNewCol = nDataCol;
                SCROW nRow = aIter.getRow();
                if (nRow > nEndRow)
                    // Went past the last row position.  Bail out.
                    break;

                MaybeAddExtraColumn(nNewCol, nRow, pDev, nPPTX, nPPTY);
                if (nNewCol > nPrintCol)
                    nPrintCol = nNewCol;
                aCell = aIter.next();
            }

            if (nPrintCol > rEndCol)
                // Make sure we don't shrink the print area.
                rEndCol = nPrintCol;
        }
        nCol = aColData.mnCol1; // move toward 0.
    }
}

void ScTable::MaybeAddExtraColumn(SCCOL& rCol, SCROW nRow, OutputDevice* pDev, double nPPTX, double nPPTY)
{
    ScRefCellValue aCell = aCol[rCol].GetCellValue(nRow);
    if (!aCell.hasString())
        return;

    long nPixel = aCol[rCol].GetTextWidth(nRow);

    // Width already calculated in Idle-Handler ?
    if ( TEXTWIDTH_DIRTY == nPixel )
    {
        ScNeededSizeOptions aOptions;
        aOptions.bTotalSize  = true;
        aOptions.bFormula    = false; //TODO: pass as parameter
        aOptions.bSkipMerged = false;

        Fraction aZoom(1,1);
        nPixel = aCol[rCol].GetNeededSize(
            nRow, pDev, nPPTX, nPPTY, aZoom, aZoom, true, aOptions, nullptr );

        aCol[rCol].SetTextWidth(nRow, static_cast<sal_uInt16>(nPixel));
    }

    long nTwips = (long) (nPixel / nPPTX);
    long nDocW = GetColWidth( rCol );

    long nMissing = nTwips - nDocW;
    if ( nMissing > 0 )
    {
        //  look at alignment

        const ScPatternAttr* pPattern = GetPattern( rCol, nRow );
        const SfxItemSet* pCondSet = pDocument->GetCondResult( rCol, nRow, nTab );

        SvxCellHorJustify eHorJust = (SvxCellHorJustify)static_cast<const SvxHorJustifyItem&>(
                        pPattern->GetItem( ATTR_HOR_JUSTIFY, pCondSet )).GetValue();
        if ( eHorJust == SvxCellHorJustify::Center )
            nMissing /= 2;                          // distributed into both directions
        else
        {
            // STANDARD is LEFT (only text is handled here)
            bool bRight = ( eHorJust == SvxCellHorJustify::Right );
            if ( IsLayoutRTL() )
                bRight = !bRight;
            if ( bRight )
                nMissing = 0;       // extended only to the left (logical)
        }
    }

    SCCOL nNewCol = rCol;
    while (nMissing > 0 && nNewCol < MAXCOL)
    {
        ScRefCellValue aNextCell = aCol[nNewCol+1].GetCellValue(nRow);
        if (!aNextCell.isEmpty())
            // Cell content in a next column ends display of this string.
            nMissing = 0;
        else
            nMissing -= GetColWidth(++nNewCol);
    }
    rCol = nNewCol;
}

namespace {

class SetTableIndex : public ::std::unary_function<ScRange, void>
{
    SCTAB mnTab;
public:
    explicit SetTableIndex(SCTAB nTab) : mnTab(nTab) {}

    void operator() (ScRange& rRange) const
    {
        rRange.aStart.SetTab(mnTab);
        rRange.aEnd.SetTab(mnTab);
    }
};

void setPrintRange(ScRange*& pRange1, const ScRange* pRange2)
{
    if (pRange2)
    {
        if (pRange1)
            *pRange1 = *pRange2;
        else
            pRange1 = new ScRange(*pRange2);
    }
    else
        DELETEZ(pRange1);
}

}

void ScTable::CopyPrintRange(const ScTable& rTable)
{
    // The table index shouldn't be used when the print range is used, but
    // just in case set the correct table index.

    aPrintRanges = rTable.aPrintRanges;
    ::std::for_each(aPrintRanges.begin(), aPrintRanges.end(), SetTableIndex(nTab));

    bPrintEntireSheet = rTable.bPrintEntireSheet;

    delete pRepeatColRange;
    pRepeatColRange = nullptr;
    if (rTable.pRepeatColRange)
    {
        pRepeatColRange = new ScRange(*rTable.pRepeatColRange);
        pRepeatColRange->aStart.SetTab(nTab);
        pRepeatColRange->aEnd.SetTab(nTab);
    }

    delete pRepeatRowRange;
    pRepeatRowRange = nullptr;
    if (rTable.pRepeatRowRange)
    {
        pRepeatRowRange = new ScRange(*rTable.pRepeatRowRange);
        pRepeatRowRange->aStart.SetTab(nTab);
        pRepeatRowRange->aEnd.SetTab(nTab);
    }
}

void ScTable::SetRepeatColRange( const ScRange* pNew )
{
    setPrintRange( pRepeatColRange, pNew );

    if (IsStreamValid())
        SetStreamValid(false);

    InvalidatePageBreaks();
}

void ScTable::SetRepeatRowRange( const ScRange* pNew )
{
    setPrintRange( pRepeatRowRange, pNew );

    if (IsStreamValid())
        SetStreamValid(false);

    InvalidatePageBreaks();
}

void ScTable::ClearPrintRanges()
{
    aPrintRanges.clear();
    bPrintEntireSheet = false;

    if (IsStreamValid())
        SetStreamValid(false);

    InvalidatePageBreaks();     // #i117952# forget page breaks for an old print range
}

void ScTable::AddPrintRange( const ScRange& rNew )
{
    bPrintEntireSheet = false;
    if( aPrintRanges.size() < 0xFFFF )
        aPrintRanges.push_back( rNew );

    if (IsStreamValid())
        SetStreamValid(false);

    InvalidatePageBreaks();
}

void ScTable::SetPrintEntireSheet()
{
    if( !IsPrintEntireSheet() )
    {
        ClearPrintRanges();
        bPrintEntireSheet = true;
    }
}

const ScRange* ScTable::GetPrintRange(sal_uInt16 nPos) const
{
    return (nPos < GetPrintRangeCount()) ? &aPrintRanges[ nPos ] : nullptr;
}

void ScTable::FillPrintSaver( ScPrintSaverTab& rSaveTab ) const
{
    rSaveTab.SetAreas( aPrintRanges, bPrintEntireSheet );
    rSaveTab.SetRepeat( pRepeatColRange, pRepeatRowRange );
}

void ScTable::RestorePrintRanges( const ScPrintSaverTab& rSaveTab )
{
    aPrintRanges = rSaveTab.GetPrintRanges();
    bPrintEntireSheet = rSaveTab.IsEntireSheet();
    SetRepeatColRange( rSaveTab.GetRepeatCol() );
    SetRepeatRowRange( rSaveTab.GetRepeatRow() );

    InvalidatePageBreaks();     // #i117952# forget page breaks for an old print range
    UpdatePageBreaks(nullptr);
}

SCROW ScTable::VisibleDataCellIterator::ROW_NOT_FOUND = -1;

ScTable::VisibleDataCellIterator::VisibleDataCellIterator(ScFlatBoolRowSegments& rRowSegs, ScColumn& rColumn) :
    mrRowSegs(rRowSegs),
    mrColumn(rColumn),
    mnCurRow(ROW_NOT_FOUND),
    mnUBound(ROW_NOT_FOUND)
{
}

ScTable::VisibleDataCellIterator::~VisibleDataCellIterator()
{
}

ScRefCellValue ScTable::VisibleDataCellIterator::reset(SCROW nRow)
{
    if (nRow > MAXROW)
    {
        mnCurRow = ROW_NOT_FOUND;
        return ScRefCellValue();
    }

    ScFlatBoolRowSegments::RangeData aData;
    if (!mrRowSegs.getRangeData(nRow, aData))
    {
        mnCurRow = ROW_NOT_FOUND;
        return ScRefCellValue();
    }

    if (!aData.mbValue)
    {
        // specified row is visible.  Take it.
        mnCurRow = nRow;
        mnUBound = aData.mnRow2;
    }
    else
    {
        // specified row is not-visible.  The first visible row is the start of
        // the next segment.
        mnCurRow = aData.mnRow2 + 1;
        mnUBound = mnCurRow; // get range data on the next iteration.
        if (mnCurRow > MAXROW)
        {
            // Make sure the row doesn't exceed our current limit.
            mnCurRow = ROW_NOT_FOUND;
            return ScRefCellValue();
        }
    }

    maCell = mrColumn.GetCellValue(mnCurRow);
    if (!maCell.isEmpty())
        // First visible cell found.
        return maCell;

    // Find a first visible cell below this row (if any).
    return next();
}

ScRefCellValue ScTable::VisibleDataCellIterator::next()
{
    if (mnCurRow == ROW_NOT_FOUND)
        return ScRefCellValue();

    while (mrColumn.GetNextDataPos(mnCurRow))
    {
        if (mnCurRow > mnUBound)
        {
            // We don't know the visibility of this row range.  Query it.
            ScFlatBoolRowSegments::RangeData aData;
            if (!mrRowSegs.getRangeData(mnCurRow, aData))
            {
                mnCurRow = ROW_NOT_FOUND;
                return ScRefCellValue();
            }

            if (aData.mbValue)
            {
                // This row is invisible.  Skip to the last invisible row and
                // try again.
                mnCurRow = mnUBound = aData.mnRow2;
                continue;
            }

            // This row is visible.
            mnUBound = aData.mnRow2;
        }

        maCell = mrColumn.GetCellValue(mnCurRow);
        if (!maCell.isEmpty())
            return maCell;
    }

    mnCurRow = ROW_NOT_FOUND;
    return ScRefCellValue();
}

void ScTable::SetAnonymousDBData(ScDBData* pDBData)
{
    delete pDBDataNoName;
    pDBDataNoName = pDBData;
}

sal_uLong ScTable::AddCondFormat( ScConditionalFormat* pNew )
{
    if(!mpCondFormatList)
        mpCondFormatList.reset(new ScConditionalFormatList());

    sal_uInt32 nMax = mpCondFormatList->getMaxKey();

    pNew->SetKey(nMax+1);
    mpCondFormatList->InsertNew(pNew);

    return nMax + 1;
}

SvtScriptType ScTable::GetScriptType( SCCOL nCol, SCROW nRow ) const
{
    if ( !IsColValid( nCol ) )
        return SvtScriptType::NONE;

    return aCol[nCol].GetScriptType(nRow);
}

void ScTable::SetScriptType( SCCOL nCol, SCROW nRow, SvtScriptType nType )
{
    if (!ValidCol(nCol))
        return;

    aCol[nCol].SetScriptType(nRow, nType);
}

SvtScriptType ScTable::GetRangeScriptType(
    sc::ColumnBlockPosition& rBlockPos, SCCOL nCol, SCROW nRow1, SCROW nRow2 )
{
    if ( !IsColValid( nCol ) )
        return SvtScriptType::NONE;

    sc::CellStoreType::iterator itr = aCol[nCol].maCells.begin();
    return aCol[nCol].GetRangeScriptType(rBlockPos.miCellTextAttrPos, nRow1, nRow2, itr);
}

size_t ScTable::GetFormulaHash( SCCOL nCol, SCROW nRow ) const
{
    if ( !IsColValid( nCol ) )
        return 0;

    return aCol[nCol].GetFormulaHash(nRow);
}

ScFormulaVectorState ScTable::GetFormulaVectorState( SCCOL nCol, SCROW nRow ) const
{
    if ( !IsColValid( nCol ) )
        return FormulaVectorUnknown;

    return aCol[nCol].GetFormulaVectorState(nRow);
}

formula::FormulaTokenRef ScTable::ResolveStaticReference( SCCOL nCol, SCROW nRow )
{
    if ( !ValidCol( nCol ) || !ValidRow( nRow ) )
        return formula::FormulaTokenRef();
    if ( nCol >= aCol.size() )
        // Return a value of 0.0 if column not exists
        return formula::FormulaTokenRef(new formula::FormulaDoubleToken(0.0));
    return aCol[nCol].ResolveStaticReference(nRow);
}

formula::FormulaTokenRef ScTable::ResolveStaticReference( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 )
{
    if (nCol2 < nCol1 || nRow2 < nRow1)
        return formula::FormulaTokenRef();

    if ( !ValidCol( nCol1 ) || !ValidCol( nCol2 ) || !ValidRow( nRow1 ) || !ValidRow( nRow2 ) )
        return formula::FormulaTokenRef();

    SCCOL nMaxCol;
    if ( nCol2 >= aCol.size() )
        nMaxCol = aCol.size() - 1;
    else
        nMaxCol = nCol2;

    ScMatrixRef pMat(new ScFullMatrix(nCol2-nCol1+1, nRow2-nRow1+1, 0.0));
    for (SCCOL nCol = nCol1; nCol <= nMaxCol; ++nCol)
    {
        if (!aCol[nCol].ResolveStaticReference(*pMat, nCol2-nCol1, nRow1, nRow2))
            // Column contains non-static cell. Failed.
            return formula::FormulaTokenRef();
    }

    return formula::FormulaTokenRef(new ScMatrixToken(pMat));
}

formula::VectorRefArray ScTable::FetchVectorRefArray( SCCOL nCol, SCROW nRow1, SCROW nRow2 )
{
    if (nRow2 < nRow1)
        return formula::VectorRefArray();

    if ( !IsColValid( nCol ) || !ValidRow( nRow1 ) || !ValidRow( nRow2 ) )
        return formula::VectorRefArray();

    return aCol[nCol].FetchVectorRefArray(nRow1, nRow2);
}

ScRefCellValue ScTable::GetRefCellValue( SCCOL nCol, SCROW nRow )
{
    if ( !IsColRowValid( nCol, nRow ) )
        return ScRefCellValue();

    return aCol[nCol].GetCellValue(nRow);
}

SvtBroadcaster* ScTable::GetBroadcaster( SCCOL nCol, SCROW nRow )
{
    if ( !IsColRowValid( nCol, nRow ) )
        return nullptr;

    return aCol[nCol].GetBroadcaster(nRow);
}

void ScTable::DeleteBroadcasters(
    sc::ColumnBlockPosition& rBlockPos, SCCOL nCol, SCROW nRow1, SCROW nRow2 )
{
    if ( !IsColValid( nCol ) )
        return;

    aCol[nCol].DeleteBroadcasters(rBlockPos, nRow1, nRow2);
}

void ScTable::FillMatrix( ScMatrix& rMat, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, svl::SharedStringPool* pPool ) const
{
    size_t nMatCol = 0;
    for (SCCOL nCol = nCol1; nCol <= nCol2; ++nCol, ++nMatCol)
        aCol[nCol].FillMatrix(rMat, nMatCol, nRow1, nRow2, pPool);
}

void ScTable::InterpretDirtyCells( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 )
{
    for (SCCOL nCol = nCol1; nCol <= nCol2; ++nCol)
        aCol[nCol].InterpretDirtyCells(nRow1, nRow2);
}

void ScTable::SetFormulaResults( SCCOL nCol, SCROW nRow, const double* pResults, size_t nLen )
{
    if (!ValidCol(nCol))
        return;

    aCol[nCol].SetFormulaResults(nRow, pResults, nLen);
}

void ScTable::SetFormulaResults(
    SCCOL nCol, SCROW nRow, const formula::FormulaConstTokenRef* pResults, size_t nLen )
{
    if (!ValidCol(nCol))
        return;

    aCol[nCol].SetFormulaResults(nRow, pResults, nLen);
}

#if DUMP_COLUMN_STORAGE
void ScTable::DumpColumnStorage( SCCOL nCol ) const
{
    if ( !IsColValid( nCol ) )
        return;

    aCol[nCol].DumpColumnStorage();
}
#endif

const SvtBroadcaster* ScTable::GetBroadcaster( SCCOL nCol, SCROW nRow ) const
{
    if ( !IsColRowValid( nCol, nRow ) )
        return nullptr;

    return aCol[nCol].GetBroadcaster(nRow);
}

void ScTable::DeleteConditionalFormat( sal_uLong nIndex )
{
    mpCondFormatList->erase(nIndex);
}

void ScTable::SetCondFormList( ScConditionalFormatList* pNew )
{
    mpCondFormatList.reset( pNew );
}

ScConditionalFormatList* ScTable::GetCondFormList()
{
    if(!mpCondFormatList)
        mpCondFormatList.reset( new ScConditionalFormatList() );

    return mpCondFormatList.get();
}

const ScConditionalFormatList* ScTable::GetCondFormList() const
{
    return mpCondFormatList.get();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
