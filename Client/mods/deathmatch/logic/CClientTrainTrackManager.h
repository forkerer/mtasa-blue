/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto
*               (Shared logic for modifications)
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        mods/deathmatch/logic/CClientTrainTrackManager.h
*  PURPOSE:     Train track manager header
*
*****************************************************************************/
#pragma once
class CClientTrainTrack;

class CClientTrainTrackManager
{
public:
    static CClientTrainTrack* Get(ElementID ID);
    CClientTrainTrack* Get(CTrainTrack* pTrainTrack);

private:
    std::unordered_map<CTrainTrack*, CClientTrainTrack*> m_Map;
};