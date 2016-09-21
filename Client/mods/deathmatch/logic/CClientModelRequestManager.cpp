/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.0
*               (Shared logic for modifications)
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        mods/shared_logic/CClientModelRequestManager.cpp
*  PURPOSE:     Entity model streaming manager class
*  DEVELOPERS:  Christian Myhre Lundheim <>
*               Jax <>
*
*****************************************************************************/

#include "StdInc.h"

CClientModelRequestManager::CClientModelRequestManager ( void )
{
    m_bDoingPulse = false;
}


CClientModelRequestManager::~CClientModelRequestManager ( void )
{
    // Delete all our requests.
    for ( auto& pRequest : m_Requests )
    {
        delete pRequest;
    }

    m_Requests.clear ();
}


bool CClientModelRequestManager::IsLoaded ( unsigned short usModelID )
{
    // Grab the model info
    CModelInfo* pInfo = g_pGame->GetModelInfo ( usModelID );
    if ( pInfo )
        return pInfo->IsLoaded ();

    return false;
}


bool CClientModelRequestManager::IsRequested ( CModelInfo* pModelInfo )
{
    // Look through the list
    for ( auto pRequest : m_Requests )
    {
        // Same model as this entry?
        if ( pRequest->pModel == pModelInfo )
        {
            return true;
        }
    }

    // Not in request list
    return false;
}


bool CClientModelRequestManager::HasRequested ( CClientEntity* pRequester )
{
    assert ( pRequester );

    // Look through the list
    for (auto pRequest : m_Requests)
    {
        // Same requester as we check for? He has requested something.
        if ( pRequest->pEntity == pRequester )
        {
            return true;
        }
    }

    // Not requested anything
    return false;
}



CModelInfo* CClientModelRequestManager::GetRequestedModelInfo ( CClientEntity* pRequester )
{
    assert ( pRequester );

    // Look through the list
    for (auto pRequest : m_Requests)
    {
        // Same requester as we check for? He has requested something.
        if ( pRequest->pEntity == pRequester )
        {
            // Return the model info he requested
            return pRequest->pModel;
        }
    }

    // Not requested anything
    return NULL;
}


bool CClientModelRequestManager::RequestBlocking ( unsigned short usModelID, const char* szTag )
{
    // Grab the model info
    CModelInfo* pInfo = g_pGame->GetModelInfo ( usModelID );
    if ( pInfo )
    {
        pInfo->Request ( BLOCKING, szTag );
        if ( pInfo->IsLoaded () )
        {
            pInfo->MakeCustomModel ();
            return true;
        }
        OutputDebugLine ( SString ( "[Models] RequestBlocking failed for id %d", usModelID ) );
    }

    // Bad model ID probably.
    return false;
}


bool CClientModelRequestManager::Request ( unsigned short usModelID, CClientEntity* pRequester )
{
    assert ( pRequester );
    SClientModelRequest* pEntry;

    // Grab the model info for that model
    CModelInfo* pInfo = g_pGame->GetModelInfo ( usModelID );
    if ( pInfo )
    {
        // Has it already requested something?
        std::list < SClientModelRequest* > ::iterator iter;
        if ( GetRequestEntry ( pRequester, iter ) )
        {
            // Get the entry
            pEntry = *iter;

            // The same model?
            if ( pInfo == pEntry->pModel )
            {
                // He has to wait more for it
                return false;
            }
            else
            {
                // Remove the reference to the old model
                pEntry->pModel->RemoveRef ();

                // Is it loaded?
                if ( pInfo->IsLoaded () )
                {
                    // Delete it, remove the it from the list and return true.
                    delete pEntry;
                    m_Requests.erase ( iter );

                    pInfo->MakeCustomModel ();
                    return true;
                }
                else
                {
                    // If not loaded. Replace the model we're going to load.
                    // Also remember that we requested it now.
                    pEntry->pModel = pInfo;
                    pEntry->requestTimer.Reset ();

                    // Start loading the new model.
                    pInfo->ModelAddRef ( NON_BLOCKING, "CClientModelRequestManager::Request" );

                    // He has to wait for it.
                    return false;
                }
            }
        }
        else
        {
            // Already loaded? Don't bother adding to the list.
            if ( pInfo->IsLoaded () )
            {
                pInfo->MakeCustomModel ();

                return true;
            }

            // Request it
            pInfo->ModelAddRef ( NON_BLOCKING, "CClientModelRequestManager::Request #2" );

            // Add him to the list over models we're waiting for.
            pEntry = new SClientModelRequest;
            pEntry->pModel = pInfo;
            pEntry->pEntity = pRequester;
            pEntry->requestTimer.SetMaxIncrement ( 500 );
            pEntry->requestTimer.Reset ();
            m_Requests.push_back ( pEntry );

            // Return false. Caller needs to wait.
            return false;
        }
    }

    // Error, model is bad. Caller should not do this.
    return false;
}


