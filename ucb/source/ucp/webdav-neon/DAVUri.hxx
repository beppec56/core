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

#include <rtl/ustring.hxx>
#include <tools/urlobj.hxx>

class DAVUri : public INetURLObject
{

public:
    DAVUri();
    explicit DAVUri( const OUString& TheAbsRef );
    virtual ~DAVUri();

    inline OUString GetPathQueryFragment( DecodeMechanism eMechanism = INetURLObject::NO_DECODE );
};

inline OUString DAVUri::GetPathQueryFragment( DecodeMechanism eMechanism )
{
    OUString PathQueryFragment( GetURLPath( eMechanism ) );
    if ( HasParam() )
    {
        PathQueryFragment += "?";
        PathQueryFragment += GetParam( eMechanism );
    }

    if ( HasMark() )
    {
        PathQueryFragment += "#";
        PathQueryFragment += GetMark( eMechanism );
    }
    return PathQueryFragment;
};

#endif // INCLUDED_UCB_SOURCE_UCP_WEBDAV_NEON_DAVURI_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
