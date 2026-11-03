#pragma once
// Dependency-free backend contract; no UI, phone credentials or vendor APIs.
namespace network_setup {
// Active native Onboarding may open its five-minute phone persistence scope.
// The standalone Wi-Fi controller opens its own scope only on explicit Start.
bool BeginOriginalOnboardingSession();
void EndOriginalOnboardingSession();
// Stock ownership alone is insufficient; actual Wi-Fi configuring state is required.
bool BeginStockWifiProvisioningSession();
void EndStockWifiProvisioningSession();
bool CredentialPersistenceAllowed();
void NotifyCredentialsSaved(bool success);
bool CredentialSaveSucceeded();
bool NetworkWasStarted();
}
