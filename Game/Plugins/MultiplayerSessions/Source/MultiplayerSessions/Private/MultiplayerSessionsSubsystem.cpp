// Fill out your copyright notice in the Description page of Project Settings.

#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() : 
                                                                 StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
                                                                 CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
                                                                 FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
                                                                 JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
                                                                 DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))

{
    IOnlineSubsystem *OnlineSubsystem = IOnlineSubsystem::Get();
    if (OnlineSubsystem)
    {
        OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1,
                                             10.f,
                                             FColor::Blue,
                                             FString::Printf(TEXT("Online Subsystem Service Provider: %s"), *OnlineSubsystem->GetSubsystemName().ToString()));
        }
    }
}

void UMultiplayerSessionsSubsystem::StartGameSession()
{
}

void UMultiplayerSessionsSubsystem::CreateGameSession(FName MatchType, int32 NumOfConnections)
{
    if (!OnlineSessionInterface.IsValid())
    {
        return;
    }

    FNamedOnlineSession *PreviousSession = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
    if (PreviousSession != nullptr)
    {
        OnlineSessionInterface->DestroySession(NAME_GameSession);
    }

    OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
    TSharedPtr<FOnlineSessionSettings> OnlineSessionSettings = MakeShareable(new FOnlineSessionSettings());

    OnlineSessionSettings->bIsLANMatch = false;
    OnlineSessionSettings->NumPublicConnections = 4;
    OnlineSessionSettings->bAllowJoinInProgress = true;
    OnlineSessionSettings->bAllowJoinViaPresence = true;
    OnlineSessionSettings->bShouldAdvertise = true;
    OnlineSessionSettings->bUsesPresence = true;
    OnlineSessionSettings->bUseLobbiesIfAvailable = true;
    OnlineSessionSettings->Set(FName("MatchType"), FString("FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    ULocalPlayer *LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(),
                                          NAME_GameSession,
                                          *OnlineSessionSettings);
}

void UMultiplayerSessionsSubsystem::FindGameSession(int32 MaxSearchResults)
{
    if (!OnlineSessionInterface.IsValid())
    {
        return;
    }

    OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
    SessionSearch = MakeShareable(new FOnlineSessionSearch());

    SessionSearch->MaxSearchResults = 10000;
    SessionSearch->bIsLanQuery = false;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

    ULocalPlayer *LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),
                                         SessionSearch.ToSharedRef());
}

void UMultiplayerSessionsSubsystem::JoinGameSession(const FOnlineSessionSearchResult SearchResult)
{
    if (!OnlineSessionInterface.IsValid())
    {
        return;
    }
    
    OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

    ULocalPlayer *LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(),
                                        NAME_GameSession,
                                        SearchResult);
}

void UMultiplayerSessionsSubsystem::DestroyGameSession()
{
}

// Delegate Callback functions

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{

}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (!GEngine)
    {
        return;
    }

    if (bWasSuccessful)
    {
        GetWorld()->ServerTravel(FString("/Game/ThirdPerson/Maps/Lobby?listen"));
        GEngine->AddOnScreenDebugMessage(-1,
                                         10.f,
                                         FColor::Blue,
                                         FString::Printf(TEXT("Session Created Successfully and lobby is designated as listen server %s"), *SessionName.ToString()));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1,
                                         10.f,
                                         FColor::Red,
                                         FString::Printf(TEXT("Session Creation Failed %s"), *SessionName.ToString()));
    }
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    if (!GEngine)
    {
        return;
    }

    if (SessionSearch.IsValid())
    {
        GEngine->AddOnScreenDebugMessage(-1,
                                         3.f,
                                         FColor::Blue,
                                         FString::Printf(TEXT("IsValid")));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1,
                                         3.f,
                                         FColor::Red,
                                         FString::Printf(TEXT("IsNotValid")));
    }

    if (bWasSuccessful)
    {
        for (auto SearchResult : SessionSearch->SearchResults)
        {
            FString SessionId = SearchResult.GetSessionIdStr();
            FUniqueNetIdPtr OwnerId = SearchResult.Session.OwningUserId;
            FString OwnerName = SearchResult.Session.OwningUserName;

            FString MatchType;
            SearchResult.Session.SessionSettings.Get(FName("MatchType"), MatchType);
            if (MatchType == FString("FreeForAll"))
            {
                GEngine->AddOnScreenDebugMessage(-1,
                                                 30.f,
                                                 FColor::Blue,
                                                 FString::Printf(TEXT("Session Found: SId: %s :: OName: %s :: MatchType: %s"),
                                                                 *SessionId, *OwnerName, *MatchType));

                // Call the JoinSession
                JoinGameSession(SearchResult);
            }
        }
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1,
                                         3.f,
                                         FColor::Red,
                                         FString::Printf(TEXT("Session Not Found after search")));
    }
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type SessionResult)
{
    if (!OnlineSessionInterface.IsValid())
    {
        return;
    }

    FString Address;
    // gives you the platform specific connection information for joining the match
    bool isSuccess = OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address);
    if (isSuccess)
    {
        APlayerController *PlayerController = GetGameInstance()->GetFirstLocalPlayerController(GetWorld());
        if (PlayerController)
        {
            PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1,
                                                 10.f,
                                                 FColor::Yellow,
                                                 FString::Printf(TEXT("Joined Session: %s"), *Address));
            }
        }
    }
}