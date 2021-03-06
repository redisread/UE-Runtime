// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpRequestImpl.h"
#include "Http.h"

FHttpRequestCompleteDelegate& FHttpRequestImpl::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FHttpRequestImpl::OnRequestProgress() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnRequestProgress()"));
	return RequestProgressDelegate;
}

FHttpRequestHeaderReceivedDelegate& FHttpRequestImpl::OnHeaderReceived()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnHeaderReceived()"));
	return HeaderReceivedDelegate;
}

FHttpRequestWillRetryDelegate& FHttpRequestImpl::OnRequestWillRetry()
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FHttpRequestImpl::OnRequestWillRetry()"));
	return OnRequestWillRetryDelegate;
}

void FHttpRequestImpl::BroadcastResponseHeadersReceived()
{
	if (OnHeaderReceived().IsBound())
	{
		const FHttpResponsePtr Response = GetResponse();
		if (Response.IsValid())
		{
			const FHttpRequestPtr ThisPtr(SharedThis(this));
			const TArray<FString> AllHeaders = Response->GetAllHeaders();
			for (const FString& Header : AllHeaders)
			{
				FString HeaderName;
				FString HeaderValue;
				if (Header.Split(TEXT(":"), &HeaderName, &HeaderValue))
				{
					HeaderValue.TrimStartInline();
					OnHeaderReceived().ExecuteIfBound(ThisPtr, HeaderName, HeaderValue);
				}
			}
		}
	}
}
