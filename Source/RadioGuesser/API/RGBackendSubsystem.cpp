// Copyright RadioGuesser. All Rights Reserved.

#include "API/RGBackendSubsystem.h"
#include "RadioGuesser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void URGBackendSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Read base URL from config: [RadioGuesser] BackendUrl=https://...
    GConfig->GetString(TEXT("RadioGuesser"), TEXT("BackendUrl"), BaseUrl, GGameIni);
    if (BaseUrl.IsEmpty())
    {
        BaseUrl = TEXT("http://localhost:5000");
        UE_LOG(LogRGAPI, Warning, TEXT("BackendUrl not set in config; defaulting to %s"), *BaseUrl);
    }
    UE_LOG(LogRGAPI, Log, TEXT("RGBackendSubsystem initialised — BaseUrl=%s"), *BaseUrl);
}

void URGBackendSubsystem::Deinitialize()
{
    UE_LOG(LogRGAPI, Log, TEXT("RGBackendSubsystem deinitialised"));
    Super::Deinitialize();
}

void URGBackendSubsystem::SetBaseUrl(const FString& Url)
{
    BaseUrl = Url;
}

void URGBackendSubsystem::SetAuthToken(const FString& Token)
{
    AuthToken = Token;
}

void URGBackendSubsystem::GetAsync(const FString& Endpoint, FOnHttpResponse Callback)
{
    SendRequest(TEXT("GET"), Endpoint, FString(), Callback);
}

void URGBackendSubsystem::PostAsync(const FString& Endpoint, const FString& JsonBody, FOnHttpResponse Callback)
{
    SendRequest(TEXT("POST"), Endpoint, JsonBody, Callback);
}

void URGBackendSubsystem::SendRequest(const FString& Verb,
                                       const FString& Endpoint,
                                       const FString& Body,
                                       FOnHttpResponse Callback)
{
    const FString Url = BaseUrl + Endpoint;
    UE_LOG(LogRGAPI, Verbose, TEXT("%s %s"), *Verb, *Url);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(Verb);
    Request->SetURL(Url);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"),       TEXT("application/json"));

    if (!AuthToken.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    }

    if (!Body.IsEmpty())
    {
        Request->SetContentAsString(Body);
    }

    Request->OnProcessRequestComplete().BindLambda(
        [Callback](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
        {
            const bool bSuccess = bConnected && Resp.IsValid() && Resp->GetResponseCode() < 400;
            const FString ResponseBody = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            if (Callback.IsBound())
            {
                Callback.Execute(bSuccess, ResponseBody);
            }
        });

    Request->ProcessRequest();
}
