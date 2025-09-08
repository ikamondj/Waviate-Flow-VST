/*
  ==============================================================================

    RestAPIHandler.h
    Created: 7 Sep 2025 12:10:48pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <string>
#include <map>

class RestApiHandler {
public:
    static std::string get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    static std::string post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {});
};