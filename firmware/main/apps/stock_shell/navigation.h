#pragma once
namespace stock_shell {
void RequestSettings();
bool ConsumeSettingsRequest();
void SetSetupContext(bool active);
void SaveSetupReturnForReset();
bool ConsumeSetupReturn();
}
