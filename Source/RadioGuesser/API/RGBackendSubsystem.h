// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGBackendSubsystem.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnHttpResponse, bool, bSuccess, const FString&, ResponseBody);

/**
 * URGBackendSubsystem
 *
 * Wraps all HTTP communication with the ASP.NET Core backend.
 * All requests are asynchronous. Never blocks the game thread.
 *
 * URL base is read from configuration — never hardcoded.
 */
UCLASS()
class RADIOGUESSER_API URGBackendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Configuration ─────────────────────────────────────────────────────────

    /** Set from DefaultGame.ini / config; e.g. https://api.radioguesser.com */
    UFUNCTION(BlueprintCallable, Category = "Backend")
    void SetBaseUrl(const FString& Url);

    UFUNCTION(BlueprintPure, Category = "Backend")
    FString GetBaseUrl() const { return BaseUrl; }

    // ── Auth helpers ──────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void SetAuthToken(const FString& Token);

    // ── Generic request ───────────────────────────────────────────────────────

    /** Fire a GET request; callback receives (bSuccess, ResponseBody) */
    void GetAsync(const FString& Endpoint, FOnHttpResponse Callback);

    /** Fire a POST request with JSON body */
    void PostAsync(const FString& Endpoint, const FString& JsonBody, FOnHttpResponse Callback);

private:
    FString BaseUrl;
    FString AuthToken;

    void SendRequest(const FString& Verb,
                     const FString& Endpoint,
                     const FString& Body,
                     FOnHttpResponse Callback);
};