void CClientModelRequestManager::Cancel ( CClientEntity* pEntity, bool bAllowQueue )
{
    assert ( pEntity );
    // Check to ensure entity has not got its knickers in a twist
    if ( ListContains ( m_CancelQueue, pEntity ) ) return;

    // Are we inside a pulse? Add it to a list to delete after or we'll crash.
    // If not, cancel now.
    if ( m_bDoingPulse )
    {
        // Check queuing is allowed by the caller
        assert ( bAllowQueue );
        m_CancelQueue.push_back ( pEntity );
    }
    else
    {
        // Got any items?
        if ( !m_Requests.empty () )
        {
            // Anything requested by the given class?
            SClientModelRequest* pEntry;
            for ( auto iter = m_Requests.begin(); iter != m_Requests.end(); )
            {
                pEntry = *iter;

                // If the requesting entity matches the given entity, delete and NULL it
                if ( pEntry->pEntity == pEntity )
                {
                    // Unreference the reference we added to it.
                    pEntry->pModel->RemoveRef ();

                    // Delete the entry
                    delete pEntry;
                    
                    // Remove from the list
                    iter = m_Requests.erase ( iter );
                }
                else
                {
                    // Increment iterator otherwize
                    ++iter;
                }
            }
        }
    }
}


void CClientModelRequestManager::DoPulse ( void )
{
    // Any requests?
    if ( m_Requests.size () > 0 )
    {
        // We are now doing the pulse
        m_bDoingPulse = true;

        // Call the callback for those finished loading and remove them from the list
        SClientModelRequest* pEntry;
        for ( auto iter = m_Requests.begin (); iter != m_Requests.end (); )
        {
            pEntry = *iter;

            // Is it loaded?
            if ( pEntry->pModel->IsLoaded () )
            {
                // Make sure custom things are replaced
                pEntry->pModel->MakeCustomModel ();

                // Call the callback
                pEntry->pEntity->ModelRequestCallback ( pEntry->pModel );

                // Unreference us from the model (callback should've added a reference!)
                pEntry->pModel->RemoveRef ();

                // Delete the request entry. Remove from the list and continue from after it
                delete pEntry;
                iter = m_Requests.erase ( iter );
            }
            else
            {
                // Been more than 2 seconds since we requested it? Request it again.
                if ( pEntry->requestTimer.Get () > 2000 )
                {
                    // Request it again. Don't add reference, or we screw up the
                    // reference count.
                    if ( g_pGame->IsASyncLoadingEnabled () )
                        pEntry->pModel->Request ( NON_BLOCKING, "CClientModelRequestManager::DoPulse #1" );
                    else
                        pEntry->pModel->Request ( BLOCKING, "CClientModelRequestManager::DoPulse #2" );

                    // Remember now as the time we requested it.
                    pEntry->requestTimer.Reset ();
                }

                // Increment iterator
                ++iter;
            }
        }

        // No longer doing the pulse
        m_bDoingPulse = false;

        // Cancel what we've scheduled for cancel now if anything
        if ( m_CancelQueue.size () > 0 )
        {
            // Cancel every entity in our cancel list
            std::list < CClientEntity* > cancelQueueCopy = m_CancelQueue;
            m_CancelQueue.clear ();

            for ( auto pEntry : cancelQueueCopy )
            {
                Cancel ( pEntry, false );
            }

        }
    }
}


bool CClientModelRequestManager::GetRequestEntry ( CClientEntity* pRequester, std::list < SClientModelRequest* > ::iterator& iterOut )
{
    // Look through the list
    for ( auto iter = m_Requests.begin() ; iter != m_Requests.end (); iter++ )
    {
        // Same requester as we check for? He has requested something.
        if ( (*iter)->pEntity == pRequester )
        {
            // Pass out the iterator entry and return true
            iterOut = iter;
            return true;
        }
    }

    // Not requested anything
    return false;
}