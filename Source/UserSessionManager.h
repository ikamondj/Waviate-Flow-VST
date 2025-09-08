/*
  ==============================================================================

    UserSessionManager.h
    Created: 7 Sep 2025 12:10:31pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <string>
#include "JWTManager.h"

class UserSessionManager {
public:
    UserSessionManager();
    bool loginWithOAuth(const std::string& provider, const std::string& oauthToken);
    bool isLoggedIn() const;
    std::string getUserId() const;
    void logout();

private:
    JWTManager jwtManager;
    std::string userId;
    std::string providerType;
};